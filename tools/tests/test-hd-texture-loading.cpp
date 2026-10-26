#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#define nil nullptr
namespace rw {
struct Texture { static Texture *(*findCB)(const char *); int id; };
struct TexDictionary {
 std::map<std::string,Texture*> textures;
 Texture *find(const char *name){auto i=textures.find(name);return i==textures.end()?nullptr:i->second;}
};
Texture *(*Texture::findCB)(const char *);
}
#include "texture-fallback-production.inc"
#include "vehicle-alpha-production.inc"
static rw::TexDictionary local;
static rw::Texture *LocalFind(const char *name){return local.find(name);}
static int checks;
static void check(bool value,const char *label){++checks;if(!value){fprintf(stderr,"FAIL %s\n",label);exit(1);}}
int main(){
 rw::Texture road{1},own{2},shared{3};rw::TexDictionary generic;
 generic.textures={{"grass_128hv",&road},{"own",&shared}};local.textures={{"own",&own}};
 rw::Texture::findCB=LocalFind;
 QuestTextureFallback::Install(&generic);
 check(rw::Texture::findCB("grass_128hv")==&road,"global generic lookup restores missing terrain texture");
 check(rw::Texture::findCB("own")==&own,"model dictionary takes precedence");
 check(rw::Texture::findCB("missing")==nullptr,"missing lookup remains missing");
 QuestTextureFallback::Install(&generic);
 check(rw::Texture::findCB("grass_128hv")==&road,"repeat install cannot recurse");
 QuestTextureFallback::Remove();
 check(rw::Texture::findCB==LocalFind,"shutdown restores previous callback");
 check(rw::Texture::findCB("grass_128hv")==nullptr,"shutdown cannot retain destroyed generic dictionary");
 QuestTextureFallback::Remove();
 check(rw::Texture::findCB==LocalFind,"repeat removal safe");
 using namespace rw::vulkan;
 for(int vehicle=0;vehicle<2;vehicle++)for(int blend=0;blend<2;blend++)for(int depth=0;depth<2;depth++)
  check(splitVehicleAlpha(vehicle!=0,blend!=0,depth!=0)==(vehicle&&blend&&depth),"depth split only targets writable blended vehicle materials");
 const float ref=vehicleOpaqueReference();
 check(1.0f>=ref&&0.5f<ref&&0.0f<ref,"opaque body and glass split into different passes");
 // Body near the viewer must prevent a later interior surface from replacing
 // it. Glass contributes no depth and still permits a hand behind it.
 float depth=100.0f;
 if(1.0f>=ref)depth=2.0f;
 check(!(4.0f<depth),"opaque body occludes interior");
 depth=100.0f;
 if(0.5f>=ref)depth=2.0f;
 check(4.0f<depth,"translucent window does not hide later hand");
 printf("PASS %d generic texture lookup/lifecycle and vehicle alpha depth checks\n",checks);
}
