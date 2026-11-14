#include "platform_desktop.h"
#include "platform_android.h"
#include "AssetVariants.h"
#include "DesktopAudio.h"
#include <filesystem>
#include <atomic>
#include <csignal>
#include <cstring>
#include <cstdio>
#ifdef _WIN32
#include <windows.h>
int strcasecmp(const char *a,const char *b){return _stricmp(a,b);}
int strncasecmp(const char *a,const char *b,size_t n){return _strnicmp(a,b,n);}
#else
#include <unistd.h>
#endif
namespace {
std::string dataRoot,userRoot;
volatile std::sig_atomic_t quit=0;
std::atomic<const char*> checkpoint{"startup"};
void interrupted(int){quit=1;}
}
namespace platform {
bool configureDesktop(const std::string &data,const std::string &user){
  if(!std::filesystem::is_regular_file(std::filesystem::u8path(data)/".libertyvr-pcvr-data"))return false;
  const int selected=assetvariants::Select(data);if(selected<0)return false;
  dataRoot=std::filesystem::absolute(std::filesystem::u8path(data)).u8string();userRoot=std::filesystem::absolute(std::filesystem::u8path(user)).u8string();
  std::filesystem::create_directories(std::filesystem::u8path(userRoot));
  for(const char *dir:{"simulation-replays","renderdoc-captures","perf-captures"})std::filesystem::create_directories(std::filesystem::u8path(userRoot)/dir);
  quit=0;std::signal(SIGINT,interrupted);std::signal(SIGTERM,interrupted);
  installNativeDiagnostics(&checkpoint);
  return DesktopAudio::Initialize(userRoot);
}
void shutdownDesktop(){DesktopAudio::Shutdown();}
bool exitRequested(){return quit!=0;}
void requestExit(){quit=1;}
void redirectStdioToLog(){}
void setStorageRoot(const char *root){userRoot=root?root:"";}
void setNativeActivity(ANativeActivity*){}
void setCheckpoint(const char *label){checkpoint.store(label,std::memory_order_relaxed);}
void startStallWatchdog(){}
const char *storageRoot(){return userRoot.c_str();}
const char *gameDataRoot(){return dataRoot.c_str();}
char *resolveGamePath(char *out,size_t capacity,const char *relative){auto path=assetvariants::Resolve(dataRoot,relative);if(path.size()+1>capacity){if(capacity)*out=0;return out;}memcpy(out,path.c_str(),path.size()+1);return out;}
bool resolveGamePathCaseInsensitive(char *out,size_t capacity,const char *relative){
  resolveGamePath(out,capacity,relative);if(!capacity || !*out)return false;
  auto requested=std::filesystem::u8path(out);if(std::filesystem::exists(requested))return true;
  auto current=requested.root_path();std::error_code error;
  for(const auto &component:requested.relative_path()) {
    auto next=current/component;if(!std::filesystem::exists(next,error)){
      auto wanted=component.u8string();for(char &c:wanted)if(c>='A'&&c<='Z')c+=32;bool found=false;
      for(std::filesystem::directory_iterator it(current,error),end;!error && it!=end;it.increment(error)){auto name=it->path().filename().u8string();for(char &c:name)if(c>='A'&&c<='Z')c+=32;if(name==wanted){if(found)return false;next=it->path();found=true;}}
      if(!found)return false;
    }current=next;
  }
  auto path=current.u8string();if(path.size()+1>capacity)return false;memcpy(out,path.c_str(),path.size()+1);return true;
}
}
