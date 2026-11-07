#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#define nil nullptr
#define RW_VULKAN
#define LIBERTYVR_DEV_TOOLS 1
using int32 = int;
struct CVector {
    float x, y, z;
    CVector(float a=0, float b=0, float c=0): x(a), y(b), z(c) {}
};
struct CMatrix { CVector operator*(CVector p) const { return p; } };
struct CColModel { struct { CVector min, max; } boundingBox; };
static CColModel collision;
static bool hasCollision;
struct CModelInfo {
    static CModelInfo *GetModelInfo(int) { static CModelInfo model; return &model; }
    CColModel *GetColModel() { return hasCollision ? &collision : nil; }
};
struct CEntity {
    void *m_rwObject = this;
    bool building = false, bIsBIGBuilding = false;
    float radius = 5;
    float GetBoundRadius() { return radius; }
    CVector GetBoundCentre() { return {}; }
    bool IsBuilding() { return building; }
    int GetModelIndex() { return 0; }
    CMatrix GetMatrix() { return {}; }
    bool GetIsOnScreen() { return false; }
    bool GetIsOnScreenComplex() { return false; }
};
enum { VR_OCCLUSION_CULLING_OFF, VR_OCCLUSION_CULLING_SAFE,
       VR_OCCLUSION_CULLING_AUTHORED, VR_OCCLUSION_CULLING_AGGRESSIVE };
static bool testStereoVisible;
static int sphereCalls, boxCalls;
static float lastMargin;
namespace rw { namespace vulkan {
int isFirstPersonWorldBoxVisibleInStereo(float (*)[3], float margin) {
    ++boxCalls; lastMargin=margin; return testStereoVisible;
}
int isFirstPersonWorldSphereVisibleInStereo(const float *, float, float margin) {
    ++sphereCalls; lastMargin=margin; return testStereoVisible;
}
} }
struct CRenderer {
    static inline bool ms_bVrPerViewCulling;
    static inline int ms_nVrOcclusionCullingMode;
    static inline int ms_nNoOfVisibleEntities;
    static inline CEntity *ms_aVisibleEntityPtrs[2];
    static inline CVector ms_vecCameraPosition;
    static bool ShouldRenderInCurrentView(CEntity *);
#include "culling-visualizer-accessors.inc"
};
// The production Vulkan branch returns before the desktop fallback.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4702)
#endif
#include "culling-visualizer-predicate.inc"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
int main() {
    int checks=0;
    CEntity a, b;
    CRenderer::ms_nNoOfVisibleEntities=2;
    CRenderer::ms_aVisibleEntityPtrs[0]=&a;
    CRenderer::ms_aVisibleEntityPtrs[1]=&b;
    assert(CRenderer::GetVisibleEntityCount()==2);
    assert(CRenderer::GetVisibleEntity(0)==&a && CRenderer::GetVisibleEntity(1)==&b);
    assert(CRenderer::GetVisibleEntity(-1)==nil && CRenderer::GetVisibleEntity(2)==nil);
    CRenderer::ms_nNoOfVisibleEntities=0;
    assert(CRenderer::GetVisibleEntity(0)==nil);
    CRenderer::ms_vecCameraPosition={3,4,5};
    auto position=CRenderer::GetVrViewCameraPosition();
    assert(position.x==3 && position.y==4 && position.z==5);
    position.x=100;
    assert(CRenderer::GetVrViewCameraPosition().x==3);
    for (bool perView : {false,true})
    for (int mode : {0,1,2,3})
    for (bool nullEntity : {false,true})
    for (bool loaded : {false,true})
    for (bool building : {false,true})
    for (bool big : {false,true})
    for (bool col : {false,true})
    for (float radius : {0.0f,5.0f,5.01f})
    for (bool visible : {false,true}) {
        CEntity e;
        e.m_rwObject=loaded ? &a : nil;
        e.building=building; e.bIsBIGBuilding=big; e.radius=radius;
        CRenderer::ms_bVrPerViewCulling=perView;
        CRenderer::ms_nVrOcclusionCullingMode=mode;
        hasCollision=col; testStereoVisible=visible;
        CEntity *entity=nullEntity ? nil : &e;
        const bool bypass=!perView || (!nullEntity && loaded && mode==0);
        const bool smallMap=building && !col && big && radius<=5;
        const bool expected=bypass || (!nullEntity && loaded && (smallMap || visible));
        sphereCalls=boxCalls=0;
        const int reason=CRenderer::VrClassifyPerView(entity);
        assert(reason==(expected ? 0 : 1));
        const bool queries=perView && !nullEntity && loaded && mode!=0 && !smallMap;
        assert(sphereCalls+boxCalls==(queries ? 1 : 0));
        if (queries) {
            assert(boxCalls==(building && col ? 1 : 0));
            const float margin=mode==1 ? 0.14054083f : (mode==3 ? 0.05240778f : 0.08748866f);
            assert(std::fabs(lastMargin-margin)<1e-7f);
        }
        assert(CRenderer::ShouldRenderInCurrentView(entity)==expected);
        assert(e.building==building && e.bIsBIGBuilding==big && e.radius==radius);
        assert(CRenderer::ms_bVrPerViewCulling==perView && CRenderer::ms_nVrOcclusionCullingMode==mode);
        ++checks;
    }
    std::printf("PASS: %d production view/classifier cases; OFF and per-view gates, stereo box/sphere tests, small-map exception, accessor bounds and snapshot copy.\n",checks);
}
