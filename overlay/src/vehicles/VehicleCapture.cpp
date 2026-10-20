#include "common.h"
#include "VehicleCapture.h"
#if defined(__ANDROID__) && defined(LIBERTYVR_VEHICLE_CAPTURE) && LIBERTYVR_VEHICLE_CAPTURE
#include "VehicleCaptureCore.h"
#include "Automobile.h"
#include "HandlingMgr.h"
#include "Pools.h"
#include "World.h"
#include "Replay.h"
#include "CutsceneMgr.h"
#include "Timer.h"
#include <sys/stat.h>
#include <time.h>
#include <cstring>
#include <memory>

namespace androidgame { namespace {
#ifndef LCVR_VEHICLE_CAPTURE_ROOT
#define LCVR_VEHICLE_CAPTURE_ROOT "/sdcard/Android/data/com.libertyvr.quest/files/vehicle-capture"
#endif
const char *Root(){return LCVR_VEHICLE_CAPTURE_ROOT;}
FILE *inputs=nullptr,*telemetry=nullptr;
std::unique_ptr<VehicleCaptureCore::Tape> tape;
int mode=0,reference=-1;unsigned model=0,signature=0;
uint64_t elapsed=0,lastInput=0,pollAfter=0,wallStart=0,frameStart=0;
unsigned samples=0,limitSeconds=30,collisions=0,contacts=0,retries=0,substeps=0,controls=0;
bool sampled=false;float frameDt=0;VehicleCaptureCore::Input applied;
double gameTime=0;
uint64_t Now(){timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return uint64_t(t.tv_sec)*1000000000ULL+t.tv_nsec;}
void Status(const char *text){char path[256];std::snprintf(path,sizeof(path),"%s/status.txt",Root());FILE *f=std::fopen(path,"w");if(f){std::fprintf(f,"%s\n",text);std::fclose(f);}}
CAutomobile *Selected(){
    CVehicle *v=FindPlayerVehicle();
    return v&&v->IsCar()&&v->GetStatus()==STATUS_PLAYER?static_cast<CAutomobile *>(v):nullptr;
}
CAutomobile *Target(){
    CVehicle *v=reference>=0?CPools::GetVehicle(reference):nullptr;
    return v&&v==Selected()&&unsigned(v->GetModelIndex())==model?static_cast<CAutomobile *>(v):nullptr;
}
unsigned Signature(CAutomobile *c){
    // Canonical scalar fingerprint; no padding bytes or pointer values.
    const tHandlingData &h=*c->pHandling;
    const float values[]={c->m_fMass,c->m_fTurnMass,h.fTractionMultiplier,h.fTractionLoss,h.fBrakeDeceleration,h.fBrakeBias,h.fSuspensionForceLevel,h.fSuspensionDampingLevel,h.fSuspensionBias,
        c->m_vecCentreOfMass.x,c->m_vecCentreOfMass.y,c->m_vecCentreOfMass.z};
    unsigned hash=2166136261u;
    for(float v:values){char text[40];std::snprintf(text,sizeof(text),"%.9g;",double(v));for(const char *p=text;*p;++p)hash=(hash^unsigned(*p))*16777619u;}
    return hash;
}
void Stop(const char *reason){
    bool failed=false;
    if(inputs){
        // Only a completed tape receives a replayable END marker.
        if(samples&&elapsed>lastInput)std::fprintf(inputs,"END,%llu\n",(unsigned long long)elapsed);
        if(std::fclose(inputs)!=0)failed=true;inputs=nullptr;
    }
    if(telemetry){std::fprintf(telemetry,"# end,%s\n",reason);if(std::fclose(telemetry)!=0)failed=true;telemetry=nullptr;}
    tape.reset();mode=0;reference=-1;Status(failed?"trace close/write error":reason);
}
void Initial(CAutomobile *c,VehicleCaptureCore::Initial &s){
    s.model=c->GetModelIndex();s.signature=Signature(c);
    const CVector vectors[]={c->GetPosition(),c->GetRight(),c->GetForward(),c->GetUp(),c->m_vecMoveSpeed,c->m_vecTurnSpeed};
    int i=0;for(const CVector &v:vectors){s.pose[i++]=v.x;s.pose[i++]=v.y;s.pose[i++]=v.z;}
}
CVector Vec(const float *p){return CVector(p[0],p[1],p[2]);}
void Restore(CAutomobile *c,const VehicleCaptureCore::Initial &s){
    // World begin hook runs before moving-list traversal, so native relinking is safe.
    c->Teleport(Vec(s.pose));CWorld::Remove(c);
    c->GetRight()=Vec(s.pose+3);c->GetForward()=Vec(s.pose+6);c->GetUp()=Vec(s.pose+9);
    c->m_vecMoveSpeed=Vec(s.pose+12);c->m_vecTurnSpeed=Vec(s.pose+15);
    c->m_vecMoveFriction=c->m_vecTurnFriction=CVector(0,0,0);
    c->m_vecMoveSpeedAvg=c->m_vecMoveSpeed;c->m_vecTurnSpeedAvg=c->m_vecTurnSpeed;c->m_nStaticFrames=0;
    c->GetMatrix().UpdateRW();c->UpdateRwFrame();CWorld::Add(c);
}
void Command(){
    char path[256],command[128],op[16],label[33],extra;unsigned seconds=0;
    std::snprintf(path,sizeof(path),"%s/control.txt",Root());FILE *f=std::fopen(path,"r");if(!f)return;
    const bool read=std::fgets(command,sizeof(command),f)!=nullptr;const bool complete=std::fgetc(f)==EOF;std::fclose(f);std::remove(path);
    if(!read||!complete){Status("invalid command");return;}
    if(std::strcmp(command,"stop\n")==0||std::strcmp(command,"stop")==0){Stop("stopped");return;}
    if(mode){Status("busy: stop before starting another run");return;}
    const int count=std::sscanf(command,"%15s %32s %u %c",op,label,&seconds,&extra);
    if(count<2||!VehicleCaptureCore::Label(label)||!((std::strcmp(op,"record")==0&&count==3&&seconds>=1&&seconds<=120)||(std::strcmp(op,"replay")==0&&count==2))){Status("invalid command");return;}
    CAutomobile *c=Selected();if(!c||CWorld::bProcessCutsceneOnly||CReplay::IsPlayingBack()){Status("select a player automobile in normal gameplay");return;}
    VehicleCaptureCore::Initial initial;Initial(c,initial);
    if(!VehicleCaptureCore::Valid(initial)){Status("invalid vehicle transform");return;}
    if(std::strcmp(op,"replay")==0){
        std::snprintf(path,sizeof(path),"%s/%s.inputs.csv",Root(),label);FILE *source=std::fopen(path,"r");
        tape.reset(new VehicleCaptureCore::Tape);
        const bool loaded=source&&tape->Load(source);if(source)std::fclose(source);
        if(!loaded||tape->initial.model!=initial.model||tape->initial.signature!=initial.signature){tape.reset();Status("invalid, truncated, wrong-model or changed-handling tape");return;}
        if((c->GetPosition()-Vec(tape->initial.pose)).MagnitudeSqr()>10000.0f){tape.reset();Status("return within 100 meters of the recorded start before replay");return;}
        mode=2;limitSeconds=120;
    }else{
        std::snprintf(path,sizeof(path),"%s/%s.inputs.csv",Root(),label);inputs=std::fopen(path,"wx");
        if(!inputs){Status("input file exists or cannot be created");return;}
        mode=1;limitSeconds=seconds;
        if(!VehicleCaptureCore::WriteHeader(inputs,initial)){Stop("input header write failed");return;}
    }
    std::snprintf(path,sizeof(path),"%s/%s.%s-%llu.telemetry.csv",Root(),label,mode==1?"record":"replay",(unsigned long long)Now());
    telemetry=std::fopen(path,"wx");if(!telemetry){Stop("telemetry file creation failed");return;}
    std::setvbuf(telemetry,nullptr,_IOFBF,65536);if(inputs)std::setvbuf(inputs,nullptr,_IOFBF,65536);
    std::fprintf(telemetry,"# LCVR_VEHICLE_TELEMETRY_V1,model=%d,handling=%u,mode=%s,build=%s %s\n",initial.model,initial.signature,mode==1?"record":"replay",__DATE__,__TIME__);
    const tHandlingData &h=*c->pHandling;
    std::fprintf(telemetry,"# handling,mass=%.9g,turn_mass=%.9g,traction=%.9g,traction_loss=%.9g,brake=%.9g,brake_bias=%.9g,spring=%.9g,damper=%.9g,spring_bias=%.9g,com=%.9g/%.9g/%.9g\n",
        double(c->m_fMass),double(c->m_fTurnMass),double(h.fTractionMultiplier),double(h.fTractionLoss),double(h.fBrakeDeceleration),double(h.fBrakeBias),double(h.fSuspensionForceLevel),double(h.fSuspensionDampingLevel),double(h.fSuspensionBias),double(c->m_vecCentreOfMass.x),double(c->m_vecCentreOfMass.y),double(c->m_vecCentreOfMass.z));
    std::fputs("game_us,wall_ns,world_ns,dt_s,nonclipped_dt_s,frame,control_calls,collision_calls,body_contacts,retries,substeps,static_frames,static,stuck,gear,gas,brake,steer,handbrake,health,x,y,z,rx,ry,rz,fx,fy,fz,ux,uy,uz,vx,vy,vz,wx,wy,wz,damage_impulse",telemetry);
    for(int i=0;i<4;++i)std::fprintf(telemetry,",spring%d,previous_spring%d,timer%d,state%d,px%d,py%d,pz%d,nx%d,ny%d,nz%d,surface%d,ground%d",i,i,i,i,i,i,i,i,i,i,i,i);
    std::fputc('\n',telemetry);
    reference=CPools::GetVehicleRef(c);model=unsigned(initial.model);signature=initial.signature;
    elapsed=lastInput=0;gameTime=0;samples=0;wallStart=Now();
    if(mode==2)Restore(c,tape->initial);
    Status(mode==1?"recording":"replaying: pose/velocities restored; world not restored");
}
} // namespace
void VehicleCaptureBeginFrame(){
    const uint64_t now=Now();if(now>=pollAfter){pollAfter=now+500000000ULL;Command();}
    if(!mode)return;
    if(!Target()||CWorld::bProcessCutsceneOnly||CReplay::IsPlayingBack()){Stop("vehicle lost, exited or gameplay mode changed");return;}
    if(Signature(Target())!=signature){Stop("handling changed");return;}
    if(now-wallStart>180000000000ULL){Stop("wall-time limit");return;}
    frameStart=now;frameDt=CTimer::GetTimeStepInSeconds();
    collisions=contacts=retries=substeps=controls=0;sampled=false;
    if(mode==2&&!tape->At(elapsed,applied)){Stop("replay complete");return;}
}
void VehicleCaptureInputs(CAutomobile *c){
    if(!mode||c!=Target()||frameDt<=0)return;
    controls++;
    if(mode==2){c->m_fGasPedal=applied.gas;c->m_fBrakePedal=applied.brake;c->m_fSteerAngle=applied.steer;c->bIsHandbrakeOn=applied.handbrake!=0;}
    if(sampled)return;sampled=true;
    if(mode==1){
        applied={elapsed,c->m_fGasPedal,c->m_fBrakePedal,c->m_fSteerAngle,c->bIsHandbrakeOn?1:0};
        if(samples>=VehicleCaptureCore::MaxSamples||!VehicleCaptureCore::Write(inputs,applied)){Stop("input sample limit or write error");return;}
        samples++;lastInput=elapsed;
    }
}
void VehicleCaptureCollision(const CPhysical *c,int n){if(mode&&c==Target()){collisions++;if(n>0)contacts+=unsigned(n);}}
void VehicleCaptureRetry(const CPhysical *c){if(mode&&c==Target())retries++;}
void VehicleCaptureSubsteps(const CPhysical *c,int n){if(mode&&c==Target()&&n>0)substeps+=unsigned(n);}
void VehicleCaptureEndFrame(){
    if(!mode||frameDt<=0)return;CAutomobile *c=Target();if(!c){Stop("vehicle lost during physics");return;}
    if(!sampled){Stop("vehicle input hook not reached");return;}
    const CVector p=c->GetPosition(),r=c->GetRight(),f=c->GetForward(),u=c->GetUp(),v=c->m_vecMoveSpeed,w=c->m_vecTurnSpeed;
    std::fprintf(telemetry,"%llu,%llu,%llu,%.9g,%.9g,%u,%u,%u,%u,%u,%u,%u,%d,%d,%d,%.9g,%.9g,%.9g,%d,%.9g",
        (unsigned long long)elapsed,(unsigned long long)(Now()-wallStart),(unsigned long long)(Now()-frameStart),double(frameDt),double(CTimer::GetTimeStepNonClippedInSeconds()),CTimer::GetFrameCounter(),controls,collisions,contacts,retries,substeps,
        unsigned(c->m_nStaticFrames),c->GetIsStatic()?1:0,c->bIsStuck?1:0,int(c->m_nCurrentGear),double(applied.gas),double(applied.brake),double(applied.steer),applied.handbrake,double(c->m_fHealth));
    const CVector vectors[]={p,r,f,u,v,w};for(const CVector &a:vectors)std::fprintf(telemetry,",%.9g,%.9g,%.9g",double(a.x),double(a.y),double(a.z));
    std::fprintf(telemetry,",%.9g",double(c->m_fDamageImpulse));
    for(int i=0;i<4;++i){CColPoint cp={};if(c->m_aSuspensionSpringRatio[i]<1.0f)cp=c->m_aWheelColPoints[i];std::fprintf(telemetry,",%.9g,%.9g,%.9g,%d,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%d,%d",
        double(c->m_aSuspensionSpringRatio[i]),double(c->m_aSuspensionSpringRatioPrev[i]),double(c->m_aWheelTimer[i]),int(c->m_aWheelState[i]),double(cp.point.x),double(cp.point.y),double(cp.point.z),double(cp.normal.x),double(cp.normal.y),double(cp.normal.z),int(cp.surfaceB),c->m_aGroundPhysical[i]?1:0);}
    std::fputc('\n',telemetry);
    if(std::ferror(telemetry)||std::ferror(inputs?inputs:telemetry)){Stop("trace write error");return;}
    gameTime+=double(frameDt);
    elapsed=uint64_t(gameTime*1000000.0+0.5);
    if(elapsed>uint64_t(limitSeconds)*1000000ULL)elapsed=uint64_t(limitSeconds)*1000000ULL;
    if(elapsed>=uint64_t(limitSeconds)*1000000ULL){Stop("duration complete");return;}
    if(mode==2&&elapsed>=tape->endUs){Stop("replay complete");return;}
    if(Now()-frameStart>1000000000ULL)Stop("trace frame exceeded wall budget");
}
} // namespace androidgame
#endif
