#include "common.h"
#include "VrWorldLod.h"
#include "ModelInfo.h"
#include "Camera.h"
#include "Draw.h"
#include "ColModel.h"
#include "Renderer.h"
#include <unordered_map>
#include <cmath>

namespace VrWorldLod {
static int mode=WorldLodPolicy::On;
static Stats stats;
static bool linked=false;
static std::unordered_map<CSimpleModelInfo*,float> transitions;
static unsigned unpaired=0;
static bool IsLod(CSimpleModelInfo *mi){
    const char *name=mi->GetModelName();
    return (name[0]=='L'||name[0]=='l')&&(name[1]=='O'||name[1]=='o')&&(name[2]=='D'||name[2]=='d');
}
void Invalidate(){linked=false;transitions.clear();unpaired=0;}
void SetMode(int value){mode=WorldLodPolicy::ClampMode(value);}
void BeginFrame(int value){SetMode(value);stats={};}
int Mode(){return mode;}
static void Add(CSimpleModelInfo *mi,float cutoff){
    auto found=transitions.find(mi);
    if(found==transitions.end())transitions.emplace(mi,cutoff);
    else if(cutoff<found->second)found->second=cutoff;
}
static void Link(){
    if(linked)return;
    linked=true;transitions.clear();unpaired=0;
    // Definitions and related-model links are stable after map loading.
    // Geometry residency can change independently and is checked on selection.
    for(int i=0;i<MODELINFOSIZE;++i){
        CBaseModelInfo *base=CModelInfo::GetModelInfo(i);
        if(!base||!base->IsSimple())continue;
        auto *lod=(CSimpleModelInfo*)base;
        if(!IsLod(lod))continue;
        CSimpleModelInfo *high=lod->m_isBigBuilding?lod->GetRelatedModel():nil;
        if(!high||IsLod(high)){++unpaired;continue;}
        const float raw=lod->m_lodDistances[2];
        if(!std::isfinite(raw)||raw<=0){++unpaired;continue;}
        const float cutoff=Min(raw,LOD_DISTANCE);
        Add(high,cutoff);
        if(high->GetModelType()==MITYPE_TIME){
            const int other=((CTimeModelInfo*)high)->GetOtherTimeModel();
            CBaseModelInfo *alternate=other>=0?CModelInfo::GetModelInfo(other):nil;
            if(alternate&&alternate->IsSimple())Add((CSimpleModelInfo*)alternate,cutoff);
        }
    }
}
Stats GetStats(){
    Link();Stats out=stats;out.pairedModels=unsigned(transitions.size());out.unpairedLodModels=unpaired;return out;
}
bool Suppress(CSimpleModelInfo *mi){
    if(mode!=WorldLodPolicy::Off||!IsLod(mi))return false;
    Link();++stats.suppressedLod;return true;
}
float LargestDistance(CSimpleModelInfo *mi){
    const float original=mi->GetLargestLodDistance();
    if(mode==WorldLodPolicy::On)return original;
    Link();auto found=transitions.find(mi);
    if(found==transitions.end())return original;
    if(mode==WorldLodPolicy::Performance)return found->second;
    const CColModel *col=mi->GetColModel();
    const float radius=col?col->boundingSphere.radius:0;
    return Max(original,CDraw::GetFarClipZ()+Max(radius,0.0f));
}
float NearDistance(CSimpleModelInfo *mi){
    if(mode!=WorldLodPolicy::Performance)return mi->GetNearDistance();
    const float raw=mi->m_lodDistances[2];
    return std::isfinite(raw)&&raw>=0?Min(raw,LOD_DISTANCE):mi->GetNearDistance();
}
float ScanRadius(float normalRadius){
    return mode==WorldLodPolicy::Off?Max(normalRadius,CDraw::GetFarClipZ()+STREAM_DISTANCE):normalRadius;
}
RpAtomic *SelectAtomic(CSimpleModelInfo *mi,float distance){
    if(mode==WorldLodPolicy::On){
        RpAtomic *a=mi->GetAtomicFromDistance(distance);
        if(!a)++stats.missingSelections;
        else if(IsLod(mi))++stats.lodSelections;
        else if(a==mi->m_atomics[mi->m_isDamaged?mi->m_firstDamaged:0])++stats.highSelections;
        else ++stats.lowerSelections;
        return a;
    }
    if(Suppress(mi))return nil;
    const int first=mi->m_isDamaged?mi->m_firstDamaged:0;
    int end=mi->m_firstDamaged&&!mi->m_isDamaged?mi->m_firstDamaged:mi->m_numAtomics;
    if(mi->m_isBigBuilding && end>2)end=2; // slot 2 stores related-model data
    float thresholds[3]={};bool loaded[3]={};
    for(int i=0;i<end&&i<3;++i){thresholds[i]=mi->m_lodDistances[i];loaded[i]=mi->m_atomics[i]!=nil;}
    const int slot=WorldLodPolicy::Select(mode,distance,LargestDistance(mi),thresholds,loaded,first,end);
    if(slot<0){++stats.missingSelections;return nil;}
    if(IsLod(mi))++stats.lodSelections;
    else if(slot==first)++stats.highSelections;
    else ++stats.lowerSelections;
    return mi->m_atomics[slot];
}
}
