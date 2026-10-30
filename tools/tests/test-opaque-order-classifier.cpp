// Execute the actual production classifier and actual game wrapper with small
// backend fixtures. No Vulkan driver, original game data or device is needed.
#include "vulkan/OpaqueOrder.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
using std::isfinite;
using uint32=uint32_t;
using bool32=int;
#define nil nullptr
constexpr int PLATFORM_VULKAN=1;
namespace rw {
struct V3 { float x=0,y=0,z=0; };
struct Matrix { V3 right{1,0,0},up{0,1,0},at{0,0,1},pos; };
struct Frame { Matrix matrix;Matrix *getLTM(){return &matrix;} };
struct Raster { enum {TEXTURE=4};int platform=PLATFORM_VULKAN,type=TEXTURE;Raster *parent=nullptr;bool alpha=false; };
struct Texture { enum {WRAP=1,MIRROR=2,CLAMP=3};Raster *raster=nullptr;uint32 u=WRAP,v=WRAP;uint32 getAddressU(){return u;}uint32 getAddressV(){return v;} };
struct Material { struct {int alpha=255;} color;rw::Texture *texture=nullptr;int effects=0; };
struct MatFX { enum {NOTHING=0};static int getEffects(Material *m){return m->effects;} };
struct MeshHeader {int serialNum=1;};
struct BaseInstance {int platform=PLATFORM_VULKAN;};
struct Geometry {enum {NATIVE=1,LIGHT=2};int numMorphTargets=1,lockedSinceInst=0,flags=0;MeshHeader *meshHeader=nullptr;BaseInstance *instData=nullptr;};
struct ObjPipeline {int platform=PLATFORM_VULKAN;};
struct Atomic {
 using RenderCB=void(*)(Atomic*);
 RenderCB renderCB=nullptr;Geometry *geometry=nullptr;ObjPipeline *pipeline=nullptr;Frame frame;
 static void defaultRenderCB(Atomic*){}
 ObjPipeline *getPipeline(){return pipeline;}
 Frame *getFrame(){return &frame;}
};
}
using namespace rw;
using RpAtomic=rw::Atomic;
static int pushes=0,pops=0,delegations=0;
#define PUSH_MEMID(x) (++pushes)
#define POP_MEMID() (++pops)
static RpAtomic *AtomicDefaultRenderCallBack(RpAtomic *a){++delegations;rw::Atomic::defaultRenderCB(a);return a;}
#include "opaque-order-wrapper.inc"
static volatile int unknownCalls=0;
static void unknownCallback(Atomic*){++unknownCalls;}
namespace fixture {
struct InstanceData {bool vertexAlpha=false;Material *material=nullptr;};
struct InstanceDataHeader:BaseInstance {bool orderBoundsValid=true;InstanceData *inst=nullptr;uint32 numMeshes=1;int serialNumber=1;float orderMin[3]{0,0,.2f},orderMax[3]{1,1,.3f};};
static void defaultInstanceCB(){}
static void defaultRenderCB(){}
struct ObjPipeline:rw::ObjPipeline {void(*instanceCB)()=defaultInstanceCB;void(*renderCB)()=defaultRenderCB;};
struct SceneData {float viewProj[2][16]{};} fixtureScene;
static SceneData *getSceneData(){return &fixtureScene;}
static struct {bool inFrame=true;float worldToPlay[16]{};} gvk;
static struct {bool zTestEnabled=true,zWriteEnabled=true;uint32 alphaTestRef=128;} gstate;
static bool gOpaqueOrderEnabled=true,dynamicObjectPass=false,vehicleAlphaPass=false;
static Atomic::RenderCB gOpaqueOrderDefaultWrapper=nullptr;
static OpaqueOrder::Stats gOpaqueOrderStats;
static bool rasterHasAlpha(Raster *r){return r->alpha;}
static void multiplyMatrix(float *out,const float *a,const float *b){
 for(int c=0;c<4;++c)for(int r=0;r<4;++r){out[c*4+r]=0;for(int k=0;k<4;++k)out[c*4+r]+=a[k*4+r]*b[c*4+k];}
}
#include "opaque-order-classifier.inc"
}
int main(){
 using namespace fixture;
 for(int i=0;i<4;++i){gvk.worldToPlay[i*5]=1;for(auto &eye:fixtureScene.viewProj)eye[i*5]=1;}
 fixture::ObjPipeline pipeline;Material material;Raster raster;rw::Texture texture;texture.raster=&raster;material.texture=&texture;
 InstanceData mesh;mesh.material=&material;InstanceDataHeader header;header.inst=&mesh;
 MeshHeader meshes;rw::Geometry geometry;geometry.meshHeader=&meshes;geometry.instData=&header;
 Atomic atomic;atomic.pipeline=&pipeline;atomic.geometry=&geometry;atomic.renderCB=Atomic::defaultRenderCB;
 bool staticBuilding=true;
 OpaqueOrder::Interval depth;
 assert(getOpaqueOrderInterval(&atomic,depth,staticBuilding));
 atomic.renderCB=(Atomic::RenderCB)DefaultRenderCB_pushid;
 assert(!getOpaqueOrderInterval(&atomic,depth,staticBuilding)); // Reproduce the old failure.
 assert(gOpaqueOrderStats.rejected[OpaqueOrder::Callback]==1);
 gOpaqueOrderDefaultWrapper=(Atomic::RenderCB)DefaultRenderCB_pushid;
 assert(getOpaqueOrderInterval(&atomic,depth,staticBuilding));
 assert(DefaultRenderCB_pushid(&atomic)==&atomic && pushes==1 && pops==1 && delegations==1);
 const auto unlitDepth=depth;
 geometry.flags=rw::Geometry::LIGHT;
 assert(getOpaqueOrderInterval(&atomic,depth,staticBuilding));
 assert(geometry.flags==rw::Geometry::LIGHT); // Classification must preserve lighting.
 for(int eye=0;eye<2;++eye){
  assert(depth.nearDepth[eye]==unlitDepth.nearDepth[eye]);
  assert(depth.farDepth[eye]==unlitDepth.farDepth[eye]);
 }
 struct Entry {OpaqueOrder::Interval depth;bool eligible;};
 Entry ordered[2];atomic.frame.matrix.pos.z=.5f;
 ordered[0].eligible=getOpaqueOrderInterval(&atomic,ordered[0].depth,staticBuilding)!=0;
 atomic.frame.matrix.pos.z=0;
 ordered[1].eligible=getOpaqueOrderInterval(&atomic,ordered[1].depth,staticBuilding)!=0;
 assert(OpaqueOrder::Sort(ordered,2)==1); // Real classifier output can reorder.
 auto reject=[&](OpaqueOrder::Rejection reason){
  auto previous=gOpaqueOrderStats.rejected[reason];
  assert(!getOpaqueOrderInterval(&atomic,depth,staticBuilding));
  assert(gOpaqueOrderStats.rejected[reason]==previous+1);
 };
 staticBuilding=false;reject(OpaqueOrder::StaticEntity);staticBuilding=true;
 dynamicObjectPass=true;reject(OpaqueOrder::PassState);dynamicObjectPass=false;
 vehicleAlphaPass=true;reject(OpaqueOrder::PassState);vehicleAlphaPass=false;
 atomic.renderCB=unknownCallback;reject(OpaqueOrder::Callback);atomic.renderCB=(Atomic::RenderCB)DefaultRenderCB_pushid;
 atomic.renderCB=nullptr;reject(OpaqueOrder::Callback);atomic.renderCB=(Atomic::RenderCB)DefaultRenderCB_pushid;
 gOpaqueOrderEnabled=false;reject(OpaqueOrder::Inactive);gOpaqueOrderEnabled=true;
 gstate.zWriteEnabled=false;reject(OpaqueOrder::PassState);gstate.zWriteEnabled=true;
 pipeline.platform=0;reject(OpaqueOrder::Pipeline);pipeline.platform=PLATFORM_VULKAN;
 geometry.lockedSinceInst=1;reject(OpaqueOrder::Geometry);geometry.lockedSinceInst=0;
 geometry.flags|=rw::Geometry::NATIVE;reject(OpaqueOrder::Geometry);geometry.flags=rw::Geometry::LIGHT;
 header.orderBoundsValid=false;reject(OpaqueOrder::Instance);header.orderBoundsValid=true;
 mesh.vertexAlpha=true;reject(OpaqueOrder::MeshAlpha);mesh.vertexAlpha=false;
 material.color.alpha=254;reject(OpaqueOrder::MeshAlpha);material.color.alpha=255;
 material.effects=1;reject(OpaqueOrder::MatFx);material.effects=0;
 raster.alpha=true;reject(OpaqueOrder::Texture);raster.alpha=false;
 texture.u=99;reject(OpaqueOrder::Address);texture.u=rw::Texture::WRAP;
 atomic.frame.matrix.pos.z=-1;reject(OpaqueOrder::Projection);atomic.frame.matrix.pos.z=0;
 fixtureScene.viewProj[1][15]=-1;reject(OpaqueOrder::Projection);fixtureScene.viewProj[1][15]=1;
 assert(getOpaqueOrderInterval(&atomic,depth,staticBuilding));
 assert(gOpaqueOrderStats.rejected[OpaqueOrder::Lighting]==0); // Legacy field retained.
 uint32 accounted=gOpaqueOrderStats.eligible;
 for(auto count:gOpaqueOrderStats.rejected)accounted+=count;
 assert(accounted==gOpaqueOrderStats.candidates);
 gOpaqueOrderStats={};assert(gOpaqueOrderStats.candidates==0);
 for(auto count:gOpaqueOrderStats.rejected)assert(count==0);
 std::puts("PASS: production classifier accepts lit static buildings, preserves lighting, produces swaps, rejects unsafe lit fixtures and accounts for every candidate.");
}
