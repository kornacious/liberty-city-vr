#include "DesktopAudio.h"
#include "NativeAudioOutput.h"
#include "platform_android.h"
#include <SDL3/SDL.h>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
}
#include <array>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>
#include <cstdio>
namespace {
constexpr unsigned Rate=48000, Capacity=32768;
struct Decoder {
  AVFormatContext *format=nullptr;AVCodecContext *codec=nullptr;SwrContext *resampler=nullptr;
  AVFrame *frame=nullptr;AVPacket *packet=nullptr;int track=-1;bool draining=false,encrypted=false;
  SDL_IOStream *input=nullptr;AVIOContext *io=nullptr;
  ~Decoder(){av_packet_free(&packet);av_frame_free(&frame);swr_free(&resampler);avcodec_free_context(&codec);avformat_close_input(&format);if(io)av_freep(&io->buffer);avio_context_free(&io);if(input)SDL_CloseIO(input);}
  static int Read(void *context,uint8_t *out,int size){auto &d=*static_cast<Decoder*>(context);size_t count=SDL_ReadIO(d.input,out,size_t(size));if(d.encrypted)for(size_t i=0;i<count;++i)out[i]^=0x22;return count?int(count):AVERROR_EOF;}
  static int64_t SeekFile(void *context,int64_t offset,int whence){auto &d=*static_cast<Decoder*>(context);if(whence&AVSEEK_SIZE)return SDL_GetIOSize(d.input);return SDL_SeekIO(d.input,offset,SDL_IOWhence(whence&~AVSEEK_FORCE));}
  bool Open(const char *path) {
    // Only local files enter this backend; network protocols are disabled in the build.
    std::string extension=std::filesystem::u8path(path).extension().u8string();for(char &c:extension)if(c>='A'&&c<='Z')c+=32;encrypted=extension==".adf";
    input=SDL_IOFromFile(path,"rb");if(!input)return false;
    auto buffer=static_cast<uint8_t*>(av_malloc(32768));if(!buffer)return false;
    io=avio_alloc_context(buffer,32768,0,this,Read,nullptr,SeekFile);if(!io){av_free(buffer);return false;}
    format=avformat_alloc_context();if(!format)return false;format->pb=io;format->flags|=AVFMT_FLAG_CUSTOM_IO;
    if(avformat_open_input(&format,nullptr,nullptr,nullptr)<0 || avformat_find_stream_info(format,nullptr)<0)return false;
    track=av_find_best_stream(format,AVMEDIA_TYPE_AUDIO,-1,-1,nullptr,0);if(track<0)return false;
    auto parameters=format->streams[track]->codecpar;auto decoder=avcodec_find_decoder(parameters->codec_id);
    if(!decoder)return false;codec=avcodec_alloc_context3(decoder);
    if(!codec || avcodec_parameters_to_context(codec,parameters)<0 || avcodec_open2(codec,decoder,nullptr)<0)return false;
    AVChannelLayout stereo=AV_CHANNEL_LAYOUT_STEREO;
    if(swr_alloc_set_opts2(&resampler,&stereo,AV_SAMPLE_FMT_FLT,Rate,&codec->ch_layout,codec->sample_fmt,codec->sample_rate,0,nullptr)<0 || swr_init(resampler)<0)return false;
    frame=av_frame_alloc();packet=av_packet_alloc();return frame && packet;
  }
  int Duration()const {return format && format->duration>0?int(std::min<int64_t>(INT32_MAX,format->duration/1000)):0;}
  bool Seek(int ms){auto stream=format->streams[track];auto target=av_rescale_q(int64_t(ms)*1000,AV_TIME_BASE_Q,stream->time_base)+(stream->start_time==AV_NOPTS_VALUE?0:stream->start_time);if(av_seek_frame(format,track,target,AVSEEK_FLAG_BACKWARD)<0)return false;avcodec_flush_buffers(codec);swr_close(resampler);swr_init(resampler);draining=false;return true;}
  // Return a decoded frame, draining delayed codec data at EOF.
  bool Next(){for(;;){int result=avcodec_receive_frame(codec,frame);if(result==0)return true;if(result==AVERROR_EOF)return false;if(result!=AVERROR(EAGAIN))return false;if(draining)return false;
    while((result=av_read_frame(format,packet))>=0){if(packet->stream_index==track){result=avcodec_send_packet(codec,packet);av_packet_unref(packet);break;}av_packet_unref(packet);}
    if(result<0){avcodec_send_packet(codec,nullptr);draining=true;}
  }}
};
struct Media {
  Decoder decoder;std::thread worker;std::atomic<bool> stop{false},paused{false},loop{false},eof{false};
  std::atomic<float> left{1},right{1};std::atomic<uint64_t> read{0},write{0},played{0};
  std::array<float,Capacity*2> samples{};int duration=0,startMs=0;
  ~Media(){stop=true;if(worker.joinable())worker.join();}
  void Push(const float *pcm,int frames){for(int i=0;i<frames && !stop;){
    auto w=write.load(std::memory_order_relaxed),r=read.load(std::memory_order_acquire);
    unsigned count=unsigned(std::min<uint64_t>(frames-i,Capacity-(w-r)));
    if(!count){std::this_thread::sleep_for(std::chrono::milliseconds(2));continue;}
    for(unsigned n=0;n<count;++n){samples[((w+n)%Capacity)*2]=pcm[size_t(i+n)*2];samples[((w+n)%Capacity)*2+1]=pcm[size_t(i+n)*2+1];}
    write.store(w+count,std::memory_order_release);i+=int(count);
  }}
  void Run(){std::vector<float> pcm;int64_t skipUntil=int64_t(startMs)*Rate/1000;
    do {
      bool produced=false;
      while(!stop && decoder.Next()) {
        auto *f=decoder.frame;int frames=int(av_rescale_rnd(swr_get_delay(decoder.resampler,decoder.codec->sample_rate)+f->nb_samples,Rate,decoder.codec->sample_rate,AV_ROUND_UP));
        pcm.resize(size_t(frames)*2);uint8_t *output=reinterpret_cast<uint8_t*>(pcm.data());
        frames=swr_convert(decoder.resampler,&output,frames,const_cast<const uint8_t**>(f->extended_data),f->nb_samples);if(frames<0){stop=true;break;}
        int skip=0;
        if(skipUntil>0 && f->best_effort_timestamp!=AV_NOPTS_VALUE){auto at=av_rescale_q(f->best_effort_timestamp,decoder.format->streams[decoder.track]->time_base,AVRational{1,int(Rate)});skip=int(std::clamp<int64_t>(skipUntil-at,0,frames));if(skip<frames)skipUntil=0;}
        if(frames>skip){produced=true;Push(pcm.data()+size_t(skip)*2,frames-skip);}
      }
      // Drain the resampler too, so the last fractional-rate samples survive EOF.
      pcm.resize(8192);while(!stop){uint8_t *out=reinterpret_cast<uint8_t*>(pcm.data());int frames=swr_convert(decoder.resampler,&out,4096,nullptr,0);if(frames<=0)break;produced=true;Push(pcm.data(),frames);}
      if(!stop && produced && loop && decoder.Seek(0)){skipUntil=0;continue;}break;
    }while(!stop);
    eof=true;
  }
};
SDL_AudioStream *output=nullptr;SDL_AudioDeviceID device=0;
NativeAudioOutput::Callback sfx=nullptr;void *sfxUser=nullptr;
std::array<std::unique_ptr<Media>,3> media;
std::string preference,preferenceFile,status="Audio not initialized";
bool initialized=false,audioReady=false;std::atomic<uint64_t> underruns{0},maxCallbackUs{0};
struct Lock {Lock(){if(output)SDL_LockAudioStream(output);}~Lock(){if(output)SDL_UnlockAudioStream(output);}};
void SDLCALL Mix(void*,SDL_AudioStream *stream,int additional,int) {
  auto begin=std::chrono::steady_clock::now();
  std::array<int16_t,2048> pcm{};
  for(int remain=(additional+3)/4;remain>0;){int count=std::min(remain,1024);std::fill(pcm.begin(),pcm.end(),0);if(sfx)sfx(sfxUser,pcm.data(),count);
    for(auto &slot:media){auto *m=slot.get();if(!m || m->paused)continue;auto r=m->read.load(std::memory_order_relaxed),w=m->write.load(std::memory_order_acquire);int available=int(std::min<uint64_t>(count,w-r));
      float l=m->left.load(),rr=m->right.load();for(int n=0;n<available;++n){for(int channel=0;channel<2;++channel){float sample=pcm[n*2+channel]+m->samples[((r+n)%Capacity)*2+channel]*32767.f*(channel?rr:l);pcm[n*2+channel]=int16_t(std::clamp(sample,-32768.f,32767.f));}}
      m->read.store(r+available,std::memory_order_release);m->played+=available;if(available<count && !m->eof)++underruns;
    }
    SDL_PutAudioStreamData(stream,pcm.data(),count*4);remain-=count;
  }
  auto us=uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count());auto old=maxCallbackUs.load();while(us>old && !maxCallbackUs.compare_exchange_weak(old,us)){}
}
void CloseOutput(){if(output){SDL_DestroyAudioStream(output);output=nullptr;}if(device){SDL_CloseAudioDevice(device);device=0;}}
bool OpenOutput(){SDL_AudioDeviceID physical=SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
  if(!preference.empty()){int count=0;auto ids=SDL_GetAudioPlaybackDevices(&count);bool found=false;for(int i=0;i<count;++i){auto name=SDL_GetAudioDeviceName(ids[i]);if(name && preference==name){physical=ids[i];found=true;break;}}SDL_free(ids);if(!found)status="Selected output unavailable; using system default";else status="Selected audio output active";}else status="System default audio output";
  SDL_AudioSpec spec{SDL_AUDIO_S16,2,int(Rate)};auto next=SDL_OpenAudioDevice(physical,&spec);if(!next){status="No audio output; waiting for a device";return false;}
  if(!output){output=SDL_CreateAudioStream(&spec,&spec);if(output)SDL_SetAudioStreamGetCallback(output,Mix,nullptr);}
  if(!output){SDL_CloseAudioDevice(next);return false;}
  SDL_UnbindAudioStream(output);if(device)SDL_CloseAudioDevice(device);device=next;
  if(!SDL_BindAudioStream(device,output)){SDL_CloseAudioDevice(device);device=0;status="Audio output unavailable; retrying";return false;}
  SDL_ResumeAudioDevice(device);return true;
}
Media *Slot(int i){return i>=0 && i<int(media.size())?media[i].get():nullptr;}
}
namespace DesktopAudio {
bool Initialize(const std::string &root){if(initialized)return true;audioReady=SDL_InitSubSystem(SDL_INIT_AUDIO);initialized=true;preferenceFile=(std::filesystem::u8path(root)/"desktop-audio.cfg").u8string();std::ifstream file(std::filesystem::u8path(preferenceFile));std::getline(file,preference);if(audioReady)OpenOutput();else status="Audio unavailable; waiting for an output backend";return true;}
void Shutdown(){CloseOutput();for(auto &m:media)m.reset();sfx=nullptr;sfxUser=nullptr;if(audioReady)SDL_QuitSubSystem(SDL_INIT_AUDIO);initialized=audioReady=false;std::fprintf(stderr,"Desktop audio: %llu underruns, longest callback %llu us\n",(unsigned long long)underruns.load(),(unsigned long long)maxCallbackUs.load());}
std::vector<std::string> Devices(){std::vector<std::string> result;if(!audioReady)return result;int count=0;auto ids=SDL_GetAudioPlaybackDevices(&count);for(int i=0;i<count;++i)if(auto name=SDL_GetAudioDeviceName(ids[i]))result.emplace_back(name);SDL_free(ids);return result;}
const std::string &Preference(){return preference;}
void Select(const std::string &name){preference=name;std::ofstream file(std::filesystem::u8path(preferenceFile+".partial"));file<<name<<'\n';file.close();if(file)SDL_RenamePath((preferenceFile+".partial").c_str(),preferenceFile.c_str());if(audioReady)OpenOutput();}
const char *Status(){return status.c_str();}
void Pump(){if(!initialized)return;static auto previous=std::chrono::steady_clock::now();static std::vector<std::string> old;auto now=std::chrono::steady_clock::now();if(now-previous<std::chrono::seconds(1))return;previous=now;if(!audioReady)audioReady=SDL_InitSubSystem(SDL_INIT_AUDIO);auto devices=Devices();if(audioReady && (!device || (!preference.empty() && devices!=old)))OpenOutput();old=std::move(devices);}
}
namespace NativeAudioOutput {
bool Open(Callback callback,void *user){Lock lock;sfx=callback;sfxUser=user;return initialized;}
void Close(){Lock lock;sfx=nullptr;sfxUser=nullptr;}
}
namespace platform {
uint32_t audioProbeDuration(const char *path){Decoder d;return d.Open(path)?uint32_t(d.Duration()):0;}
std::string audioProbeTitle(const char *path){Decoder d;if(!d.Open(path))return {};auto entry=av_dict_get(d.format->metadata,"title",nullptr,0);return entry?entry->value:"";}
void audioStopStream(int i){if(i<0 || i>=int(media.size()))return;std::unique_ptr<Media> old;{Lock lock;old=std::move(media[i]);}}
bool audioLoadStream(int i,const char *path,int position,bool loop,bool autoStart){if(i<0 || i>=int(media.size()))return false;audioStopStream(i);auto m=std::make_unique<Media>();if(!m->decoder.Open(path))return false;m->duration=m->decoder.Duration();m->startMs=std::max(0,position);if(m->duration)m->startMs=std::min(m->startMs,m->duration);if(m->startMs && !m->decoder.Seek(m->startMs))return false;m->loop=loop;m->paused=!autoStart;m->worker=std::thread(&Media::Run,m.get());{Lock lock;media[i]=std::move(m);}return true;}
void audioPlayStream(int i){Lock lock;if(auto m=Slot(i))m->paused=false;}
void audioPauseStream(int i,bool pause){Lock lock;if(auto m=Slot(i))m->paused=pause;}
void audioSetStreamVolume(int i,float l,float r){Lock lock;if(auto m=Slot(i)){m->left=std::clamp(l,0.f,1.f);m->right=std::clamp(r,0.f,1.f);}}
void audioSetStreamLoop(int i,bool loop){Lock lock;if(auto m=Slot(i))m->loop=loop;}
int audioGetStreamPosition(int i){Lock lock;auto m=Slot(i);if(!m)return 0;uint64_t ms=m->startMs+m->played.load()*1000/Rate;return m->loop && m->duration?int(ms%m->duration):int(std::min<uint64_t>(INT32_MAX,ms));}
int audioGetStreamDuration(int i){Lock lock;auto m=Slot(i);return m?m->duration:0;}
bool audioIsStreamPlaying(int i){Lock lock;auto m=Slot(i);return m && !m->paused && !(m->eof && m->read==m->write);}
}
