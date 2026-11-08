#include "RuntimeCompat.h"
#include "WaveDecode.h"
#include <mmsystem.h>
#include <mmreg.h>
#include <msacm.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace {
std::mutex decoderMutex;
void require(bool condition,const char *message) { if(!condition) throw std::runtime_error(message); }
template<class T> T read(std::istream &input) { T value{}; input.read(reinterpret_cast<char*>(&value),sizeof(value)); require(bool(input),"Truncated audio header"); return value; }
void pcmHeader(std::ostream &output,uint16_t channels,uint32_t rate,uint32_t bytes) {
    auto u16=[&](uint16_t value) {output.write(reinterpret_cast<const char*>(&value),2);};
    auto u32=[&](uint32_t value) {output.write(reinterpret_cast<const char*>(&value),4);};
    output.write("RIFF",4); u32(bytes+36); output.write("WAVEfmt ",8); u32(16); u16(WAVE_FORMAT_PCM);
    u16(channels); u32(rate); u32(rate*channels*2); u16(channels*2); u16(16); output.write("data",4); u32(bytes);
}
struct Decoder {
    HACMSTREAM stream=nullptr; ACMSTREAMHEADER header{}; bool prepared=false;
    ~Decoder() { if(prepared) acmStreamUnprepareHeader(stream,&header,0); if(stream) acmStreamClose(stream,0); }
};
}
std::string WindowsPlayableAudio(const char *source,const std::string &userRoot) {
    std::lock_guard<std::mutex> lock(decoderMutex);
    const auto path=std::filesystem::u8path(source);
    std::ifstream input(path,std::ios::binary);
    if(!input) return source;
    char riff[4]{}; input.read(riff,4); if(memcmp(riff,"RIFF",4)) return source;
    read<uint32_t>(input); char wave[4]{}; input.read(wave,4); if(memcmp(wave,"WAVE",4)) return source;
    const auto length=std::filesystem::file_size(path);
    std::vector<BYTE> format;
    uint64_t dataOffset=0; uint32_t dataSize=0,factFrames=0;
    while(input && uint64_t(input.tellg())+8<=length) {
        char id[4]; input.read(id,4); auto size=read<uint32_t>(input); auto next=uint64_t(input.tellg())+size+(size&1);
        require(next<=length,"Invalid WAV chunk bounds");
        if(!memcmp(id,"fmt ",4)) {
            require(size>=16 && size<=65536,"Invalid WAV format header");
            format.resize(std::max<size_t>(size,sizeof(WAVEFORMATEX)),0); input.read(reinterpret_cast<char*>(format.data()),size);
        } else if(!memcmp(id,"data",4)) { dataOffset=uint64_t(input.tellg()); dataSize=size; }
        else if(!memcmp(id,"fact",4) && size>=4) factFrames=read<uint32_t>(input);
        input.seekg(std::streamoff(next));
    }
    if(format.empty()) return source;
    auto from=reinterpret_cast<WAVEFORMATEX*>(format.data());
    if(from->wFormatTag!=WAVE_FORMAT_IMA_ADPCM && from->wFormatTag!=WAVE_FORMAT_ADPCM) return source;
    require(sizeof(WAVEFORMATEX)+from->cbSize<=format.size(),"Incomplete ADPCM format extension");
    require((from->nChannels==1 || from->nChannels==2) && from->nSamplesPerSec && from->nBlockAlign && dataSize && dataOffset,
        "Unsupported compressed WAV layout");
    require(dataSize%from->nBlockAlign==0,"Incomplete ADPCM block");
    auto cache=std::filesystem::u8path(userRoot)/"audio-cache"; std::filesystem::create_directories(cache);
    const auto identity=path.u8string()+":"+std::to_string(length)+":"+std::to_string(std::filesystem::last_write_time(path).time_since_epoch().count());
    std::ostringstream name; name<<"pcm-v1-"<<std::hex<<std::hash<std::string>{}(identity)<<".wav";
    const auto outputPath=cache/name.str();
    if(std::filesystem::is_regular_file(outputPath) && std::filesystem::file_size(outputPath)>44) {
        std::ifstream cached(outputPath,std::ios::binary); cached.seekg(40);
        if(uint64_t(read<uint32_t>(cached))+44==std::filesystem::file_size(outputPath)) return outputPath.u8string();
    }
    WAVEFORMATEX to{}; to.wFormatTag=WAVE_FORMAT_PCM; to.nChannels=from->nChannels; to.nSamplesPerSec=from->nSamplesPerSec;
    to.wBitsPerSample=16; to.nBlockAlign=to.nChannels*2; to.nAvgBytesPerSec=to.nSamplesPerSec*to.nBlockAlign;
    Decoder decoder;
    require(acmStreamOpen(&decoder.stream,nullptr,from,&to,nullptr,0,0,ACM_STREAMOPENF_NONREALTIME)==MMSYSERR_NOERROR,
        "Windows ADPCM decoder is unavailable");
    const DWORD sourceCapacity=std::max<DWORD>(from->nBlockAlign,(65536/from->nBlockAlign)*from->nBlockAlign);
    DWORD destinationCapacity=0;
    require(acmStreamSize(decoder.stream,sourceCapacity,&destinationCapacity,ACM_STREAMSIZEF_SOURCE)==MMSYSERR_NOERROR,
        "Cannot size ADPCM conversion");
    std::vector<BYTE> sourceBytes(sourceCapacity),destinationBytes(destinationCapacity);
    auto &header=decoder.header; header.cbStruct=sizeof(header); header.pbSrc=sourceBytes.data(); header.cbSrcLength=sourceCapacity;
    header.pbDst=destinationBytes.data(); header.cbDstLength=destinationCapacity;
    require(acmStreamPrepareHeader(decoder.stream,&header,0)==MMSYSERR_NOERROR,"Cannot prepare ADPCM conversion"); decoder.prepared=true;
    const auto partial=std::filesystem::path(outputPath.wstring()+L".partial"); std::ofstream output(partial,std::ios::binary|std::ios::trunc);
    require(bool(output),"Cannot create local PCM cache"); pcmHeader(output,to.nChannels,to.nSamplesPerSec,0);
    input.clear(); input.seekg(std::streamoff(dataOffset)); uint32_t remaining=dataSize; uint64_t written=0;
    const uint64_t limit=factFrames ? uint64_t(factFrames)*to.nBlockAlign : UINT64_MAX;
    bool first=true;
    while(remaining) {
        const DWORD amount=std::min<DWORD>(remaining,sourceCapacity); input.read(reinterpret_cast<char*>(sourceBytes.data()),amount);
        require(bool(input),"Truncated ADPCM samples"); header.cbSrcLength=amount; header.cbDstLength=destinationCapacity;
        DWORD flags=ACM_STREAMCONVERTF_BLOCKALIGN | (first?ACM_STREAMCONVERTF_START:0) | (amount==remaining?ACM_STREAMCONVERTF_END:0);
        require(acmStreamConvert(decoder.stream,&header,flags)==MMSYSERR_NOERROR && header.cbSrcLengthUsed==amount,"ADPCM conversion failed");
        auto bytes=std::min<uint64_t>(header.cbDstLengthUsed,limit-written);
        require(written+bytes<=UINT32_MAX-36,"PCM cache exceeds WAV limits");
        output.write(reinterpret_cast<const char*>(destinationBytes.data()),std::streamsize(bytes)); require(bool(output),"PCM cache write failed");
        written+=bytes; remaining-=amount; first=false;
    }
    require(written>0,"ADPCM decoder produced no samples"); output.seekp(0); pcmHeader(output,to.nChannels,to.nSamplesPerSec,uint32_t(written));
    output.close(); require(bool(output),"PCM cache close failed");
    require(MoveFileExW(partial.c_str(),outputPath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,"PCM cache publication failed");
    return outputPath.u8string();
}
