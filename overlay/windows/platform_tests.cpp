#include "RuntimeCompat.h"
#include "platform_windows.h"
#include "platform_android.h"
#include "NativeAudioOutput.h"
#include "WaveDecode.h"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <stdexcept>
namespace {
void require(bool condition,const char *message) { if(!condition) throw std::runtime_error(message); }
template<class Predicate> bool waitFor(Predicate predicate) {
    for(int i=0;i<200;++i) { if(predicate()) return true; std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
    return false;
}
std::atomic<unsigned> mixedFrames{0};
int32_t silence(void*,void *data,int32_t frames) { memset(data,0,size_t(frames)*4); mixedFrames+=unsigned(frames); return 0; }
}
int main() {
    bool configured=false;
    try {
        wchar_t executable[32768]{}; GetModuleFileNameW(nullptr,executable,32768);
        auto root=std::filesystem::path(executable).parent_path()/std::filesystem::path("platform-fixture-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        auto data=root/"data",user=root/"user";
        std::filesystem::create_directories(data/"models"); std::filesystem::create_directories(data/"data");
        require(!platform::configureWindows(data.u8string(),user.u8string()),"Unprepared game directory was accepted");
        std::ofstream(data/".libertyvr-pcvr-data")<<"synthetic fixture";
        require(platform::configureWindows(data.u8string(),user.u8string()),"Windows media services unavailable"); configured=true;
        require(std::filesystem::path(platform::storageRoot())==user,"User root is not isolated");
        const auto wave=root/"silence.wav";
        struct Header { char riff[4]; uint32_t size; char wave[4],fmt[4]; uint32_t formatSize; uint16_t format,channels; uint32_t rate,bytesPerSecond; uint16_t alignment,bits; char data[4]; uint32_t dataSize; };
        static_assert(sizeof(Header)==44);
        Header header{{'R','I','F','F'},192036,{'W','A','V','E'},{'f','m','t',' '},16,1,2,48000,192000,4,16,{'d','a','t','a'},192000};
        std::ofstream file(wave,std::ios::binary); file.write(reinterpret_cast<const char*>(&header),sizeof(header));
        std::string zero(192000,'\0'); file.write(zero.data(),zero.size()); file.close();
        auto path=wave.u8string(); auto duration=platform::audioProbeDuration(path.c_str());
        require(duration>=990 && duration<=1010,"Synthetic WAV duration is incorrect");
        require(NativeAudioOutput::Open(silence,nullptr),"Windows PCM output could not open");
        require(waitFor([] {return mixedFrames>=2048;}),"PCM mixer callback did not execute");
        NativeAudioOutput::Close(); auto completed=mixedFrames.load();
        std::this_thread::sleep_for(std::chrono::milliseconds(30)); require(mixedFrames==completed,"PCM callback continued after Close");
        require(platform::audioLoadStream(0,path.c_str(),0,false,false),"Windows media stream did not open");
        platform::audioSetStreamVolume(0,0,0);
        require(waitFor([] { return platform::audioGetStreamDuration(0)>=990; }),"Media stream was not prepared");
        platform::audioPlayStream(0); require(waitFor([] {return platform::audioIsStreamPlaying(0);}),"Media stream did not play");
        require(waitFor([] {return platform::audioGetStreamPosition(0)>0;}),"Media position did not advance");
        platform::audioPauseStream(0,true); require(waitFor([] {return !platform::audioIsStreamPlaying(0);}),"Media stream did not pause");
        platform::audioStopStream(0); require(!platform::audioIsStreamPlaying(0),"Stopped media stream remained active");
        std::cerr<<"Windows PCM/media checks passed; checking IMA decoder.\n";
        // Two hundred silent stereo IMA blocks, 249 frames per block. This
        // exercises the original game's compressed radio format without assets.
        auto compressed=root/"ima-silence.wav"; std::ofstream ima(compressed,std::ios::binary);
        auto u16=[&](uint16_t value) {ima.write(reinterpret_cast<const char*>(&value),2);};
        auto u32=[&](uint32_t value) {ima.write(reinterpret_cast<const char*>(&value),4);};
        ima.write("RIFF",4); u32(51252); ima.write("WAVEfmt ",8); u32(20); u16(17); u16(2);
        u32(22050); u32(22669); u16(256); u16(4); u16(2); u16(249);
        ima.write("fact",4); u32(4); u32(49800); ima.write("data",4); u32(51200);
        std::string silentBlocks(51200,'\0'); ima.write(silentBlocks.data(),silentBlocks.size()); ima.close();
        auto compressedPath=compressed.u8string();
        auto converted=WindowsPlayableAudio(compressedPath.c_str(),user.u8string());
        std::cerr<<"IMA conversion completed.\n";
        require(converted!=compressedPath,"IMA source was not decoded locally");
        require(WindowsPlayableAudio(compressedPath.c_str(),user.u8string())==converted,"PCM cache was not reusable");
        require(std::filesystem::file_size(compressed)==51260,"Original compressed audio changed");
        std::cerr<<"IMA cache and source preservation passed.\n";
        auto compressedDuration=platform::audioProbeDuration(compressedPath.c_str());
        require(compressedDuration>=2240 && compressedDuration<=2280,"Decoded IMA duration is incorrect");
        std::cerr<<"IMA duration passed.\n";
        require(platform::audioLoadStream(0,compressedPath.c_str(),0,false,true),"Decoded IMA playback failed to open");
        platform::audioSetStreamVolume(0,0,0);
        require(waitFor([] {return platform::audioIsStreamPlaying(0);}),"Decoded IMA stream did not play");
        std::cerr<<"IMA playback passed.\n";
        platform::audioStopStream(0);
        platform::shutdownWindows(); configured=false;
        std::cerr<<"Windows data isolation, PCM mixer lifecycle, media duration/play/pause/stop and IMA cache/playback passed."<<std::endl; return 0;
    } catch(const std::exception &error) {
        std::cerr<<"Windows platform failure: "<<error.what()<<'\n';
        if(configured) platform::shutdownWindows();
        std::cerr<<error.what()<<'\n'; return 1;
    }
}
