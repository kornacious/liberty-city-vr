#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
using std::isfinite;
using int32=int;
using float32=float;
using bool32=int;
#define nil nullptr
#define PUSH_RENDERGROUP(name) ((void)0)
#define POP_RENDERGROUP() ((void)0)

struct CVector {
    float x=0,y=0,z=0;
    CVector()=default;
    CVector(float a,float b,float c):x(a),y(b),z(c){}
};
struct Matrix {
    CVector translation;
    CVector operator*(const CVector &v) const {
        return {v.x+translation.x,v.y+translation.y,v.z+translation.z};
    }
};
struct CColModel { struct {CVector min,max;} boundingBox; };
static CColModel normalRoad={{{-.05f,-.05f,-.05f},{.05f,.05f,.05f}}};
static CColModel longRoad={{{-150,-1,-1},{150,1,1}}};
struct ModelInfo {
    CColModel *col;
    CColModel *GetColModel(){return col;}
};
struct CModelInfo {
    static ModelInfo *GetModelInfo(int index){
        static ModelInfo models[]={{&normalRoad},{&longRoad},{nullptr}};
        return &models[index];
    }
    static CColModel *GetColModel(int index){return GetModelInfo(index)->col;}
};
struct CEntity {
    void *m_rwObject=this;
    bool bIsBIGBuilding=false;
    int model=0;
    Matrix matrix;
    bool IsBuilding()const{return true;}
    float GetBoundRadius()const{return 1;}
    CVector GetBoundCentre()const{return matrix.translation;}
    int GetModelIndex()const{return model;}
    const Matrix &GetMatrix()const{return matrix;}
    bool GetIsOnScreen()const{return true;}
    bool GetIsOnScreenComplex()const{return true;}
};
static int submitted=0,profiled=0,collisionDraws=0;
static bool gbDontRenderBuildings=false,gbShowCollisionPolys=false;
namespace GameRender { void SubmitEntity(CEntity *){++submitted;} }
namespace androidgame { void QuestProfilerCountVisibleBuilding(CEntity *){++profiled;} }
struct CCollision {
    static void DrawColModel_Coloured(const Matrix &,const CColModel &,int){++collisionDraws;}
};
enum { VR_OCCLUSION_CULLING_OFF,VR_OCCLUSION_CULLING_SAFE,
       VR_OCCLUSION_CULLING_AUTHORED,VR_OCCLUSION_CULLING_AGGRESSIVE };
namespace rw { namespace vulkan {
struct {
    bool firstPersonActive=true,stereoCullPlanesValid=true;
    float worldToPlay[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    float fpHeadWorld[3]={};
    float stereoCullPlanes[2][5][4]={
        {{1,0,0,4},{-1,0,0,1},{0,1,0,2},{0,-1,0,2},{0,0,-1,-.1f}},
        {{1,0,0,1},{-1,0,0,4},{0,1,0,2},{0,-1,0,2},{0,0,-1,-.1f}}
    };
}gvk;
#define ROAD_CULLING_BACKEND
#include "road-culling-production.inc"
#undef ROAD_CULLING_BACKEND
} }
namespace vr {
struct CRenderer {
    static inline bool ms_bVrPerViewCulling=true;
    static inline int ms_nVrOcclusionCullingMode=VR_OCCLUSION_CULLING_SAFE;
    static bool ShouldRenderInCurrentView(CEntity *);
    static void RenderOneRoad(CEntity *);
};
#define RW_VULKAN
#define __ANDROID__
#define ROAD_CULLING_HELPER
// The existing Vulkan helper always returns before its mono fallback.
#pragma warning(push)
#pragma warning(disable:4702)
#include "road-culling-production.inc"
#pragma warning(pop)
#undef ROAD_CULLING_HELPER
#undef __ANDROID__
#undef RW_VULKAN
}
namespace shipping {
struct CRenderer : vr::CRenderer { static void RenderOneRoad(CEntity *); };
#define MASTER
#define RW_VULKAN
#include "road-culling-production.inc"
#undef RW_VULKAN
#undef MASTER
}
namespace mono {
struct CRenderer { static void RenderOneRoad(CEntity *); };
#include "road-culling-production.inc"
}
static void Expect(CEntity *road,bool visible){
    submitted=profiled=0;
    vr::CRenderer::RenderOneRoad(road);
    assert(submitted==int(visible)&&profiled==int(visible));
    submitted=0;
    shipping::CRenderer::RenderOneRoad(road);
    assert(submitted==int(visible));
}
int main(){
    using vr::CRenderer;
    auto &gpu=rw::vulkan::gvk;
    CEntity road;
    road.matrix.translation={0,0,-5};Expect(&road,true);
    // Reject only when neither eye sees any of the road's bounds.
    for(int mode=VR_OCCLUSION_CULLING_SAFE;mode<=VR_OCCLUSION_CULLING_AGGRESSIVE;++mode){
        CRenderer::ms_nVrOcclusionCullingMode=mode;
        road.matrix.translation={-3,0,-5};Expect(&road,true); // left only
        road.matrix.translation={3,0,-5};Expect(&road,true);  // right only
        road.matrix.translation={100,0,-5};Expect(&road,false);
        road.model=1;Expect(&road,true); // long road intersects despite offscreen centre
        road.model=0;
    }
    road.matrix.translation={0,0,10};Expect(&road,false);
    gpu.worldToPlay[0]=gpu.worldToPlay[10]=-1;Expect(&road,true); // head turn, same entity/list
    gpu.worldToPlay[0]=gpu.worldToPlay[10]=1;
    road.matrix.translation={100,0,-5};
    CRenderer::ms_nVrOcclusionCullingMode=VR_OCCLUSION_CULLING_OFF;Expect(&road,true);
    CRenderer::ms_nVrOcclusionCullingMode=VR_OCCLUSION_CULLING_SAFE;
    CRenderer::ms_bVrPerViewCulling=false;Expect(&road,true);
    CRenderer::ms_bVrPerViewCulling=true;
    gpu.stereoCullPlanesValid=false;Expect(&road,true);gpu.stereoCullPlanesValid=true;
    gpu.firstPersonActive=false;Expect(&road,true);gpu.firstPersonActive=true;
    road.matrix.translation.x=std::numeric_limits<float>::quiet_NaN();Expect(&road,true);
    road.matrix.translation={100,0,-5};road.model=2;Expect(&road,false); // sphere fallback
    road.bIsBIGBuilding=true;Expect(&road,true);road.bIsBIGBuilding=false;road.model=0;
    road.m_rwObject=nullptr;Expect(&road,false);Expect(nullptr,false);road.m_rwObject=&road;
    // Mono retains its previous draw behavior; no tracked-eye rejection.
    submitted=0;mono::CRenderer::RenderOneRoad(&road);assert(submitted==1);
    // Debug collision drawing and visible-building accounting also obey culling.
    gbShowCollisionPolys=true;collisionDraws=profiled=0;
    vr::CRenderer::RenderOneRoad(&road);assert(collisionDraws==0&&profiled==0);
    road.matrix.translation={0,0,-5};
    vr::CRenderer::RenderOneRoad(&road);assert(collisionDraws==1&&profiled==1);
    gbShowCollisionPolys=false;gbDontRenderBuildings=true;
    submitted=profiled=0;vr::CRenderer::RenderOneRoad(&road);assert(submitted==0&&profiled==0);
    std::puts("PASS: production road submission + renderer visibility + stereo box/sphere tests; either eye, offscreen, large bounds, head turns, invalid tracking, disabled culling, null objects, shipping/mono and debug accounting.");
}
