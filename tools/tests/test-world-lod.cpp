#define _CRT_SECURE_NO_WARNINGS
#include "../../overlay/src/renderer/WorldLodPolicy.h"
#include <cassert>
#include <cstring>
#include <unordered_map>
#include <cmath>
#include <algorithm>
using uint32=uint32_t;
constexpr int MODELINFOSIZE=16,MITYPE_SIMPLE=1,MITYPE_TIME=3;
constexpr float LOD_DISTANCE=300,STREAM_DISTANCE=20;
#define nil nullptr
template<class T> T Min(T a,T b){return std::min(a,b);}
template<class T> T Max(T a,T b){return std::max(a,b);}
struct RpAtomic {int id;};
struct CColModel {struct {float radius=20;} boundingSphere;};
struct CBaseModelInfo {
    int type=MITYPE_SIMPLE;char name[24]="detail";
    bool IsSimple(){return type==MITYPE_SIMPLE||type==MITYPE_TIME;}
    int GetModelType(){return type;}
    char *GetModelName(){return name;}
};
struct CSimpleModelInfo:CBaseModelInfo {
    bool m_isDamaged=false,m_isBigBuilding=false;
    int m_alpha=255;
    void *GetRwObject(){return m_atomics[0];}
    int m_firstDamaged=0,m_numAtomics=2,originalCalls=0;
    float m_lodDistances[3]={50,100,0};
    RpAtomic *m_atomics[3]={};CSimpleModelInfo *related=nullptr;CColModel col;
    CSimpleModelInfo *GetRelatedModel(){return related;}
    CColModel *GetColModel(){return &col;}
    float GetLargestLodDistance(){return m_lodDistances[m_isDamaged||!m_firstDamaged?m_numAtomics-1:m_firstDamaged-1]*2.2f;}
    float GetNearDistance(){return m_lodDistances[2]*2.2f;}
    RpAtomic *GetAtomicFromDistance(float dist){
        ++originalCalls;
        for(int i=m_isDamaged?m_firstDamaged:0;i<m_numAtomics;++i)
            if(dist<m_lodDistances[i]*2.2f)return m_atomics[i];
        return nullptr;
    }
};
struct CTimeModelInfo:CSimpleModelInfo {int other=-1;int GetOtherTimeModel(){return other;}};
struct CModelInfo {static CBaseModelInfo *models[MODELINFOSIZE];static CBaseModelInfo *GetModelInfo(int id){return models[id];}};
CBaseModelInfo *CModelInfo::models[MODELINFOSIZE]={};
struct CDraw {static float GetFarClipZ(){return 800;}};
namespace VrWorldLod {
using Stats=WorldLodPolicy::Stats;
void Invalidate();void BeginFrame(int);void SetMode(int);int Mode();Stats GetStats();
bool Suppress(CSimpleModelInfo*);float LargestDistance(CSimpleModelInfo*);
float NearDistance(CSimpleModelInfo*);float ScanRadius(float);
struct Layer { RpAtomic *atomic=nullptr; float weight=0; };
struct Transition { Layer layers[2]; int count=0; bool fading=false; };
Transition GetTransition(CSimpleModelInfo*,float);
RpAtomic *SelectAtomic(CSimpleModelInfo*,float);
}
#include "world-lod-production.inc"
int main(){
    RpAtomic high{0},low{1},damage{2};
    CSimpleModelInfo model,lod,root,solo;
    model.m_atomics[0]=&high;model.m_atomics[1]=&low;
    lod.m_numAtomics=1;lod.m_atomics[0]=&low;lod.m_isBigBuilding=true;
    lod.related=&model;lod.m_lodDistances[0]=1000;lod.m_lodDistances[2]=100;
    std::strcpy(lod.name,"LODdetail");std::strcpy(root.name,"LODroot");
    root.m_isBigBuilding=true;root.m_numAtomics=1;root.m_atomics[0]=&low;root.m_lodDistances[0]=1000;
    solo.m_numAtomics=1;solo.m_atomics[0]=&high;solo.m_lodDistances[0]=100;
    CModelInfo::models[0]=&model;CModelInfo::models[1]=&lod;CModelInfo::models[2]=&root;CModelInfo::models[3]=&solo;
    VrWorldLod::Invalidate();VrWorldLod::BeginFrame(WorldLodPolicy::On);
    assert(VrWorldLod::SelectAtomic(&model,80)==&high);
    assert(VrWorldLod::SelectAtomic(&model,150)==&low);
    assert(model.originalCalls==2 && VrWorldLod::NearDistance(&lod)==220);
    assert(VrWorldLod::ScanRadius(300)==300);
    VrWorldLod::BeginFrame(WorldLodPolicy::Performance);
    assert(VrWorldLod::SelectAtomic(&model,40)==&high);
    assert(VrWorldLod::SelectAtomic(&model,80)==&low);
    assert(VrWorldLod::SelectAtomic(&model,100)==nullptr);
    assert(VrWorldLod::NearDistance(&lod)==100);
    assert(VrWorldLod::SelectAtomic(&lod,800)==&low); // distant coverage retained
    assert(VrWorldLod::SelectAtomic(&solo,150)==&high); // no partner: do not create a gap
    assert(VrWorldLod::ScanRadius(300)==300);
    VrWorldLod::BeginFrame(WorldLodPolicy::Aggressive);
    assert(VrWorldLod::LargestDistance(&solo)==100);
    auto plan=VrWorldLod::GetTransition(&solo,110);
    assert(plan.count==1 && plan.fading && plan.layers[0].weight==0.5f);
    assert(VrWorldLod::GetTransition(&solo,120).count==0);
    assert(VrWorldLod::GetTransition(&solo,99).layers[0].weight==1);
    plan=VrWorldLod::GetTransition(&model,60);
    assert(plan.count==2&&plan.layers[0].atomic==&high&&plan.layers[1].atomic==&low);
    assert(plan.layers[0].weight==0.5f&&plan.layers[1].weight==0.5f);
    plan=VrWorldLod::GetTransition(&lod,110);
    assert(plan.count==1&&plan.layers[0].weight==0.5f);
    assert(VrWorldLod::GetTransition(&lod,100).count==0);
    model.m_alpha=0;
    assert(VrWorldLod::GetTransition(&lod,90).layers[0].weight==1); // residency fallback
    model.m_alpha=255;
    for(float d=0;d<140;d+=0.25f){
        const auto a=VrWorldLod::GetTransition(&solo,d);
        const auto b=VrWorldLod::GetTransition(&solo,d+0.25f);
        const float wa=a.count?a.layers[0].weight:0,wb=b.count?b.layers[0].weight:0;
        assert(wa>=wb&&wa-wb<=0.013f); // reversible continuous distance fade
    }
    model.m_atomics[1]=nullptr;
    plan=VrWorldLod::GetTransition(&model,60);
    assert(plan.count==1&&plan.layers[0].weight==1); // no half-transparent missing counterpart
    model.m_atomics[1]=&low;
    assert(VrWorldLod::GetTransition(&solo,NAN).count==0);
    VrWorldLod::BeginFrame(WorldLodPolicy::Off);
    assert(VrWorldLod::SelectAtomic(&model,700)==&high);
    assert(VrWorldLod::LargestDistance(&model)==820);
    assert(VrWorldLod::SelectAtomic(&lod,10)==nullptr && VrWorldLod::Suppress(&root));
    assert(VrWorldLod::ScanRadius(300)==820);
    model.m_atomics[0]=nullptr;
    assert(VrWorldLod::SelectAtomic(&model,20)==nullptr); // never silently choose low geometry
    model.m_atomics[0]=&high;
    model.m_firstDamaged=1;model.m_isDamaged=true;model.m_atomics[1]=&damage;
    assert(VrWorldLod::SelectAtomic(&model,20)==&damage);
    model.m_isDamaged=false;assert(VrWorldLod::SelectAtomic(&model,150)==&high);
    VrWorldLod::BeginFrame(WorldLodPolicy::Performance);
    assert(VrWorldLod::SelectAtomic(&model,80)==&high); // damaged slot cannot be a low-detail slot
    CTimeModelInfo day,night;day.type=night.type=MITYPE_TIME;day.other=5;
    CModelInfo::models[4]=&day;CModelInfo::models[5]=&night;lod.related=&day;
    day.m_atomics[0]=night.m_atomics[0]=&high;night.m_lodDistances[1]=300;
    VrWorldLod::Invalidate();
    assert(VrWorldLod::LargestDistance(&day)==100 && VrWorldLod::LargestDistance(&night)==100);
    const auto stats=VrWorldLod::GetStats();assert(stats.pairedModels==2 && stats.unpairedLodModels==1);
    VrWorldLod::BeginFrame(WorldLodPolicy::Aggressive);
    day.m_atomics[0]=nullptr;night.m_alpha=255;
    assert(VrWorldLod::GetTransition(&lod,110).layers[0].weight==0.5f); // alternate time residency
    solo.m_lodDistances[0]=1000;
    assert(VrWorldLod::LargestDistance(&solo)==300);
    assert(VrWorldLod::GetTransition(&solo,310).layers[0].weight==0.5f);
    model.m_isDamaged=true;model.m_firstDamaged=1;
    auto damaged=VrWorldLod::GetTransition(&model,30);
    assert(damaged.count==1&&damaged.layers[0].atomic==&damage);
    model.m_isDamaged=false;
    VrWorldLod::BeginFrame(999);assert(VrWorldLod::Mode()==WorldLodPolicy::On);
    bool loaded[3]={true,true,true};float thresholds[3]={50,100,200};
    assert(WorldLodPolicy::Select(WorldLodPolicy::Off,NAN,200,thresholds,loaded,0,2)==-1);
    assert(WorldLodPolicy::Select(WorldLodPolicy::Off,0,200,thresholds,loaded,2,2)==-1);
    assert(WorldLodPolicy::Select(WorldLodPolicy::Performance,0,200,thresholds,loaded,0,4)==-1);
}
