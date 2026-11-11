#include "RuntimeCompat.h"
#include "platform_windows.h"
#include "platform_android.h"
#include "NativeAudioOutput.h"
#include "WaveDecode.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <propvarutil.h>
#include <wrl/client.h>
#include <mmsystem.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <avrt.h>
#include <future>
#include <dbghelp.h>
#include <array>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;
int strcasecmp(const char *a,const char *b) { return _stricmp(a,b); }
int strncasecmp(const char *a,const char *b,size_t size) { return _strnicmp(a,b,size); }
namespace {
std::string dataRoot, userRoot;
std::atomic<bool> quit{false};
std::atomic<const char*> checkpoint{"startup"};
LONG WINAPI crashReport(EXCEPTION_POINTERS *exception) {
    auto process=GetCurrentProcess();
    wchar_t executable[32768]{}; GetModuleFileNameW(nullptr,executable,32768);
    auto symbols=std::filesystem::path(executable).parent_path().wstring();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
    std::fprintf(stderr,"PCVR crash: exception %08lx, checkpoint %s\n",exception->ExceptionRecord->ExceptionCode,checkpoint.load());
    if(SymInitializeW(process,symbols.c_str(),TRUE)) {
        CONTEXT context=*exception->ContextRecord;
        STACKFRAME64 frame{}; frame.AddrPC.Offset=context.Rip; frame.AddrStack.Offset=context.Rsp; frame.AddrFrame.Offset=context.Rbp;
        frame.AddrPC.Mode=frame.AddrStack.Mode=frame.AddrFrame.Mode=AddrModeFlat;
        for(int i=0;i<24;++i) {
            alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO)+1024]{};
            auto symbol=reinterpret_cast<SYMBOL_INFO*>(storage); symbol->SizeOfStruct=sizeof(SYMBOL_INFO); symbol->MaxNameLen=1024;
            DWORD64 displacement=0; IMAGEHLP_LINE64 line{}; line.SizeOfStruct=sizeof(line); DWORD lineDisplacement=0;
            if(SymFromAddr(process,frame.AddrPC.Offset,&displacement,symbol)) {
                if(SymGetLineFromAddr64(process,frame.AddrPC.Offset,&lineDisplacement,&line))
                    std::fprintf(stderr,"PCVR stack: %s (%s:%lu)\n",symbol->Name,line.FileName,line.LineNumber);
                else std::fprintf(stderr,"PCVR stack: %s\n",symbol->Name);
            }
            if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,GetCurrentThread(),&frame,&context,nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr)) break;
        }
        SymCleanup(process);
    }
    std::fflush(stderr); return EXCEPTION_EXECUTE_HANDLER;
}
BOOL WINAPI controlEvent(DWORD event) {
    if(event==CTRL_C_EVENT || event==CTRL_BREAK_EVENT) { quit=true; return TRUE; } return FALSE;
}
std::wstring wide(const char *text) {
    const int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,nullptr,0);
    if(!size) return {};
    std::wstring result(size,L'\0'); MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,result.data(),size);
    result.pop_back(); return result;
}
void seek(IMFPMediaPlayer *player,int ms) {
    PROPVARIANT position{}; position.vt=VT_I8; position.hVal.QuadPart=LONGLONG(std::max(0,ms))*10000;
    player->SetPosition(MFP_POSITIONTYPE_100NS,&position);
}
class MediaCallback final : public IMFPMediaPlayerCallback {
    std::atomic<ULONG> references{1};
public:
    std::atomic<bool> loop{false}, play{false}, cancelled{false}; int startMs=0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void **out) override {
        if(!out) return E_POINTER; *out=nullptr;
        if(id==__uuidof(IUnknown) || id==__uuidof(IMFPMediaPlayerCallback)) { *out=static_cast<IMFPMediaPlayerCallback*>(this); AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override { auto count=--references; if(!count) delete this; return count; }
    void STDMETHODCALLTYPE OnMediaPlayerEvent(MFP_EVENT_HEADER *event) override {
        if(cancelled) return;
        if(FAILED(event->hrEvent)) { play=false; std::fprintf(stderr,"Windows media event failed: %08lx\n",event->hrEvent); return; }
        if(event->eEventType==MFP_EVENT_TYPE_MEDIAITEM_CREATED) {
            auto created=MFP_GET_MEDIAITEM_CREATED_EVENT(event);
            event->pMediaPlayer->SetMediaItem(created->pMediaItem);
        } else if(event->eEventType==MFP_EVENT_TYPE_MEDIAITEM_SET) {
            if(startMs) seek(event->pMediaPlayer,startMs);
            if(play) event->pMediaPlayer->Play();
        } else if(event->eEventType==MFP_EVENT_TYPE_PLAYBACK_ENDED) {
            if(loop && play) { seek(event->pMediaPlayer,0); event->pMediaPlayer->Play(); }
            else play=false;
        }
    }
};
struct MediaStream { ComPtr<IMFPMediaPlayer> player; ComPtr<MediaCallback> callback; };
std::array<MediaStream,3> streams;
MediaStream *stream(int i) { return i>=0 && i<int(streams.size()) ? &streams[i] : nullptr; }
int mediaTime(IMFPMediaPlayer *player,bool duration) {
    if(!player) return 0;
    PROPVARIANT value{}; auto result=duration ? player->GetDuration(MFP_POSITIONTYPE_100NS,&value) : player->GetPosition(MFP_POSITIONTYPE_100NS,&value);
    LONGLONG ticks=0;
    int ms=SUCCEEDED(result) && SUCCEEDED(PropVariantToInt64(value,&ticks)) ?
        int(std::clamp<LONGLONG>(ticks/10000,0,INT_MAX)) : 0;
    PropVariantClear(&value); return ms;
}
}
namespace platform {
bool configureWindows(const std::string &data,const std::string &user) {
    if(!std::filesystem::is_regular_file(std::filesystem::u8path(data)/".libertyvr-pcvr-data")) return false;
    if(!std::filesystem::is_directory(std::filesystem::u8path(data)/"models") ||
       !std::filesystem::is_directory(std::filesystem::u8path(data)/"data")) return false;
    dataRoot=std::filesystem::absolute(std::filesystem::u8path(data)).u8string();
    userRoot=std::filesystem::absolute(std::filesystem::u8path(user)).u8string();
    std::filesystem::create_directories(std::filesystem::u8path(userRoot));
    for(const char *directory : {"simulation-replays","renderdoc-captures","perf-captures"})
        std::filesystem::create_directories(std::filesystem::u8path(userRoot)/directory);
    if(FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED))) return false;
    if(FAILED(MFStartup(MF_VERSION))) { CoUninitialize(); return false; }
    SetConsoleCtrlHandler(controlEvent,TRUE); SetUnhandledExceptionFilter(crashReport); return true;
}
void shutdownWindows() { for(int i=0;i<3;++i) audioStopStream(i); NativeAudioOutput::Close(); MFShutdown(); CoUninitialize(); }
bool exitRequested() { return quit.load(); }
void requestExit() { quit=true; }
void redirectStdioToLog() {}
void setStorageRoot(const char *root) { userRoot=root ? root : ""; }
void setNativeActivity(ANativeActivity*) {}
void setCheckpoint(const char *label) { checkpoint.store(label,std::memory_order_relaxed); }
void startStallWatchdog() {}
const char *storageRoot() { return userRoot.c_str(); }
const char *gameDataRoot() { return dataRoot.c_str(); }
char *resolveGamePath(char *out,size_t capacity,const char *relative) {
    std::string path=(std::filesystem::u8path(dataRoot)/std::filesystem::u8path(relative)).u8string();
    if(path.size()+1>capacity) { if(capacity) *out=0; return out; }
    memcpy(out,path.c_str(),path.size()+1); return out;
}
bool resolveGamePathCaseInsensitive(char *out,size_t capacity,const char *relative) {
    resolveGamePath(out,capacity,relative); return *out && std::filesystem::exists(std::filesystem::u8path(out));
}
uint32_t audioProbeDuration(const char *path) {
    auto initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    ComPtr<IMFSourceReader> reader; uint32_t duration=0;
    std::string playable;
    try { playable=WindowsPlayableAudio(path,userRoot); } catch(...) { if(SUCCEEDED(initialized)) CoUninitialize(); return 0; }
    if(SUCCEEDED(MFCreateSourceReaderFromURL(wide(playable.c_str()).c_str(),nullptr,&reader))) {
        PROPVARIANT value{};
        if(SUCCEEDED(reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE,MF_PD_DURATION,&value)) && value.vt==VT_UI8)
            duration=uint32_t(std::min<ULONGLONG>(UINT32_MAX,value.uhVal.QuadPart/10000));
        PropVariantClear(&value);
    }
    reader.Reset(); if(SUCCEEDED(initialized)) CoUninitialize(); return duration;
}
std::string audioProbeTitle(const char*) { return {}; }
bool audioLoadStream(int i,const char *path,int position,bool loop,bool autoStart) {
    auto target=stream(i); if(!target) return false; audioStopStream(i);
    std::string playable;
    try { playable=WindowsPlayableAudio(path,userRoot); } catch(...) { LOGE("Windows compressed audio preparation failed"); return false; }
    target->callback.Attach(new MediaCallback); target->callback->loop=loop;
    target->callback->play=autoStart; target->callback->startMs=position;
    auto result=MFPCreateMediaPlayer(wide(playable.c_str()).c_str(),FALSE,MFP_OPTION_FREE_THREADED_CALLBACK,
        target->callback.Get(),nullptr,&target->player);
    if(FAILED(result)) { target->callback.Reset(); return false; } return true;
}
void audioPlayStream(int i) { auto s=stream(i); if(s && s->player) { s->callback->play=true; s->player->Play(); } }
void audioPauseStream(int i,bool pause) { auto s=stream(i); if(s && s->player) { s->callback->play=!pause; if(pause) s->player->Pause(); else s->player->Play(); } }
void audioStopStream(int i) { auto s=stream(i); if(s) { if(s->callback) { s->callback->cancelled=true; s->callback->play=false; } if(s->player) s->player->Shutdown(); s->player.Reset(); s->callback.Reset(); } }
void audioSetStreamVolume(int i,float left,float right) {
    auto s=stream(i); if(s && s->player) {
        float volume=std::clamp(std::max(left,right),0.0f,1.0f); s->player->SetVolume(volume);
        s->player->SetBalance(volume>0 ? std::clamp((right-left)/volume,-1.0f,1.0f) : 0);
    }
}
void audioSetStreamLoop(int i,bool loop) { auto s=stream(i); if(s && s->callback) s->callback->loop=loop; }
int audioGetStreamPosition(int i) { auto s=stream(i); return s ? mediaTime(s->player.Get(),false) : 0; }
int audioGetStreamDuration(int i) { auto s=stream(i); return s ? mediaTime(s->player.Get(),true) : 0; }
bool audioIsStreamPlaying(int i) {
    auto s=stream(i);
    // MF prepares and starts asynchronously. MusicManager interprets false as
    // end-of-track, so include a requested start (and loop restart) until the
    // completion/error callback clears it. Preloaded and paused streams stay idle.
    return s && s->player && s->callback && s->callback->play.load();
}
}
namespace NativeAudioOutput {
namespace {
HANDLE event=nullptr;
std::atomic<bool> stop{false};
std::thread worker;
void render(Callback callback,void *user,std::promise<bool> ready) {
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    HRESULT result=initialized;
    unsigned emptyBuffers=0;
    double longestMixMs=0;
    HANDLE scheduling=nullptr;
    {
        ComPtr<IMMDeviceEnumerator> enumerator;
        ComPtr<IMMDevice> device;
        ComPtr<IAudioClient> client;
        ComPtr<IAudioRenderClient> renderer;
        UINT32 capacity=0;
        WAVEFORMATEX format{};
        format.wFormatTag=WAVE_FORMAT_PCM; format.nChannels=2; format.nSamplesPerSec=48000;
        format.wBitsPerSample=16; format.nBlockAlign=4; format.nAvgBytesPerSec=192000;
        if(SUCCEEDED(result)) result=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator));
        if(SUCCEEDED(result)) result=enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&device);
        if(SUCCEEDED(result)) result=device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,&client);
        if(SUCCEEDED(result)) result=client->Initialize(AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
            300000,0,&format,nullptr);
        if(SUCCEEDED(result)) result=client->SetEventHandle(event);
        if(SUCCEEDED(result)) result=client->GetBufferSize(&capacity);
        if(SUCCEEDED(result)) result=client->GetService(IID_PPV_ARGS(&renderer));
        BYTE *data=nullptr;
        if(SUCCEEDED(result)) result=renderer->GetBuffer(capacity,&data);
        if(SUCCEEDED(result)) result=renderer->ReleaseBuffer(capacity,AUDCLNT_BUFFERFLAGS_SILENT);
        DWORD task=0;
        if(SUCCEEDED(result)) {
            scheduling=AvSetMmThreadCharacteristicsW(L"Pro Audio",&task);
            if(scheduling) AvSetMmThreadPriority(scheduling,AVRT_PRIORITY_HIGH);
            result=client->Start();
        }
        ready.set_value(SUCCEEDED(result));
        if(SUCCEEDED(result)) {
            std::fprintf(stderr,"Windows SFX: WASAPI stereo 48000 Hz, buffer %u frames, MMCSS %s\n",capacity,scheduling ? "enabled" : "unavailable");
            while(!stop) {
                const auto wait=WaitForSingleObject(event,2000);
                if(stop) break;
                if(wait!=WAIT_OBJECT_0) { result=HRESULT_FROM_WIN32(wait==WAIT_TIMEOUT ? ERROR_TIMEOUT : GetLastError()); break; }
                UINT32 padding=0;
                result=client->GetCurrentPadding(&padding);
                if(FAILED(result)) break;
                if(padding==0) ++emptyBuffers;
                const UINT32 available=capacity-padding;
                if(!available) continue;
                result=renderer->GetBuffer(available,&data);
                if(FAILED(result)) break;
                const auto before=std::chrono::steady_clock::now();
                callback(user,data,static_cast<int32_t>(available));
                longestMixMs=std::max(longestMixMs,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-before).count());
                result=renderer->ReleaseBuffer(available,0);
                if(FAILED(result)) break;
            }
            client->Stop();
        }
        if(scheduling) AvRevertMmThreadCharacteristics(scheduling);
    }
    if(SUCCEEDED(initialized)) CoUninitialize();
    std::fprintf(stderr,"Windows SFX: empty buffers %u, longest mix %.2f ms, result %08lx\n",emptyBuffers,longestMixMs,result);
}
}
void Close() {
    stop=true;
    if(event) SetEvent(event);
    if(worker.joinable()) worker.join();
    if(event) { CloseHandle(event); event=nullptr; }
}
bool Open(Callback callback,void *user) {
    Close();
    if(!callback) return false;
    event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!event) return false;
    stop=false;
    std::promise<bool> ready;
    auto started=ready.get_future();
    try { worker=std::thread(render,callback,user,std::move(ready)); }
    catch(...) { Close(); return false; }
    if(!started.get()) { Close(); return false; }
    return true;
}
}
