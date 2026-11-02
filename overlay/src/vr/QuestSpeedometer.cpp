#include "common.h"
#if defined(GTA_VR_WEAPONS) && defined(__ANDROID__)
#include "QuestSpeedometer.h"
#include "Speedometer.h"
#include "VehiclePlacement.h"
#include "ModelSets.h"
#include "OculusVR.h"
#include "android.h"
#include "platform_android.h"
#include "Vehicle.h"
#include "HandlingMgr.h"
#include "PlayerPed.h"
#include "PlayerInfo.h"
#include "Frontend.h"
#include "CutsceneMgr.h"
#include "Camera.h"
#include "GameRenderCommand.h"

extern bool gVrFirstPersonActive;
extern CVehicle *FindPlayerVehicle(void);
namespace QuestSpeedometer {
namespace {
bool loaded=false, visible=true, analog=false, metric=false;
char notice[96]="SHARED BOTTOM-CENTRE PIVOT / CURRENT MODEL AND MODELSET";
const char *keys[]={"SpeedometerX","SpeedometerY","SpeedometerZ","SpeedometerPitch","SpeedometerYaw","SpeedometerRoll","SpeedometerSize"};
const char *labels[]={"RIGHT","FORWARD","UP","PITCH","YAW","ROLL","SIZE"};
const char *settings=".\\vr_settings.ini";
void Load() {
    if(loaded) return;
    visible=GetPrivateProfileIntA("VR","SpeedometerVisible",1,settings)!=0;
    analog=GetPrivateProfileIntA("VR","SpeedometerAnalog",0,settings)!=0;
    metric=GetPrivateProfileIntA("VR","SpeedometerMetric",0,settings)!=0;
    loaded=true;
}
int Set() { return (int)ModelSets::GetCalibrationSetForCategory(ModelSets::MODEL_CATEGORY_VEHICLES); }
CVehicle *Vehicle() {
    CVehicle *v=FindPlayerVehicle();
    return v && v->pDriver==FindPlayerPed() && v->IsCar() && !v->IsRealHeli() && !v->IsRealPlane()?v:nil;
}
VehiclePlacement::Values Values(CVehicle *v) {
    auto &store=VehiclePlacement::runtime();
    auto values=VehiclePlacement::legacyDefaults(Set(),v->GetModelIndex(),false);
    return VehiclePlacement::resolve(store,Set(),v->GetModelIndex(),values);
}
void Toggle(const char *key,bool &value) {
    if(WritePrivateProfileStringA("VR",key,value?"0":"1",settings)) value=!value;
    else std::snprintf(notice,sizeof(notice),"SETTING SAVE FAILED");
}
}
const char *Status() { return notice; }
void MenuRow(int row,char *text,int length) {
    Load(); CVehicle *v=Vehicle();
    if(row==0) std::snprintf(text,length,"SPEEDOMETER  < %s >",visible?"ON":"OFF");
    else if(row==1) std::snprintf(text,length,"STYLE  < %s >",analog?"CLASSIC ANALOG":"DIGITAL POD");
    else if(row==2) std::snprintf(text,length,"SPEED UNITS  < %s >",metric?"KM/H":"MPH");
    else if(row>=FirstPlacementRow && row<=LastPlacementRow) {
        int i=row-FirstPlacementRow;
        if(v) std::snprintf(text,length,"%s  < %+.1f %s >",labels[i],Values(v)[keys[i]],i<3?"CM":i<6?"DEG":"%");
        else std::snprintf(text,length,"%s  < ENTER A CAR >",labels[i]);
    } else std::snprintf(text,length,"%s",row==10?"RESET MODEL PLACEMENT TO INHERITED":"BACK");
}
void MenuAction(int row,int direction) {
    Load();
    if(row==0) Toggle("SpeedometerVisible",visible);
    else if(row==1) Toggle("SpeedometerAnalog",analog);
    else if(row==2) Toggle("SpeedometerMetric",metric);
    else if(row>=FirstPlacementRow && row<=10) {
        CVehicle *v=Vehicle(); if(!v) { std::snprintf(notice,sizeof(notice),"ENTER A CAR TO EDIT PLACEMENT"); return; }
        auto &store=VehiclePlacement::runtime(); auto next=store.personal;
        auto &entry=next.entries[VehiclePlacement::section(Set(),v->GetModelIndex())];
        if(row==10) for(auto key:keys) entry.erase(key);
        else {
            int i=row-FirstPlacementRow; float lo,hi;
            VehiclePlacement::limits(keys[i],lo,hi);
            entry[keys[i]]=std::max(lo,std::min(hi,Values(v)[keys[i]]+direction*(i<3?.5f:i<6?1.f:5.f)));
        }
        std::string error;
        if(next.save(VehiclePlacement::personalPath(),error)) {
            store.personal=next;
            std::snprintf(notice,sizeof(notice),"%s",row==10?"MODEL PLACEMENT RESET":"MODEL / MODELSET PLACEMENT SAVED");
        } else std::snprintf(notice,sizeof(notice),"PLACEMENT SAVE FAILED");
    }
}
void Render() {
    Load(); CVehicle *v=Vehicle(); CMatrix anchor;
    if(!visible || !v || !gVrFirstPersonActive || !OculusVR::IsImmersiveCarDrivingActive() ||
       OculusVR::IsQuestVehicleThirdPerson() || FindPlayerPed()->DyingOrDead() ||
       CCutsceneMgr::IsRunning() || CCutsceneMgr::IsCutsceneProcessing() ||
       androidgame::VrCutsceneCameraActor()!=nil || FrontEndMenuManager.m_bMenuActive ||
       FrontEndMenuManager.m_bGameNotLoaded || TheCamera.m_WideScreenOn ||
       !OculusVR::GetQuestVehicleHudAnchor(&anchor)) return;
    auto values=Values(v);
    VehiclePlacement::Basis parent;
    parent.position={anchor.GetPosition().x,anchor.GetPosition().y,anchor.GetPosition().z};
    parent.right={anchor.GetRight().x,anchor.GetRight().y,anchor.GetRight().z};
    parent.up={anchor.GetUp().x,anchor.GetUp().y,anchor.GetUp().z};
    parent.forward={anchor.GetForward().x,anchor.GetForward().y,anchor.GetForward().z};
    auto basis=VehiclePlacement::namedPanel(parent,values,"Speedometer");
    float scale=values["SpeedometerSize"]*.01f*values["DashSize"]*.01f;
    // Ground speed, matching the existing speed profiler; vertical jumps do
    // not inflate it, and reverse still reports a positive magnitude.
    CVector velocity=v->GetMoveSpeed();
    float speed=Speedometer::kmh(std::sqrt(velocity.x*velocity.x+velocity.y*velocity.y));
    float handling=v->pHandling?Speedometer::kmh(v->pHandling->Transmission.fMaxCruiseVelocity):180.f;
    static Speedometer::Mesh mesh;
    Speedometer::build(mesh,analog,metric,speed,Speedometer::dialMaximum(handling,metric));
    static std::vector<RwIm3DVertex> vertices; vertices.clear();
    static std::vector<RwImVertexIndex> indices; indices.clear();
    for(const auto &a:mesh.vertices) {
        auto p=basis.position+(basis.right*a.x+basis.up*a.y-basis.forward*a.z)*scale;
        RwIm3DVertex vertex{}; RwIm3DVertexSetPos(&vertex,p.x,p.y,p.z);
        RwIm3DVertexSetRGBA(&vertex,a.color.r,a.color.g,a.color.b,255); indices.push_back((RwImVertexIndex)vertices.size()); vertices.push_back(vertex);
    }
    const RwRenderState states[]={rwRENDERSTATETEXTURERASTER,rwRENDERSTATEVERTEXALPHAENABLE,rwRENDERSTATEZTESTENABLE,rwRENDERSTATEZWRITEENABLE,rwRENDERSTATECULLMODE};
    void *old[5]={}; for(int i=0;i<5;++i) RwRenderStateGet(states[i],&old[i]);
    RwRenderStateSet(states[0],nil); RwRenderStateSet(states[1],(void*)FALSE);
    RwRenderStateSet(states[2],(void*)TRUE); RwRenderStateSet(states[3],(void*)TRUE);
    RwRenderStateSet(states[4],(void*)rwCULLMODECULLNONE);
    if(GameRender::BeginImmediate3D(vertices.data(),vertices.size(),nil,rwIM3D_VERTEXXYZ|rwIM3D_VERTEXRGBA,GameRender::IMMEDIATE3D_DRAW_VR_HAND)) {
        GameRender::SubmitImmediate3DIndexed(rwPRIMTYPETRILIST,indices.data(),indices.size()); GameRender::EndImmediate3D();
    }
    for(int i=0;i<5;++i) RwRenderStateSet(states[i],old[i]);
}
} // namespace QuestSpeedometer
#endif
