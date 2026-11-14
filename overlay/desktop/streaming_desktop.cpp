#include "common.h"
#include "CdStream.h"
#include "ModelSets.h"
#include "QuestCdStream.h"
#include "platform_android.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstring>
#include <algorithm>
namespace {
queststream::Queue queue;
std::array<SDL_IOStream*,MAX_CDIMAGES> images{};
char names[MAX_CDIMAGES][1024]{};
int imageCount=0,channelCount=0,lastPosition=0;
bool ReadImage(int index,void *buffer,size_t bytes,uint64_t offset,void*) {
  if(index<0 || index>=MAX_CDIMAGES || !images[index] || SDL_SeekIO(images[index],int64_t(offset),SDL_IO_SEEK_SET)<0)return false;
  return SDL_ReadIO(images[index],buffer,bytes)==bytes;
}
}
#ifdef FLUSHABLE_STREAMING
bool flushStream[MAX_CDCHANNELS]{};
#endif
extern "C" {
void CdStreamInit(int count){CdStreamRemoveImages();queue.Stop();channelCount=count;lastPosition=0;queue.Start(count,ReadImage);}
void CdStreamInitThread(){}
void CdStreamShutdown(){queue.Stop();channelCount=0;CdStreamRemoveImages();}
int CdStreamRead(int channel,void *buffer,unsigned offset,unsigned size){int image=int(offset>>24);if(!buffer || channel<0 || channel>=channelCount || image>=imageCount)return STREAM_ERROR;lastPosition=int(offset+size);return queue.Submit(channel,image,buffer,uint64_t(offset&0xffffff)*CDSTREAM_SECTOR_SIZE,size_t(size)*CDSTREAM_SECTOR_SIZE);}
int CdStreamGetStatus(int channel){return channel>=0 && channel<channelCount?queue.Status(channel):STREAM_ERROR;}
int CdStreamGetLastPosn(){return lastPosition;}
int CdStreamSync(int channel){if(channel<0 || channel>=channelCount)return STREAM_ERROR;bool discard=false;
#ifdef FLUSHABLE_STREAMING
discard=flushStream[channel];flushStream[channel]=false;
#endif
return queue.Sync(channel,discard);}
uint32 GetGTA3ImgSize(){return imageCount?uint32(SDL_GetIOSize(images[0])):0;}
bool CdStreamAddImage(const char *path){
  if(!path || imageCount>=MAX_CDIMAGES)return false;char model[1024],resolved[1024];path=ModelSets::ResolveAssetPath(path,model,sizeof(model));
  if(!platform::resolveGamePathCaseInsensitive(resolved,sizeof(resolved),path))return false;
  auto stream=SDL_IOFromFile(resolved,"rb");if(!stream)return false;
  images[imageCount]=stream;std::strncpy(names[imageCount],resolved,1023);++imageCount;return true;
}
char *CdStreamGetImageName(int index){return index>=0 && index<imageCount?names[index]:nullptr;}
void CdStreamRemoveImages(){for(int i=0;i<channelCount;++i)queue.Sync(i);for(auto &stream:images)if(stream){SDL_CloseIO(stream);stream=nullptr;}imageCount=0;}
int32 CdStreamGetNumImages(){return imageCount;}
void AddToQueue(Queue *q,int item){q->items[q->tail]=item;q->tail=(q->tail+1)%q->size;}
int32 GetFirstInQueue(Queue *q){return q->head==q->tail?-1:q->items[q->head];}
void RemoveFirstInQueue(Queue *q){if(q->head!=q->tail)q->head=(q->head+1)%q->size;}
}
extern "C" bool CdStreamAddModelSetImages(const char *basePath) {
    char resolved[1024];
    if(!CdStreamAddImage(ModelSets::ResolveAssetPath(basePath,resolved,sizeof(resolved)))) return false;
    char overlay[1024];
    if(ModelSets::IsModernActive() && ModelSets::GetModernAssetPath("models/gta3.img",overlay,sizeof(overlay)) && !CdStreamAddImage(overlay)) return false;
    if(ModelSets::IsXboxActive() && ModelSets::GetXboxAssetPath("models/gta3.img",overlay,sizeof(overlay)) && !CdStreamAddImage(overlay)) return false;
    return true;
}
