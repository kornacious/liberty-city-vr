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
    if(found==transitions.end()){
        if(mode!=WorldLodPolicy::Aggressive||IsLod(mi))return original;
        int end=mi->m_firstDamaged&&!mi->m_isDamaged?mi->m_firstDamaged:mi->m_numAtomics;
        if(mi->m_isBigBuilding&&end>2)end=2;
        const float raw=end>0&&end<=3?mi->m_lodDistances[end-1]:0;
        return std::isfinite(raw)&&raw>0?Min(raw,300.0f):original;
    }
    if(mode==WorldLodPolicy::Aggressive)return Min(found->second,300.0f);
    if(mode==WorldLodPolicy::Performance)return found->second;
    const CColModel *col=mi->GetColModel();
    const float radius=col?col->boundingSphere.radius:0;
    return Max(original,CDraw::GetFarClipZ()+Max(radius,0.0f));
}
float NearDistance(CSimpleModelInfo *mi){
    if(mode!=WorldLodPolicy::Performance&&mode!=WorldLodPolicy::Aggressive)return mi->GetNearDistance();
    const float raw=mi->m_lodDistances[2];
    return std::isfinite(raw)&&raw>=0?Min(raw,mode==WorldLodPolicy::Aggressive?300.0f:LOD_DISTANCE):mi->GetNearDistance();
}
float ScanRadius(float normalRadius){
    return mode==WorldLodPolicy::Off?Max(normalRadius,CDraw::GetFarClipZ()+STREAM_DISTANCE):normalRadius;
}
Transition GetTransition(CSimpleModelInfo *mi,float distance){
    Transition out;
    if(mode!=WorldLodPolicy::Aggressive||!std::isfinite(distance))return out;
    const int first=mi->m_isDamaged?mi->m_firstDamaged:0;
    int end=mi->m_firstDamaged&&!mi->m_isDamaged?mi->m_firstDamaged:mi->m_numAtomics;
    if(mi->m_isBigBuilding&&end>2)end=2;
    if(first<0||end>3||first>=end)return out;
    const float limit=LargestDistance(mi);
    if(!std::isfinite(limit)||limit<=0)return out;
    // Fade over a 20 m transition band. Distance-only
    // weights reverse immediately and smoothly when travelling back inwards.
    const float band=20.0f;
    const float farWeight=Max(0.0f,Min(1.0f,(limit+band-distance)/band));
    float nearWeight=1.0f;
    if(IsLod(mi)){
        CSimpleModelInfo *high=mi->m_isBigBuilding?mi->GetRelatedModel():nil;
        // Retain low-detail coverage while its detailed replacement streams.
        // Model alpha also prevents an abrupt handoff as residency becomes ready.
        float ready=0;
        if(high&&high->GetRwObject())ready=high->m_alpha/255.0f;
        if(high&&high->GetModelType()==MITYPE_TIME){
            const int other=((CTimeModelInfo*)high)->GetOtherTimeModel();
            CBaseModelInfo *base=other>=0?CModelInfo::GetModelInfo(other):nil;
            if(base&&base->IsSimple()){
                auto *alternate=(CSimpleModelInfo*)base;
                if(alternate->GetRwObject())ready=Max(ready,alternate->m_alpha/255.0f);
            }
        }
        const float t=Max(0.0f,Min(1.0f,(distance-NearDistance(mi))/band));
        nearWeight=high?1.0f-(1.0f-t)*ready:t;
    }
    const float envelope=farWeight*nearWeight;
    if(envelope<=0)return out;
    auto add=[&](int slot,float weight){
        if(weight>0&&mi->m_atomics[slot]&&out.count<2){
            out.layers[out.count++]={mi->m_atomics[slot],weight*envelope};
        }
    };
    for(int i=first;i<end-1;++i){
        const float cutoff=mi->m_lodDistances[i];
        if(!std::isfinite(cutoff)||cutoff<=0)continue;
        float width=Min(band,Max(0.0f,limit-cutoff));
        if(i+1<end-1)width=Min(width,Max(0.0f,mi->m_lodDistances[i+1]-cutoff));
        if(distance<cutoff){add(i,1);break;}
        if(width>0&&distance<cutoff+width){
            const float t=(distance-cutoff)/width;
            // If one variant is unavailable keep the resident one solid.
            if(!mi->m_atomics[i])add(i+1,1);
            else if(!mi->m_atomics[i+1])add(i,1);
            else {add(i,1-t);add(i+1,t);}
            break;
        }
        if(i==end-2)add(end-1,1);
    }
    if(end==first+1)add(first,1);
    out.fading=out.count>1||(out.count==1&&out.layers[0].weight<1.0f);
    if(!out.count)++stats.missingSelections;
    else if(IsLod(mi))++stats.lodSelections;
    else if(out.layers[0].atomic==mi->m_atomics[first])++stats.highSelections;
    else ++stats.lowerSelections;
    return out;
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
