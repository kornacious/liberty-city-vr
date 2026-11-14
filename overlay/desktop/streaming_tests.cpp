#include "common.h"
#include "CdStream.h"
#include "ModelSets.h"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <iostream>
#include <string>
#include <stdexcept>
#include <chrono>
// Isolate the production archive/worker adapter from game asset selection.
namespace ModelSets {
const char *ResolveAssetPath(const char *path,char*,size_t){return path;}
bool IsModernActive(){return false;}
bool IsXboxActive(){return false;}
bool GetModernAssetPath(const char*,char*,size_t){return false;}
bool GetXboxAssetPath(const char*,char*,size_t){return false;}
}
namespace platform {
bool resolveGamePathCaseInsensitive(char *out,size_t capacity,const char *path){if(std::strlen(path)>=capacity)return false;std::strcpy(out,path);return true;}
}
static void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
int main(){auto root=std::filesystem::temp_directory_path()/("lcvr-stream-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 try{
  std::filesystem::create_directories(root);auto a=(root/"first.img").u8string(),b=(root/"second.img").u8string();
  {std::ofstream f(std::filesystem::u8path(a),std::ios::binary);f<<std::string(4096,'A');}
  {std::ofstream f(std::filesystem::u8path(b),std::ios::binary);f<<std::string(4096,'B');}
  CdStreamInit(2);require(CdStreamAddImage(a.c_str()),"First archive");require(CdStreamAddImage(b.c_str()),"Second archive");require(CdStreamGetNumImages()==2,"Archive count");
  char first[2048]{},second[2048]{};
  for(int i=0;i<30;++i){require(CdStreamRead(0,first,0,1)==STREAM_SUCCESS,"First read submit");require(CdStreamRead(1,second,0x01000001,1)==STREAM_SUCCESS,"Second read submit");require(CdStreamGetLastPosn()==0x01000002,"Encoded last archive position");require(CdStreamSync(0)==STREAM_NONE && CdStreamSync(1)==STREAM_NONE,"Read completion");require(first[0]=='A' && second[2047]=='B',"Archive selection/offset");}
  require(CdStreamRead(0,first,0x02000000,1)==STREAM_ERROR,"Unknown archive accepted");
  require(CdStreamRead(0,first,20,1)==STREAM_SUCCESS,"Out-of-range submit");require(CdStreamSync(0)==STREAM_ERROR,"Short read not reported");
  require(CdStreamRead(0,first,0,1)==STREAM_SUCCESS,"Recovery submit");CdStreamRemoveImages();require(first[0]=='A' && CdStreamGetNumImages()==0,"Close failed to drain reads");
  CdStreamShutdown();CdStreamInit(1);CdStreamShutdown();std::filesystem::remove_all(root);std::cout<<"Desktop streaming checks passed\n";return 0;
 }catch(const std::exception &error){CdStreamShutdown();std::filesystem::remove_all(root);std::cerr<<error.what()<<'\n';return 1;}
}
