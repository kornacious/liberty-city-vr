#include "RenderDebugCore.h"
#include "RenderDebugProfile.h"
#include "ViewerInputPolicy.h"
#include "../src/renderer/WorldLodPolicy.h"
#include <iostream>
#include <functional>
#include <chrono>
using namespace RenderDebug;
static void Reject(const std::function<void()> &fn){bool rejected=false;try{fn();}catch(...){rejected=true;}Check(rejected,"Invalid input was accepted");}
int main(){try{
    for(float fov:{70.f,90.f,110.f}){
        const float multiplier=WorldLodPolicy::DetailDistanceMultiplier(70.f/fov*1.2f);
        Check(multiplier==2.2f,"Observation camera lost the production LOD distance floor");
        Check(WorldLodPolicy::GenerationDistanceMultiplier(70.f/fov)==2.f,"Observation population distance lost production floor");
    }
    Check(WorldLodPolicy::DetailDistanceMultiplier(3.5f)==3.5f,"Camera zoom/draw-distance scale was clamped down");
    HoldCameraCapture mouse;
    Check(!mouse.Update(true,true,false,false,false),"Camera captured without right mouse");
    Check(mouse.Update(true,true,true,false,false),"Viewport right press did not capture");
    Check(mouse.Update(true,true,true,true,false),"UI visibility interrupted an active flight gesture");
    Check(!mouse.Update(true,true,false,false,false),"Right release did not restore UI control");
    Check(!mouse.Update(true,true,true,true,false),"Right click on UI captured camera");
    Check(!mouse.Update(true,true,true,false,false),"Dragging out of UI started a flight gesture");
    mouse.Update(true,true,false,false,false);
    Check(mouse.Update(true,true,true,false,false),"New viewport gesture did not capture");
    Check(!mouse.Update(false,true,true,false,false),"Focus loss did not release capture");
    Check(!mouse.Update(true,true,true,false,false),"Focus regain resumed a held gesture");
    mouse.Update(true,true,false,false,false);
    Check(!mouse.Update(true,false,true,false,false),"Disabled keyboard/mouse captured camera");
    mouse.Update(true,true,false,false,false);
    Check(!mouse.Update(true,true,true,false,true),"Playback/batch captured camera");
    Scenario s;s.settings.views=2;s.settings.traffic=true;s.settings.graphics["MsaaSamples"]=4;s.screenshots={0,599};s.path={{0,Camera()},{10,Camera()}};s.path[1].camera.position={20,40,60};s.path[1].camera.rotation={0,0,1,0};s.path[1].camera.fov=120;
    auto decoded=DecodeScenario(DebugJson::Reader(DebugJson::Dump(Encode(s))).Parse());Check(DebugJson::Dump(Encode(s))==DebugJson::Dump(Encode(decoded)),"Scenario roundtrip changed data");
    auto mid=Sample(s.path,5);Check(std::abs(mid.position[0]+80)<1e-9&&std::abs(mid.fov-105)<1e-9,"Position/FOV interpolation incorrect");Check(std::abs(mid.rotation[2]-std::sqrt(.5))<1e-9,"Quaternion interpolation incorrect");
    s.path[1].camera.rotation={0,0,0,-1};mid=Sample(s.path,5);Check(std::abs(mid.rotation[3]-1)<1e-9,"Quaternion shortest path failed");
    Check(Sample(s.path,-1).position==s.path.front().camera.position&&Sample(s.path,20).position==s.path.back().camera.position,"Path endpoints not clamped");
    Check(std::abs(PlaybackTime(600,1./60)-10)<1e-12,"Fixed step playback drifted");
    MatchedProfile::Values profile{{"width",1200},{"height",1100},{"setting_Sgsr2Mode",0},{"setting_MsaaSamples",4}};auto native=s;ApplyProfile(native,profile);Check(native.settings.width==1200&&native.settings.height==1100&&native.settings.views==2,"Quest profile geometry was not applied");
    profile["setting_Sgsr2Mode"]=2;Reject([&]{ApplyProfile(native,profile);});
    Reject([&]{auto bad=Encode(s);bad.object["version"]=2;DecodeScenario(bad);});Reject([&]{auto bad=Encode(s);bad.object["surprise"]=0;DecodeScenario(bad);});
    Reject([&]{auto bad=Encode(s);bad.object["timestep"]=0;DecodeScenario(bad);});Reject([&]{auto bad=Encode(s);bad.object["repetitions"]=-1;DecodeScenario(bad);});
    Reject([&]{auto path=s.path;path[1].time=0;Validate(path);});Reject([&]{Camera c;c.rotation={0,0,0,0};Validate(c);});
    Reject([&]{DebugJson::Reader("{\"a\":1,\"a\":2}").Parse();});Reject([&]{DebugJson::Reader("[1,]").Parse();});Reject([&]{DebugJson::Reader("01").Parse();});Reject([&]{DebugJson::Reader("1e999").Parse();});
    auto escaped=DebugJson::Reader("\"\\uD83D\\uDE00\\n\\\"\"").Parse();Check(DebugJson::Reader(DebugJson::Dump(escaped)).Parse().Str()==escaped.Str(),"Unicode/escaping roundtrip failed");
    auto root=std::filesystem::temp_directory_path()/("lcvr-viewer-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(root);auto first=NewRun(root),second=NewRun(root);Check(first!=second,"Output isolation failed");DebugJson::Save(first/"scenario.json",Encode(s));DebugJson::Save(first/"scenario.json",Encode(decoded));Check(DebugJson::Dump(DebugJson::Load(first/"scenario.json"))==DebugJson::Dump(Encode(decoded)),"Document replacement failed");
    Check(Within(root,first)&&!Within(first,second)&&!Within(first,root),"Directory containment isolation failed");
    std::filesystem::remove(first/"scenario.json");std::filesystem::remove(first);std::filesystem::remove(second);std::filesystem::remove(root);
    std::cout<<"Renderer debug core checks passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
