#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#define __ANDROID__
#define FIX_BUGS
#define GTA_VR_OCULUS
using uint8=unsigned char; using uint32=unsigned; using uint64=unsigned long long;
inline float Pow(float a,float b){return std::pow(a,b);}
static float Clamp(float v,float lo,float hi){return std::max(lo,std::min(hi,v));}
#define SQR(v) ((v)*(v))
#define DEGTORAD(v) ((v)*0.01745329251994329577f)
struct CTimer {
    static float seconds; static uint32 ticks; static uint64 counter;
    static float GetTimeStepInSeconds(){return seconds;}
    static uint32 GetGameFramesPassed(){return ticks;}
    static uint64 GetGameFrameCounter(){return counter;}
};
float CTimer::seconds; uint32 CTimer::ticks; uint64 CTimer::counter;
struct CPad {
    int brake=0,accelerate=0,steer=0;
    static CPad pads[4]; static CPad *GetPad(uint8 p){return &pads[p];}
    int GetBrake(){return brake;} int GetAccelerate(){return accelerate;}
    int GetSteeringLeftRight(){return steer;}
};
CPad CPad::pads[4];
struct OculusVR {
    static bool immersive; static float target;
    static bool GetImmersiveCarSteering(void *,float *out){*out=target;return immersive;}
};
bool OculusVR::immersive; float OculusVR::target;
struct Handling {float fSteeringLock=35;};
struct CBoat {
    float m_fBrake=0,m_fAccelerate=0,m_fSteeringLeftRight=0,m_fSteerAngle=0,m_fGasPedal=0;
    uint8 m_nPadID=0;
    Handling handling; Handling *pHandling=&handling;
    void ProcessControlInputs(uint8);
};
#include "boat-input-timing-production.inc"
static unsigned checks;
static void Check(bool ok,const char *message){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static bool Near(float a,float b){return std::fabs(a-b)<0.00002f;}
// Independent legacy 30 FPS recurrence, including its brake deadzone and reverse.
static void Reference(CBoat &b,const CPad &p){
    b.m_fBrake=Clamp(b.m_fBrake+(p.brake/255.0f-b.m_fBrake)*0.1f,0,1);
    if(b.m_fBrake<0.05f){
        b.m_fBrake=0;
        b.m_fAccelerate=Clamp(b.m_fAccelerate+(p.accelerate/255.0f-b.m_fAccelerate)*0.1f,0,1);
    }else b.m_fAccelerate=-b.m_fBrake*0.2f;
}
static void Validate(const CBoat &b){
    Check(std::isfinite(b.m_fGasPedal)&&std::isfinite(b.m_fSteerAngle),"outputs remain finite");
    Check(b.m_fBrake>=0&&b.m_fBrake<=1&&b.m_fAccelerate>=-0.2f&&b.m_fAccelerate<=1,"pedals remain bounded");
    Check(std::fabs(b.m_fSteeringLeftRight)<=1,"steering remains bounded");
    Check(Near(b.m_fGasPedal,b.m_fAccelerate),"filtered throttle reaches hull gas input");
    const float s=b.m_fSteeringLeftRight;
    Check(Near(b.m_fSteerAngle,(s<0?-s*s:s*s)*DEGTORAD(b.handling.fSteeringLock)),"signed squared steering lock preserved");
}
static void LegacyMatch(){
    CBoat b,reference; CTimer::seconds=1.0f/30; CTimer::ticks=1;
    for(int i=0;i<240;++i){
        CPad &p=CPad::pads[2];
        p.brake=i<60?0:i<120?255:i<180?100:0;
        p.accelerate=i<60?255:180;p.steer=i%60<30?-128:128;
        OculusVR::immersive=(i%2)==0;OculusVR::target=i%60<30?-0.65f:0.35f;
        Reference(reference,p);
        const float target=OculusVR::immersive?OculusVR::target:-p.steer/128.0f;
        reference.m_fSteeringLeftRight+=(target-reference.m_fSteeringLeftRight)*0.2f;
        b.ProcessControlInputs(2);
        Check(Near(b.m_fBrake,reference.m_fBrake)&&Near(b.m_fAccelerate,reference.m_fAccelerate),"30 Hz pedal recurrence preserved");
        Check(Near(b.m_fSteeringLeftRight,reference.m_fSteeringLeftRight),"30 FPS steering recurrence preserved");
        Check(b.m_nPadID==2,"selected controller preserved");Validate(b);
    }
}
static void Rates(int fps,float scale,bool jitter,bool immersive){
    CBoat b,reference;
    OculusVR::immersive=immersive;
    double fractionalMs=0;unsigned tickRemainder=0;double steeringTime=0;
    for(int phase=0;phase<6;++phase){
        CPad &p=CPad::pads[0];p.accelerate=phase==5?0:255;
        p.brake=phase==1?255:phase==3?100:phase==4?127:0;
        p.steer=phase%2?-128:128;OculusVR::target=p.steer/128.0f*0.6f;
        const float initial=b.m_fSteeringLeftRight;steeringTime=0;
        for(int frame=0;frame<fps;++frame){
            const double dt=std::min(0.06,scale/fps*(jitter?(frame%2?1.15:0.85):1.0));
            CTimer::seconds=float(dt);steeringTime+=CTimer::seconds;
            fractionalMs+=dt*1000;
            const unsigned ms=unsigned(fractionalMs+1.0e-8);fractionalMs-=ms;
            tickRemainder+=30*ms;CTimer::ticks=tickRemainder/1000;tickRemainder%=1000;
            CTimer::counter+=CTimer::ticks;
            for(unsigned t=0;t<CTimer::ticks;++t)Reference(reference,p);
            b.ProcessControlInputs(0);
            Check(Near(b.m_fBrake,reference.m_fBrake)&&Near(b.m_fAccelerate,reference.m_fAccelerate),"pedals follow crossed 30 Hz ticks, including deadzone/reverse transitions");
            const float target=immersive?OculusVR::target:-p.steer/128.0f;
            const float expected=target+(initial-target)*float(std::pow(0.8,30*steeringTime));
            Check(Near(b.m_fSteeringLeftRight,expected),"steering follows elapsed game time across rates and input reversal");
            Validate(b);
        }
        if(phase==1)Check(b.m_fBrake>0.7f&&b.m_fAccelerate<0,"high-rate braking engages and supplies reverse thrust");
    }
}
static void PauseAndCatchup(){
    CBoat b;CPad::pads[0].brake=255;CPad::pads[0].accelerate=255;
    CPad::pads[0].steer=-128;OculusVR::immersive=false;
    CTimer::ticks=0;CTimer::seconds=0;
    for(int i=0;i<100;++i)b.ProcessControlInputs(0);
    Check(b.m_fBrake==0&&b.m_fGasPedal==0&&b.m_fSteeringLeftRight==0,"paused inputs do not accumulate");
    CTimer::ticks=0;CTimer::seconds=1.0f/144;b.ProcessControlInputs(0);
    Check(b.m_fBrake==0&&b.m_fSteeringLeftRight>0,"steering remains smooth between pedal ticks");
    CTimer::ticks=2;CTimer::seconds=0.06f;b.ProcessControlInputs(0);
    Check(Near(b.m_fBrake,0.19f)&&Near(b.m_fGasPedal,-0.038f),"clipped stall catches both crossed pedal updates");
    const float brake=b.m_fBrake,gas=b.m_fGasPedal,steer=b.m_fSteeringLeftRight;
    CTimer::ticks=0;CTimer::seconds=0;
    CPad::pads[0].brake=0;CPad::pads[0].steer=128;
    for(int i=0;i<100;++i)b.ProcessControlInputs(0);
    Check(b.m_fBrake==brake&&b.m_fGasPedal==gas&&b.m_fSteeringLeftRight==steer,"pause freezes an already active boat despite changed inputs");
}
int main(){
    LegacyMatch();
    for(int fps : {15,18,20,30,36,60,72,80,90,120,144})
        for(float scale : {0.5f,1.0f})for(bool jitter : {false,true})for(bool immersive : {false,true})Rates(fps,scale,jitter,immersive);
    PauseAndCatchup();
    std::printf("PASS: %u production boat input timing checks\n",checks);
}
