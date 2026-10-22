#include "common.h"
#if defined(GTA_VR_WEAPONS) && defined(__ANDROID__)
#include "QuestRadio.h"
#include "RadioBoardLayout.h"
#include "QuestMusic.h"
#include "android.h"
#include "platform_android.h"
#include "OculusVR.h"
#include "Vehicle.h"
#include "VehicleModelInfo.h"
#include "ModelInfo.h"
#include "PlayerPed.h"
#include "PlayerInfo.h"
#include "MusicManager.h"
#include "sampman.h"
#include "Frontend.h"
#include "CutsceneMgr.h"
#include "Camera.h"
#include "Pad.h"
#include "GameRenderCommand.h"
#include "vulkan/rwvk.h"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

extern bool gVrFirstPersonActive;
extern CVehicle *FindPlayerVehicle(void);
extern const char *GetVrVehicleModelName(int model);
extern int32 gNumRetunePresses;
extern int32 gRetuneCounter;
namespace QuestRadio {
namespace {
using RadioBoard::Placement;
const char *keys[]={"Xmm","Ymm","Zmm","PitchDeg","YawDeg","RollDeg","ScalePercent"};
const char *labels[]={"RIGHT","FORWARD","UP","PITCH","YAW","ROLL","SIZE"};
Placement global,model,bundledGlobal,bundledModel,draft;
bool loaded=false, visible=true, edit=false, modelTarget=false, interacting=false, xDown=false;
bool triggerDown[2]={true,true};
bool touchMode=false;
RadioBoard::Touch touch[2];
CVehicle *touchVehicle=nil;
int modelId=-1, hover=-1;
double xSince=0;
char notice[96]="HOLD X, POINT, THEN CLICK TRIGGER";
std::string personal,defaults;
double Now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
std::string Section(int id) { return id<0?"Global":"Vehicle."+std::to_string(id); }
Placement Read(const std::string &path,int id) {
    Placement p;
    if(GetPrivateProfileIntA("RadioBoard","Version",0,path.c_str())!=1) return p;
    std::string section=Section(id);
    p.owned=GetPrivateProfileIntA(section.c_str(),"Enabled",0,path.c_str())==1;
    for(int i=0;i<7;++i) p.v[i]=RadioBoard::ClampValue(i,GetPrivateProfileIntA(section.c_str(),keys[i],p.v[i],path.c_str()));
    return p;
}
bool Write(const std::string &path,int id,const Placement &p) {
    // Whole file atomic replacement is handled below for personal saves.
    bool ok=WritePrivateProfileStringA("RadioBoard","Version","1",path.c_str())!=0;
    std::string section=Section(id);
    for(int i=0;i<7;++i) { char value[24]; snprintf(value,sizeof(value),"%d",p.v[i]); ok=WritePrivateProfileStringA(section.c_str(),keys[i],value,path.c_str())!=0 && ok; }
    return WritePrivateProfileStringA(section.c_str(),"Enabled",p.owned?"1":"0",path.c_str())!=0 && ok;
}
CVehicle *Vehicle() {
    CVehicle *v=FindPlayerVehicle();
    return v && v->pDriver==FindPlayerPed() && (v->IsCar() || v->IsBoat()) && !v->IsRealHeli() && !v->IsRealPlane()?v:nil;
}
void Load() {
    if(!loaded) {
        personal=std::string(platform::gameDataRoot())+"/radio_board.ini";
        defaults=std::string(platform::storageRoot())+"/radio_board_defaults.ini";
        global=Read(personal,-1); bundledGlobal=Read(defaults,-1);
        visible=GetPrivateProfileIntA("RadioBoard","Visible",1,personal.c_str())!=0;
        touchMode=GetPrivateProfileIntA("RadioBoard","InteractionMode",0,personal.c_str())==1;
        if(touchMode) snprintf(notice,sizeof(notice),"TOUCH BUTTONS WITH YOUR INDEX FINGER");
        loaded=true;
    }
    CVehicle *current=Vehicle();
    if(current!=touchVehicle) { for(auto &t:touch) t.Reset(); touchVehicle=current; }
    int id=current?current->GetModelIndex():-1;
    if(id!=modelId) {
        if(edit) CloseEditor();
        modelId=id; model=Read(personal,id); bundledModel=id<0?Placement():Read(defaults,id);
    }
}
Placement Resolved() { return RadioBoard::Resolve(model,global,bundledModel,bundledGlobal); }
Placement TargetPlacement() { return modelTarget ? Resolved() : (global.owned?global:bundledGlobal); }
void BeginEdit() { if(!edit) { draft=TargetPlacement(); edit=true; } }
void SavePlacement(bool own=true) {
    draft.owned=own;
    if(modelTarget && modelId<0) { snprintf(notice,sizeof(notice),"ENTER A VEHICLE FIRST"); return; }
    // Preserve other personal sections, then rename the completed replacement.
    std::string temp=personal+".tmp";
    FILE *in=fopen(personal.c_str(),"rb"),*out=fopen(temp.c_str(),"wb");
    if(!out) { if(in) fclose(in); snprintf(notice,sizeof(notice),"SAVE FAILED"); return; }
    bool copied=true;
    if(in) { char buf[4096]; size_t n; while((n=fread(buf,1,sizeof(buf),in))) if(fwrite(buf,1,n,out)!=n) copied=false; if(ferror(in)) copied=false; fclose(in); }
    if(fclose(out)!=0) copied=false;
    int id=modelTarget?modelId:-1;
    if(copied && Write(temp,id,draft) && rename(temp.c_str(),personal.c_str())==0) {
        if(modelTarget) model=draft; else global=draft;
        edit=false; snprintf(notice,sizeof(notice),"PLACEMENT SAVED");
    } else { remove(temp.c_str()); snprintf(notice,sizeof(notice),"SAVE FAILED"); }
}
struct Plane { CVector origin,right,up,normal; float width; };
void Rotate(CVector &a,CVector &b,int deg) { float t=deg*0.01745329252f; CVector old=a; a=a*cosf(t)+b*sinf(t); b=b*cosf(t)-old*sinf(t); }
bool BuildPlane(Plane &p) {
    Load(); CVehicle *v=Vehicle();
    if(!v || !visible || !gVrFirstPersonActive || OculusVR::IsQuestVehicleThirdPerson() ||
       FindPlayerPed()->DyingOrDead() || CCutsceneMgr::IsRunning() || CCutsceneMgr::IsCutsceneProcessing() || androidgame::VrCutsceneCameraActor()!=nil ||
       FrontEndMenuManager.m_bMenuActive || FrontEndMenuManager.m_bGameNotLoaded || TheCamera.m_WideScreenOn) return false;
    auto info=(CVehicleModelInfo*)CModelInfo::GetModelInfo(v->GetModelIndex()); if(!info) return false;
    CVector seat=info->GetFrontSeatPosn(); if(!v->IsBoat()) seat.x=-seat.x;
    Placement place=edit?draft:Resolved();
    seat+=CVector(place.v[0]*.001f,place.v[1]*.001f,place.v[2]*.001f);
    p.origin=v->GetPosition()+Multiply3x3(v->GetMatrix(),seat);
    p.right=v->GetRight(); p.up=v->GetUp(); p.normal=v->GetForward()*-1;
    Rotate(p.up,p.normal,place.v[3]); Rotate(p.normal,p.right,place.v[4]); Rotate(p.right,p.up,place.v[5]);
    p.width=.40f*place.v[6]/100.f; return true;
}
void Station(int direction) {
    CVehicle *v=Vehicle(); if(!v || MusicManager.UsesPoliceRadio(v)) return;
    int station=v->m_nRadioStation;
    // Include RADIO OFF, omit police radio and unavailable user tracks.
    for(int tries=0;tries<12;++tries) {
        station=(station+direction+12)%12;
        if(station==POLICE_RADIO || (station==USERTRACK && !QuestMusic::Available())) continue;
        MusicManager.SetRadioInCar(station);
        // SetRadioInCar only saves the tuning; ServiceGameMode needs the same
        // retune request as the normal station shortcut to change the stream.
        // The board already selected an exact station, so discard pending taps.
        gNumRetunePresses=0;
        gRetuneCounter=0;
        MusicManager.m_bSetNextStation=true;
        break;
    }
}
void Activate(int button) {
    bool custom=MusicManager.GetNextTrack()==USERTRACK;
    if(button<2) Station(button==0?-1:1);
    else if(button<4) {
        int volume=FrontEndMenuManager.m_PrefsMusicVolume+(button==2?-8:8);
        volume=volume<0?0:volume>127?127:volume;
        FrontEndMenuManager.m_PrefsMusicVolume=volume; SampleManager.SetMusicMasterVolume(volume); FrontEndMenuManager.SaveSettings();
    } else if(button<7 && custom) {
        if(button==5) QuestMusic::TogglePause(); else QuestMusic::Skip(button==4?-1:1);
    } else if(button==7) QuestMusic::Rescan();
}
const char *StationName() {
    static const char *names[]={"HEAD RADIO","DOUBLE CLEF","K-JAH","RISE FM","LIPS 106","GAME FM","MSX FM","FLASHBACK","CHATTERBOX","CUSTOM SOUNDTRACKS","POLICE RADIO","RADIO OFF"};
    int station=MusicManager.GetRadioInCar(); return station>=0 && station<12?names[station]:"RADIO OFF";
}
std::vector<RwIm3DVertex> vertices;
std::vector<RwImVertexIndex> indices;
void Rect(const Plane &p,float x,float y,float w,float h,int r,int g,int b,float depth) {
    if(vertices.size()+4>65000) return;
    RwImVertexIndex base=(RwImVertexIndex)vertices.size();
    const float xs[]={x,x+w,x+w,x},ys[]={y,y,y+h,y+h};
    for(int i=0;i<4;++i) {
        CVector v=p.origin+p.right*((xs[i]-256)*p.width/512)+p.up*((128-ys[i])*p.width/512)+p.normal*depth;
        RwIm3DVertex vert{}; RwIm3DVertexSetPos(&vert,v.x,v.y,v.z); RwIm3DVertexSetRGBA(&vert,r,g,b,255); vertices.push_back(vert);
    }
    for(int i:{0,1,2,0,2,3}) indices.push_back(base+i);
}
void Text(const Plane &p,const char *text,float cx,float y,float scale,int r,int g,int b) {
    int count=std::min(40,(int)strlen(text)); float x=cx-count*6*scale/2;
    for(int i=0;i<count;++i,x+=6*scale) {
        const unsigned char *rows=androidgame::VrRadioGlyph(text[i]);
        for(int row=0;row<7;++row) for(int col=0;col<5;++col)
            if(rows[row]&(1<<(4-col))) Rect(p,x+col*scale,y+row*scale,scale,scale,r,g,b,.002f);
    }
}
}
const char *MenuStatus() { return notice; }
void CloseEditor() { edit=false; }
bool Interacting() { return interacting; }
bool TouchEnabled() { Plane p; return BuildPlane(p) && touchMode; }
bool Update(bool blocked) {
    Load(); const auto &input=androidgame::GetPadInput();
    double now=Now(); bool tap=false;
    Plane plane; bool available=!blocked && BuildPlane(plane);
    bool x=available && input.x;
    if(x && !xDown) xSince=now;
    if(!x && xDown && available && now-xSince<.30) tap=true;
    interacting=!touchMode && x && now-xSince>=.30;
    xDown=x; hover=-1;
    bool activated=false;
    for(int hand=0;hand<2;++hand) {
        bool trigger=(hand?input.rightTrigger:input.leftTrigger)>=.55f;
        const auto &aim=input.aimPose[hand];
        if(touchMode) {
            CVector tip;
            if(available && OculusVR::GetTrackedIndexTip(hand,&tip)) {
                CVector delta=tip-plane.origin; int near=-1;
                int press=touch[hand].Update(DotProduct(delta,plane.right),DotProduct(delta,plane.up),
                    DotProduct(delta,plane.normal),plane.width,near);
                if(near>=0) { hover=near; interacting=true; }
                if(press>=0 && !activated) { Activate(press); activated=true; }
            } else touch[hand].Reset();
        } else if(interacting && aim.valid && !OculusVR::IsTrackedWeaponHeld(hand)) {
            float right[3],up[3],forward[3],origin[3];
            if(rw::vulkan::playPoseToFirstPersonWorld(aim.position,aim.orientation,right,up,forward,origin)) {
                CVector delta=CVector(origin[0],origin[1],origin[2])-plane.origin;
                CVector dir(forward[0],forward[1],forward[2]); float px,py;
                if(RadioBoard::Intersect(DotProduct(delta,plane.right),DotProduct(delta,plane.up),DotProduct(delta,plane.normal),
                   DotProduct(dir,plane.right),DotProduct(dir,plane.up),DotProduct(dir,plane.normal),plane.width,px,py)) {
                    int hit=RadioBoard::Hit(px,py); if(hit>=0) {
                        hover=hit; if(trigger && !triggerDown[hand]) Activate(hit);
                    }
                }
            }
        }
        // Seed while blocked too; held acceleration cannot become a UI click.
        triggerDown[hand]=trigger;
    }
    // Preserve the original X station shortcut when the board is hidden or
    // third person is active. These paths intentionally use an ordinary edge.
    static bool fallbackDown=false;
    if(!available && !blocked && Vehicle()) tap=input.x && !fallbackDown;
    fallbackDown=input.x;
    return tap;
}
void SuppressDriving(CControllerState *state) { if(interacting && state) state->Clear(); }
void MenuRow(int row,char *text,int length) {
    Load(); Placement p=edit?draft:TargetPlacement();
    if(row==1) { snprintf(text,length,"INTERACTION  < %s >",touchMode?"TOUCH":"LASER POINTER"); return; }
    if(row>1) --row;
    if(row==0) snprintf(text,length,"RADIO BOARD  < %s >",visible?"VISIBLE":"HIDDEN");
    else if(row==1) snprintf(text,length,"EDIT  < %s >",modelTarget?(modelId>=0?GetVrVehicleModelName(modelId):"ENTER VEHICLE"):"GLOBAL");
    else if(row>=2 && row<=8) snprintf(text,length,"%s  < %+d %s >",labels[row-2],p.v[row-2],row<5?"MM":row<8?"DEG":"%");
    else {
        const char *rows[]={"SAVE PLACEMENT","CANCEL UNSAVED CHANGES","RESET TARGET TO INHERITED DEFAULT","EXPORT THIS VEHICLE PLACEMENT","RESCAN MUSIC LIBRARY","BACK TO SETTINGS"};
        snprintf(text,length,"%s",rows[row-9]);
    }
}
void MenuAction(int row,int direction) {
    Load();
    if(row==1) {
        bool next=!touchMode;
        if(WritePrivateProfileStringA("RadioBoard","InteractionMode",next?"1":"0",personal.c_str())) {
            touchMode=next; for(auto &t:touch) t.Reset(); interacting=false;
            snprintf(notice,sizeof(notice),"%s",touchMode?"TOUCH BUTTONS WITH YOUR INDEX FINGER":"HOLD X, POINT, THEN CLICK TRIGGER");
        } else snprintf(notice,sizeof(notice),"INTERACTION SETTING SAVE FAILED");
        return;
    }
    if(row>1) --row;
    if(row==0) { visible=!visible; WritePrivateProfileStringA("RadioBoard","Visible",visible?"1":"0",personal.c_str()); }
    else if(row==1) { CloseEditor(); modelTarget=!modelTarget; }
    else if(row>=2 && row<=8) {
        if(modelTarget && modelId<0) return;
        BeginEdit(); int i=row-2; draft.v[i]=RadioBoard::ClampValue(i,draft.v[i]+direction*(i<3?5:i<6?1:5));
    } else if(row==9) { BeginEdit(); SavePlacement(); }
    else if(row==10) { CloseEditor(); snprintf(notice,sizeof(notice),"UNSAVED CHANGES CANCELLED"); }
    else if(row==11) { if(modelTarget && modelId<0) return; BeginEdit(); draft=Placement(); draft.owned=false;
        // Clearing ownership uses the same atomic personal-file save path.
        Placement inherited=modelTarget?RadioBoard::Resolve(Placement(),global,bundledModel,bundledGlobal):bundledGlobal;
        draft=inherited; SavePlacement(false);
    } else if(row==12) {
        if(modelId<0) { snprintf(notice,sizeof(notice),"ENTER A VEHICLE TO EXPORT"); return; }
        std::string path=std::string(platform::gameDataRoot())+"/radio_board_export.ini",temp=path+".tmp";
        remove(temp.c_str()); Placement p=Resolved(); p.owned=true;
        if(Write(temp,modelId,p) && rename(temp.c_str(),path.c_str())==0) snprintf(notice,sizeof(notice),"EXPORTED SAVED MODEL %d",modelId);
        else snprintf(notice,sizeof(notice),"EXPORT FAILED");
    } else if(row==13) QuestMusic::Rescan();
}
void Render() {
    Plane p; if(!BuildPlane(p)) return;
    vertices.clear(); indices.clear();
    Rect(p,0,0,512,256,18,19,25,0);
    Rect(p,8,8,496,58,4,14,12,.001f);
    Text(p,StationName(),256,14,2,255,206,104);
    Text(p,MusicManager.GetNextTrack()==USERTRACK?QuestMusic::Title():"LIBERTY CITY RADIO",256,40,1.6f,110,240,180);
    bool custom=MusicManager.GetNextTrack()==USERTRACK;
    for(int i=0;i<8;++i) {
        const auto &b=RadioBoard::Buttons[i]; bool disabled=i>=4 && i<=6 && !custom;
        Rect(p,b.left,b.top,b.right-b.left,b.bottom-b.top,hover==i?90:35,hover==i?79:39,hover==i?36:48,.001f);
        Text(p,i==5 && QuestMusic::UserPaused()?"PLAY":b.text,(b.left+b.right)/2,b.top+16,1.6f,disabled?95:245,disabled?95:220,disabled?95:170);
    }
    char volume[64]; snprintf(volume,sizeof(volume),"VOLUME %d%% / %s",FrontEndMenuManager.m_PrefsMusicVolume*100/127,QuestMusic::Status());
    Text(p,volume,184,204,1,150,190,180);
    Text(p,edit?"PLACEMENT PREVIEW - SAVE OR CANCEL":notice,184,228,1,210,210,190);
    RwIm3DVertex rays[4]{}; int rayCount=0;
    if(interacting && !touchMode) for(int hand=0;hand<2;++hand) {
        const auto &aim=androidgame::GetPadInput().aimPose[hand];
        if(!aim.valid || OculusVR::IsTrackedWeaponHeld(hand)) continue;
        float right[3],up[3],forward[3],origin[3];
        if(!rw::vulkan::playPoseToFirstPersonWorld(aim.position,aim.orientation,right,up,forward,origin)) continue;
        CVector from(origin[0],origin[1],origin[2]),dir(forward[0],forward[1],forward[2]),delta=from-p.origin;
        CVector end=from+dir;
        float px,py;
        if(RadioBoard::Intersect(DotProduct(delta,p.right),DotProduct(delta,p.up),DotProduct(delta,p.normal),
           DotProduct(dir,p.right),DotProduct(dir,p.up),DotProduct(dir,p.normal),p.width,px,py)) {
            end=p.origin+p.right*((px-256)*p.width/512)+p.up*((128-py)*p.width/512)+p.normal*.003f;
            Rect(p,px-3,py-3,6,6,255,235,150,.004f);
        }
        RwIm3DVertexSetPos(&rays[rayCount],from.x,from.y,from.z);
        RwIm3DVertexSetRGBA(&rays[rayCount],110,220,180,255); ++rayCount;
        RwIm3DVertexSetPos(&rays[rayCount],end.x,end.y,end.z);
        RwIm3DVertexSetRGBA(&rays[rayCount],255,235,150,255); ++rayCount;
    }
    const RwRenderState states[]={rwRENDERSTATETEXTURERASTER,rwRENDERSTATEVERTEXALPHAENABLE,rwRENDERSTATEZTESTENABLE,rwRENDERSTATEZWRITEENABLE,rwRENDERSTATECULLMODE};
    void *old[5]={}; for(int i=0;i<5;++i) RwRenderStateGet(states[i],&old[i]);
    RwRenderStateSet(states[0],nil); RwRenderStateSet(states[1],(void*)FALSE); RwRenderStateSet(states[2],(void*)TRUE); RwRenderStateSet(states[3],(void*)TRUE); RwRenderStateSet(states[4],(void*)rwCULLMODECULLNONE);
    if(GameRender::BeginImmediate3D(vertices.data(),vertices.size(),nil,rwIM3D_VERTEXXYZ|rwIM3D_VERTEXRGBA,GameRender::IMMEDIATE3D_DRAW_VR_HAND)) {
        GameRender::SubmitImmediate3DIndexed(rwPRIMTYPETRILIST,indices.data(),indices.size()); GameRender::EndImmediate3D();
    }
    if(rayCount && GameRender::BeginImmediate3D(rays,rayCount,nil,rwIM3D_VERTEXXYZ|rwIM3D_VERTEXRGBA,GameRender::IMMEDIATE3D_DRAW_LINES)) {
        for(int i=0;i<rayCount;i+=2) GameRender::SubmitImmediate3DLine(i,i+1);
        GameRender::EndImmediate3D();
    }
    for(int i=0;i<5;++i) RwRenderStateSet(states[i],old[i]);
}
}
#endif
