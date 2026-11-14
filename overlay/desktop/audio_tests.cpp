#include "DesktopAudio.h"
#include "NativeAudioOutput.h"
#include "platform_android.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <iostream>
#include <cstring>
static void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
static int32_t silence(void*,void *out,int32_t frames){std::memset(out,0,size_t(frames)*4);return frames;}
int main(int argc,char **argv){auto root=std::filesystem::temp_directory_path()/("lcvr-audio-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try{
    std::filesystem::create_directories(root);auto path=root/"tone.wav";
    std::ofstream out(path,std::ios::binary);auto u16=[&](uint16_t v){char b[2]={char(v),char(v>>8)};out.write(b,2);};auto u32=[&](uint32_t v){char b[4]={char(v),char(v>>8),char(v>>16),char(v>>24)};out.write(b,4);};
    out.write("RIFF",4);u32(48000*4+36);out.write("WAVEfmt ",8);u32(16);u16(1);u16(2);u32(48000);u32(192000);u16(4);u16(16);out.write("data",4);u32(192000);std::string pcm(192000,0);out.write(pcm.data(),pcm.size());out.close();
    auto file=path.u8string();require(platform::audioProbeDuration(file.c_str())==1000,"PCM duration");
    require(DesktopAudio::Initialize(root.u8string()),"SDL audio initialize");require(NativeAudioOutput::Open(silence,nullptr),"SFX callback");
    bool hardware=argc>1 && std::string(argv[1])=="--hardware";
    if(argc>1 && std::string(argv[1])=="--unavailable") {
      require(DesktopAudio::Devices().empty(),"Unavailable backend unexpectedly has an endpoint");
      require(platform::audioLoadStream(0,file.c_str(),200,false,true),"Media must load without an output device");
      std::this_thread::sleep_for(std::chrono::milliseconds(100));DesktopAudio::Pump();
      require(platform::audioGetStreamPosition(0)==200,"Missing output advanced media position");
      DesktopAudio::Shutdown();std::filesystem::remove_all(root);std::cout<<"Missing audio backend handled without losing media state\n";return 0;
    }
    if(hardware)require(!DesktopAudio::Devices().empty(),"No hardware audio output endpoint");
    require(!platform::audioLoadStream(0,"missing-file",0,false,true),"Missing media accepted");
    require(platform::audioLoadStream(0,file.c_str(),200,false,false),"PCM load/seek");
    require(platform::audioGetStreamPosition(0)==200,"Initial seek position");platform::audioPlayStream(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(180));platform::audioPauseStream(0,true);int paused=platform::audioGetStreamPosition(0);require(paused>200,"Playback did not advance");
    std::this_thread::sleep_for(std::chrono::milliseconds(70));require(platform::audioGetStreamPosition(0)==paused,"Pause advanced position");
    platform::audioPauseStream(0,false);platform::audioSetStreamVolume(0,.2f,.8f);DesktopAudio::Select("Nonexistent test output");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));require(platform::audioGetStreamPosition(0)>=paused,"Device change reset media");
    platform::audioStopStream(0);require(!platform::audioIsStreamPlaying(0),"Stop state");
    require(platform::audioLoadStream(1,file.c_str(),0,true,true),"Loop load");std::this_thread::sleep_for(std::chrono::milliseconds(1150));require(platform::audioIsStreamPlaying(1),"Loop ended");
    if(argc>1 && !hardware){for(const char *name:{"tone.mp3","tone.m4a","tone.ogg","tone.opus","tone.flac","pcm.wav","ima.wav","ms.wav","radio.adf"}){
      auto fixture=(std::filesystem::u8path(argv[1])/name).u8string();auto duration=platform::audioProbeDuration(fixture.c_str());require(duration>=900 && duration<1500,name);
      require(platform::audioLoadStream(2,fixture.c_str(),350,false,true),name);std::this_thread::sleep_for(std::chrono::milliseconds(150));require(platform::audioGetStreamPosition(2)>350,name);platform::audioStopStream(2);std::cout<<"PASS "<<name<<'\n';
    }auto broken=(std::filesystem::u8path(argv[1])/"broken.wav").u8string();require(!platform::audioLoadStream(2,broken.c_str(),0,false,true),"Malformed WAV accepted");}
    DesktopAudio::Shutdown();require(DesktopAudio::Initialize(root.u8string()),"Reinitialize");DesktopAudio::Shutdown();std::filesystem::remove_all(root);std::cout<<"Desktop audio checks passed\n";return 0;
  }catch(const std::exception &e){DesktopAudio::Shutdown();std::filesystem::remove_all(root);std::cerr<<e.what()<<'\n';return 1;}
}
