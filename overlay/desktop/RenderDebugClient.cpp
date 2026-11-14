#include "common.h"
#include "RenderDebugClient.h"
#include "RenderDebugProfile.h"
#include "DebugWorld.h"
#include "DebugVulkan.h"
#include "DesktopMirror.h"
#include "platform_desktop.h"
#include "WorldSession.h"
#include "RendererSettings.h"
#include "QuestProfiler.h"
#include "QuestRenderDoc.h"
#include "BenchmarkIdentity.h"
#include "ModelSets.h"
#include "QuestUpstate.h"
#include "SourceIdentity.h"
#include "xr_vulkan_session.h"
#include "vulkan/rwvk.h"
#include "imgui.h"
#include "ViewerInputPolicy.h"
#include "VrWorldLod.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <thread>
#include <set>
#include <numeric>
#include <fstream>

namespace RenderDebug {
extern xrvk::GraphicsContext sessionGraphics;
using Clock=std::chrono::steady_clock;
static double Milliseconds(Clock::time_point a,Clock::time_point b){return std::chrono::duration<double,std::milli>(b-a).count();}
struct Frame {uint64_t id=0;int repeat=0,index=0;double simulation=0,cpu=0,update=0,render=0,present=0,interval=0;Camera camera;GpuPassProfile::Result gpu;};
struct Client {
    Scenario scenario;Camera camera;Settings activeSettings;std::string user,output,error,device,contentIdentity,activeProfile;
    std::filesystem::path run;bool batch=false,startBatch=false,stopBatch=false,recording=false,playing=false,loop=false,pausedPath=false,captureRenderDoc=false;
    int repeat=0,frame=0,keySelection=0,screenshotFrame=0;double pathTime=0,playbackSpeed=1,recordTime=0,simulation=0,yaw=0,pitch=0;
    unsigned frameLimit=0;uint64_t nextFrame=1;std::vector<Frame> results;std::map<uint64_t,size_t> rows;
    std::vector<float> history;std::map<std::string,Camera> bookmarks;
    char name[128]="camera",document[1024]="scenario.json",profile[1024]="",message[256]="";
    HoldCameraCapture mouseFlight;
    bool showDetails=true;double lastCpu=0,lastUpdate=0,lastRender=0,lastPresent=0;
    explicit Client(Scenario s):scenario(std::move(s)),camera(scenario.camera),activeSettings(scenario.settings){}
    void Graphics(bool startup){
        size_t n=0;const auto controls=RendererSettings::Controls(n);
        for(auto &entry:scenario.settings.graphics){bool found=false;
            if(entry.first=="CarReflectionScale")CheckInput(entry.second==1||entry.second==2||entry.second==4||entry.second==8,"Reflection scale must be 1, 2, 4 or 8");
            if(entry.first=="DynamicLightMax")CheckInput(entry.second%2==0,"Dynamic light limit must be even");
            if(entry.first=="MsaaSamples"){CheckInput(entry.second==1||entry.second==2||entry.second==4,"MSAA must be 1, 2 or 4");continue;}
            for(size_t i=0;i<n;++i)if(entry.first==controls[i].name){CheckInput(entry.second>=controls[i].minimum&&entry.second<=controls[i].maximum,"Renderer setting out of range");if(startup||!controls[i].restart)controls[i].set(entry.second);found=true;break;}
            CheckInput(found,"Unknown renderer setting");
        }RendererSettings::ApplySideEffects();
    }
    void SnapshotSettings(){size_t count=0;auto controls=RendererSettings::Controls(count);for(size_t i=0;i<count;++i)if(!scenario.settings.graphics.count(controls[i].name))scenario.settings.graphics[controls[i].name]=controls[i].get();}
    void SavePreferences(){Scenario s=scenario;s.camera=camera;s.settings.windowMode=int(Desktop::Settings().mode);SDL_GetWindowSize(static_cast<SDL_Window*>(Desktop::WindowHandle()),&s.settings.windowWidth,&s.settings.windowHeight);s=DecodeScenario(Encode(s));DebugJson::Save(std::filesystem::u8path(user)/"viewer.json",Encode(s));Json j=Json::Map();j.object["version"]=1;Json b=Json::Map();for(auto &entry:bookmarks)b.object[entry.first]=Encode(entry.second);j.object["bookmarks"]=b;DebugJson::Save(std::filesystem::u8path(user)/"bookmarks.json",j);}
    std::filesystem::path PathDocument(){std::string n=name;Check(!n.empty()&&n!="."&&n!=".."&&n.find_first_of("/\\:")==std::string::npos,"Use a camera-path name without directory separators");auto root=std::filesystem::u8path(user)/"paths";std::filesystem::create_directories(root);return root/std::filesystem::u8path(n+".json");}
    std::filesystem::path Document()const {std::filesystem::path p=std::filesystem::u8path(document);return p.is_absolute()?p:std::filesystem::u8path(user)/p;}
    void ApplyDisplay(){auto &s=scenario.settings;auto &o=Desktop::Settings();if(o.vsync!=s.vsync){o.vsync=s.vsync;Desktop::RequestResize();}o.eye=s.eye==1?1:0;o.sideBySide=s.eye==2;}
    void AnglesFromCamera(){auto &q=camera.rotation;double fx=2*(q[0]*q[1]-q[2]*q[3]),fy=1-2*(q[0]*q[0]+q[2]*q[2]),fz=2*(q[1]*q[2]+q[0]*q[3]);yaw=std::atan2(-fx,fy);pitch=std::asin(std::clamp(fz,-1.,1.));}
    void Fly(double dt){
        const auto &in=Desktop::Input();const auto &o=Desktop::Settings();auto &io=ImGui::GetIO();
        if(!in.focused||playing)return;
        const bool mouseControl=o.keyboard&&o.capture;
        const bool padControl=in.pad&&!io.WantCaptureKeyboard&&!io.WantCaptureMouse;
        if(!mouseControl&&!padControl)return;
        auto &s=scenario.settings;
        double dx=mouseControl?in.deltaX:0,dy=mouseControl?in.deltaY:0;
        if(padControl){dx+=in.rx*dt*600;dy-=in.ry*dt*600;}
        if(dx||dy){AnglesFromCamera();yaw-=dx*s.sensitivity*3.141592653589793/180;pitch=std::clamp(pitch-dy*s.sensitivity*3.141592653589793/180,-1.55,1.55);double sy=std::sin(yaw/2),cy=std::cos(yaw/2),sp=std::sin(pitch/2),cp=std::cos(pitch/2);camera.rotation={cy*sp,sy*sp,sy*cp,cy*cp};}
        auto key=[&](unsigned k){return mouseControl&&in.keys[k]?1.:0.;};double forward=key('W')-key('S')+(padControl?in.ly:0),right=key('D')-key('A')+(padControl?in.lx:0),up=key('E')-key('Q')+(padControl?in.rt-in.lt:0);
        double speed=dt*s.speed*(in.keys[Desktop::Key_Shift]?4:1)*(in.keys[Desktop::Key_Control]?.25:1);
        auto &q=camera.rotation;std::array<double,3> r{{1-2*(q[1]*q[1]+q[2]*q[2]),2*(q[0]*q[1]+q[2]*q[3]),2*(q[0]*q[2]-q[1]*q[3])}},f{{2*(q[0]*q[1]-q[2]*q[3]),1-2*(q[0]*q[0]+q[2]*q[2]),2*(q[1]*q[2]+q[0]*q[3])}};
        double length=std::max(1.,std::sqrt(forward*forward+right*right+up*up));for(int i=0;i<3;++i)camera.position[i]+=speed/length*(forward*f[i]+right*r[i]+(i==2?up:0));
    }
    static bool Real(const char *label,double &value,float low,float high){float v=float(value);if(!ImGui::SliderFloat(label,&v,low,high))return false;value=v;return true;}
    void Statistics(){
        auto &s=scenario.settings;
        ImGui::SetNextWindowSize(ImVec2(560,600),ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(560,60),ImGuiCond_FirstUseEver);
        ImGui::Begin("Renderer statistics (F2)",&Desktop::Settings().statistics);

            if(!batch)ImGui::Checkbox("Enable profiling",&s.profiling);ImGui::Text("CPU %.2f ms; update %.2f; render %.2f; present %.2f",lastCpu,lastUpdate,lastRender,lastPresent);if(!history.empty())ImGui::PlotLines("CPU frame ms",history.data(),int(history.size()));
            const char *lodNames[]={"Off - high detail only","On","Performance","Aggressive"};ImGui::Text("FPS %.1f / World LOD: %s",ImGui::GetIO().Framerate,lodNames[WorldLodPolicy::ClampMode(VrWorldLod::Mode())]);ImGui::Text("Camera LOD distance %.2fx (On mode)",LodDistanceMultiplier());ImGui::Text("Texture memory %.1f MiB",rw::vulkan::getTextureMemoryUsed()/1048576.);GpuPassProfile::Result sample;if(rw::vulkan::getGpuPassProfile(&sample)){char text[256];GpuPassProfile::FormatTimingRows(sample,text,sizeof(text));ImGui::TextUnformatted(text);}else ImGui::TextUnformatted("GPU timings unavailable");
            ImGui::Checkbox("Detailed rendering counters",&showDetails);if(showDetails)ImGui::TextUnformatted(androidgame::QuestProfilerRenderingDetails());
            for(int eye=0;eye<activeSettings.views;++eye){int extent=0;auto pixels=androidgame::QuestProfilerOcclusionMap(eye,&extent);if(pixels&&extent){ImGui::Text("Eye %d occlusion map",eye);auto origin=ImGui::GetCursorScreenPos();auto draw=ImGui::GetWindowDrawList();float scale=128.f/extent;for(int y=0;y<extent;++y)for(int x=0;x<extent;++x){unsigned char p=pixels[y*extent+x];draw->AddRectFilled(ImVec2(origin.x+x*scale,origin.y+y*scale),ImVec2(origin.x+(x+1)*scale,origin.y+(y+1)*scale),IM_COL32(p,p,p,255));}ImGui::Dummy(ImVec2(128,128));}}
            if(!batch){
            if(ImGui::Button("Save screenshot")){auto root=NewRun(std::filesystem::u8path(user)/"screenshots");Desktop::RequestSnapshot((root/"view.bmp").u8string());}ImGui::SameLine();if(ImGui::Button("RenderDoc next frame"))captureRenderDoc=true;
            if(ImGui::Button(androidgame::QuestProfilerIsRecording()?"Stop profiler export":"Start profiler export"))androidgame::QuestProfilerSetReplayProfiling(androidgame::QuestProfilerIsRecording()?1:2);
                    }
        ImGui::End();
    }
    void UpdateMouseCapture(){
        const auto &in=Desktop::Input();
        const bool captured=mouseFlight.Update(in.focused,Desktop::Settings().keyboard,in.buttons[1],ImGui::IsAnyWindowHovered()||ImGui::GetIO().WantCaptureMouse,batch||playing);
        Desktop::SetToolCameraCapture(captured);
    }
    void Ui(){
        if(Desktop::Settings().statistics)Statistics();
        if(!Desktop::Settings().panel)return;
        ImGui::SetNextWindowSize(ImVec2(480,760),ImGuiCond_FirstUseEver);ImGui::Begin("Renderer debug viewer (F1)",&Desktop::Settings().panel);ImGui::TextUnformatted("Desktop GPU measurements; no XR runtime or player");
        if(!error.empty())ImGui::TextWrapped("%s",error.c_str());auto &s=scenario.settings;
        if(batch){ImGui::Text("Pass %d / %d, frame %d",repeat+1,scenario.repeats,frame);ImGui::TextUnformatted("Settings are locked during measurement.");if(ImGui::Button("Cancel batch"))stopBatch=true;ImGui::SameLine();if(ImGui::Button("Quit / Exit"))platform::requestExit();ImGui::End();return;}
        if(ImGui::CollapsingHeader("Camera")){
            float p[3];for(int i=0;i<3;++i)p[i]=float(camera.position[i]);if(ImGui::InputFloat3("Position",p))for(int i=0;i<3;++i)camera.position[i]=p[i];
            float q[4];for(int i=0;i<4;++i)q[i]=float(camera.rotation[i]);if(ImGui::InputFloat4("Orientation quaternion",q)){double length=0;for(float n:q)length+=n*n;if(length>.000001)for(int i=0;i<4;++i)camera.rotation[i]=q[i]/std::sqrt(length);}
            if(MatchedProfile::active)ImGui::Text("Profile horizontal FOV: %.2f degrees",ProfileHorizontalFov(MatchedProfile::requested));else Real("Horizontal FOV",camera.fov,10,170);Real("Near clip",s.nearClip,.01f,10);Real("Far clip",s.farClip,50,10000);Real("Flight speed",s.speed,.1f,1000);Real("Sensitivity",s.sensitivity,.01f,1);
            ImGui::Checkbox("Keyboard / mouse",&Desktop::Settings().keyboard);ImGui::Checkbox("Gamepad",&Desktop::Settings().gamepad);if(ImGui::Button("Reset camera")){camera=Camera();AnglesFromCamera();}ImGui::TextWrapped("Hold right mouse over the viewport to fly/look with WASD/QE. Release to use the UI. Shift fast, Ctrl slow. F1 settings; F2 statistics.");
        }
        if(ImGui::CollapsingHeader("Display and views")){
            int mode=int(Desktop::Settings().mode);if(ImGui::Combo("Window mode",&mode,"Windowed (bordered)\0Borderless\0Desktop fullscreen\0")){s.windowMode=mode;Desktop::SetMode(Desktop::WindowMode(mode));}
            ImGui::InputInt("Window width",&s.windowWidth);ImGui::InputInt("Window height",&s.windowHeight);if(ImGui::Button("Apply window size"))SDL_SetWindowSize(static_cast<SDL_Window*>(Desktop::WindowHandle()),std::clamp(s.windowWidth,256,8192),std::clamp(s.windowHeight,256,8192));
            ImGui::Checkbox("VSync",&s.vsync);Real("FPS cap (0 = unlimited)",s.fps,0,240);
            int view=s.views-1;if(ImGui::Combo("Render views (restart)",&view,"Mono\0Synthetic stereo\0"))s.views=view+1;
            ImGui::Combo("Present",&s.eye,"Left / mono\0Right\0Side by side\0");Real("Eye separation (metres)",s.ipd,0,.2f);
            ImGui::InputInt("Render width (restart)",&s.width);ImGui::InputInt("Render height (restart)",&s.height);
            ImGui::Text("Active: %d x %d, %d view(s)",activeSettings.width,activeSettings.height,activeSettings.views);
            ImGui::TextWrapped("Restart-required changes are saved with Save viewer settings.");
        }
        if(ImGui::CollapsingHeader("Environment")){
            Real("Time of day",s.hour,0,23.999f);ImGui::Checkbox("Advance clock",&s.clock);ImGui::Combo("Weather",&s.weather,"Sunny\0Cloudy\0Rainy\0Foggy\0");ImGui::Combo("Blend to",&s.nextWeather,"Sunny\0Cloudy\0Rainy\0Foggy\0");Real("Weather blend",s.weatherBlend,0,1);
            ImGui::Checkbox("Pause environmental simulation",&s.paused);ImGui::Checkbox("Ambient vehicles",&s.traffic);ImGui::Checkbox("Ambient pedestrians",&s.pedestrians);ImGui::TextWrapped("Ambient activity adds variation; camera scrubbing does not rewind simulation.");
        }
        if(ImGui::CollapsingHeader("Graphics")){
            size_t count=0;auto controls=RendererSettings::Controls(count);for(size_t i=0;i<count;++i){auto &c=controls[i];int value=scenario.settings.graphics.count(c.name)?scenario.settings.graphics.at(c.name):c.get();std::string label=c.name;if(c.restart)label+=" (restart)";
                bool changed=false;if(std::string(c.name)=="CarReflectionScale"){int index=value==8?3:value==4?2:value==2?1:0;changed=ImGui::Combo(label.c_str(),&index,"Full\0Half\0Quarter\0Eighth\0");value=1<<index;}else if(std::string(c.name)=="WorldLodMode")changed=ImGui::Combo("World LOD",&value,"Off - high detail only\0On - current distances\0Performance - earlier LOD\0Aggressive - early LOD / hide\0");else changed=ImGui::SliderInt(label.c_str(),&value,c.minimum,c.maximum);
                if(changed){if(std::string(c.name)=="DynamicLightMax")value&=~1;scenario.settings.graphics[c.name]=value;if(!c.restart)c.set(value);if(MatchedProfile::active)MatchedProfile::dirty=true;}}
            int msaa=s.graphics.count("MsaaSamples")?s.graphics["MsaaSamples"]:1,index=msaa==4?2:msaa==2?1:0;if(ImGui::Combo("MSAA (restart)",&index,"Off\0x2\0x4\0"))s.graphics["MsaaSamples"]=index==2?4:index==1?2:1;
            RendererSettings::ApplySideEffects();
        }
        if(ImGui::CollapsingHeader("Visualization")){
            ImGui::Combo("Geometry",&s.mesh,"Solid\0Wireframe xray\0Solid + wireframe\0Hidden-line wireframe\0");ImGui::Combo("Shading",&s.shading,"Normal lit\0Unlit albedo\0Solid grey\0View normals\0Vertex color\0UV checker\0Vertex lighting\0Linear depth\0Overdraw xray\0Depth-tested overdraw\0");
            ImGui::Checkbox("Collision bounds",&s.bounds);ImGui::Checkbox("Collision shapes",&s.collision);ImGui::Checkbox("Velocity",&s.velocity);ImGui::SliderInt("Overlay range",&s.range,1,2000);const char *layers[]={"Sky/clouds","Buildings","Roads/terrain","Pedestrians","Vehicles","Objects/props","Water","Particles/effects","Shadows"};for(int i=0;i<9;++i)ImGui::Checkbox(layers[i],&s.layers[i]);
        }
        if(ImGui::CollapsingHeader("Bookmarks and camera paths")){
            ImGui::InputText("Name",name,sizeof(name));if(ImGui::Button("Save bookmark")){bookmarks[name]=camera;SavePreferences();}ImGui::SameLine();if(ImGui::Button("Load bookmark")){auto i=bookmarks.find(name);if(i!=bookmarks.end())camera=i->second;else error="Bookmark not found";}
            for(auto &b:bookmarks)if(ImGui::Selectable(b.first.c_str())){camera=b.second;std::snprintf(name,sizeof(name),"%s",b.first.c_str());}
            if(ImGui::Button(recording?"Stop recording":"Record flight")){recording=!recording;recordTime=0;if(recording){playing=false;scenario.path={{0,camera}};}}ImGui::SameLine();if(ImGui::Button("Add keyframe")){double time=scenario.path.empty()?0:scenario.path.back().time+1;scenario.path.push_back({time,camera});}
            if(!scenario.path.empty()){
                if(ImGui::Button(playing?"Stop playback":"Play path")){playing=!playing;pathTime=0;recording=false;}ImGui::SameLine();ImGui::Checkbox("Pause playback",&pausedPath);ImGui::Checkbox("Loop",&loop);Real("Playback speed",playbackSpeed,.05f,4);
                if(Real("Scrub camera",pathTime,0,float(scenario.path.back().time)))camera=Sample(scenario.path,pathTime);
                ImGui::SliderInt("Keyframe",&keySelection,0,int(scenario.path.size())-1);keySelection=std::clamp(keySelection,0,int(scenario.path.size())-1);auto &key=scenario.path[keySelection];float time=float(key.time);
                if(ImGui::InputFloat("Keyframe seconds",&time)){double low=keySelection?scenario.path[keySelection-1].time+.001:0,high=keySelection+1<int(scenario.path.size())?scenario.path[keySelection+1].time-.001:86400;key.time=keySelection?std::clamp(double(time),low,high):0;}
                if(ImGui::Button("Replace keyframe with camera"))key.camera=camera;ImGui::SameLine();if(ImGui::Button("Go to keyframe"))camera=key.camera;
                if(ImGui::Button("Delete keyframe")){scenario.path.erase(scenario.path.begin()+keySelection);if(!scenario.path.empty()){double base=scenario.path.front().time;for(auto &k:scenario.path)k.time-=base;}}
            }
            if(ImGui::Button("Save named path")){Json j=Json::Map();j.object={{"version",1},{"path",EncodePath(scenario.path)}};DebugJson::Save(PathDocument(),j);}ImGui::SameLine();if(ImGui::Button("Load named path")){auto j=DebugJson::Load(PathDocument());j.At("version").Int(1,1);scenario.path=DecodePath(j.At("path"));}
        }
        if(ImGui::CollapsingHeader("Profiles and batch automation")){
            ImGui::InputText("Quest profile",profile,sizeof(profile));ImGui::TextWrapped("Profile changes require restart. Save settings then launch again to apply dimensions and stereo geometry.");
            ImGui::Text("Profile: %s%s",MatchedProfile::active?"Quest workload":"native desktop",MatchedProfile::dirty?" (modified)":"");
            if(MatchedProfile::active){ImGui::Text("Captured eyes: %.0f x %.0f; source display %.0f Hz",MatchedProfile::requested.at("width"),MatchedProfile::requested.at("height"),MatchedProfile::requested.at("hz"));ImGui::TextWrapped("Captured eye poses and asymmetric FOV override native eye separation/FOV. This is a desktop workload; source display Hz is metadata, not headset pacing.");}
            ImGui::InputText("Scenario file",document,sizeof(document));if(ImGui::Button("Save scenario")){scenario.camera=camera;scenario.questProfile=profile;DebugJson::Save(Document(),Encode(scenario));}ImGui::SameLine();if(ImGui::Button("Load scenario")){auto source=Document();auto loaded=DecodeScenario(DebugJson::Load(source));if(!loaded.questProfile.empty())loaded.questProfile=(source.parent_path()/std::filesystem::u8path(loaded.questProfile)).lexically_normal().u8string();scenario=loaded;std::snprintf(profile,sizeof(profile),"%s",scenario.questProfile.c_str());camera=scenario.camera;Graphics(false);}
            int content=scenario.content=="classic"?1:scenario.content=="xbox"?2:scenario.content=="upstate"?3:0;if(ImGui::Combo("Content (restart)",&content,"Installation selection\0Classic\0Original Xbox\0Upstate\0")){const char *names[]={"selected","classic","xbox","upstate"};scenario.content=names[content];}
            ImGui::Text("Active content: %s",QuestUpstate::Active()?"Upstate":ModelSets::GetSourceName(ModelSets::GetActive()));
            Real("Fixed timestep",scenario.timestep,1.f/240,1.f/10);Real("Duration (seconds)",scenario.duration,.1f,600);ImGui::SliderInt("Warmup frames",&scenario.warmup,0,600);ImGui::SliderInt("Repetitions",&scenario.repeats,1,20);
            ImGui::InputInt("Screenshot frame (zero based)",&screenshotFrame);
            if(ImGui::Button("Add screenshot point")){Check(screenshotFrame>=0&&screenshotFrame<int(std::ceil(scenario.duration/scenario.timestep)),"Screenshot point is outside the measured duration");scenario.screenshots.push_back(screenshotFrame);std::sort(scenario.screenshots.begin(),scenario.screenshots.end());scenario.screenshots.erase(std::unique(scenario.screenshots.begin(),scenario.screenshots.end()),scenario.screenshots.end());}
            for(size_t i=0;i<scenario.screenshots.size();++i){std::string label="Remove screenshot at frame "+std::to_string(scenario.screenshots[i]);if(ImGui::Button(label.c_str())){scenario.screenshots.erase(scenario.screenshots.begin()+i);break;}}
            if(ImGui::Button(batch?"Cancel batch":"Run scenario")){if(batch)stopBatch=true;else startBatch=true;}ImGui::Text("Pass %d / %d, frame %d",repeat+1,scenario.repeats,frame);
        }
        if(ImGui::Button("Save viewer settings")){scenario.questProfile=profile;SavePreferences();}ImGui::SameLine();if(ImGui::Button("Quit / Exit"))platform::requestExit();ImGui::End();ApplyDisplay();
    }
    Json Summary(bool complete,const std::string &reason){Json j=Json::Map();j.object={{"version",1},{"completed",complete},{"reason",reason},{"source_identity",LCVR_SOURCE_IDENTITY},{"device",device},{"target","desktop renderer debug"},{"content",QuestUpstate::Active()?"upstate":scenario.content},{"scenario",Encode(scenario)},{"active_settings",Encode(activeSettings)},{"profile_modified",MatchedProfile::dirty},{"ambient_nondeterministic",scenario.settings.traffic||scenario.settings.pedestrians},{"presentation_included",true},{"instrumentation",androidgame::QuestProfilerIsEnabled()},{"measured_frames",double(results.size())}};
        j.object["world_lod_mode"]=VrWorldLod::Mode();
        j.object["lod_distance_multiplier"]=LodDistanceMultiplier();
        const auto lod=VrWorldLod::GetStats();Json lodStats=Json::Map();lodStats.object={{"high",int(lod.highSelections)},{"lower",int(lod.lowerSelections)},{"lod",int(lod.lodSelections)},{"suppressed",int(lod.suppressedLod)}};j.object["world_lod_stats"]=lodStats;
        j.object["content_identity"]=contentIdentity;
        j.object["active_content"]=QuestUpstate::Active()?"upstate":ModelSets::IsModernActive()?"xbox":"classic";
        Json capabilities=Json::Map();auto &features=sessionGraphics.enabledFeatures;
        capabilities.object={{"anisotropy",features.samplerAnisotropy!=0},{"max_anisotropy",double(sessionGraphics.maxSamplerAnisotropy)},{"fragment_atomics",features.fragmentStoresAndAtomics!=0},{"bc",features.textureCompressionBC!=0},{"astc",features.textureCompressionASTC_LDR!=0},{"etc2",features.textureCompressionETC2!=0}};
        j.object["capabilities"]=capabilities;j.object["renderdoc_attached"]=androidgame::QuestRenderDocAvailable();
        if(MatchedProfile::active){
            j.object["quest_profile_identity"]=MatchedProfile::Identity(MatchedProfile::requested);Json geometry=Json::Map();for(auto &v:MatchedProfile::requested)if(v.first.rfind("eye",0)==0||v.first=="width"||v.first=="height"||v.first=="hz")geometry.object[v.first]=v.second;j.object["profile_geometry"]=geometry;
            Json deviations=Json::List();
            for(auto &setting:activeSettings.graphics){auto requested=MatchedProfile::requested.find("setting_"+setting.first);if(requested!=MatchedProfile::requested.end()&&requested->second!=setting.second)deviations.array.emplace_back(setting.first);}
            auto differs=[&](const char *key,double effective){auto requested=MatchedProfile::requested.find("setting_"+std::string(key));if(requested!=MatchedProfile::requested.end()&&requested->second!=effective)deviations.array.emplace_back(key);};
            differs("ModelSet",ModelSets::IsModernActive()?1:0);
            if(!scenario.settings.traffic)differs("CarTrafficPercent",0);
            if(!scenario.settings.pedestrians)differs("PedTrafficPercent",0);
            j.object["profile_modified"]=MatchedProfile::dirty||!deviations.array.empty();j.object["profile_deviations"]=deviations;
            Json excluded=Json::List();for(const char *system:{"player","missions","weapons","HUD","save loading"})excluded.array.emplace_back(system);j.object["viewer_exclusions"]=excluded;
        }
        Json passes=Json::List();for(int pass=0;pass<scenario.repeats;++pass){std::vector<double> cpu,gpu,interval;for(auto &f:results)if(f.repeat==pass){cpu.push_back(f.cpu);interval.push_back(f.interval);if(f.gpu.status==GpuPassProfile::Complete)gpu.push_back(f.gpu.frameMs);}Json p=Json::Map();p.object["repeat"]=pass+1;auto stats=[](std::vector<double> values){if(values.empty())return Json();std::sort(values.begin(),values.end());Json v=Json::Map();v.object={{"count",int(values.size())},{"mean",std::accumulate(values.begin(),values.end(),0.)/values.size()},{"p50",values[size_t((values.size()-1)*.5)]},{"p95",values[size_t((values.size()-1)*.95)]},{"p99",values[size_t((values.size()-1)*.99)]}};return v;};p.object["cpu_ms"]=stats(cpu);p.object["gpu_ms"]=stats(gpu);p.object["frame_interval_ms"]=stats(interval);passes.array.push_back(p);}j.object["passes"]=passes;return j;
    }
    void Export(bool complete,const std::string &reason){
        if(run.empty())return;
        std::ofstream csv(run/"frames.csv",std::ios::binary|std::ios::trunc);csv<<"frame_id,repeat,frame,simulation_seconds,cpu_ms,update_ms,render_ms,present_ms,interval_ms,gpu_source_frame,gpu_ms,gpu_scene_ms,gpu_post_ms,camera_x,camera_y,camera_z,rotation_x,rotation_y,rotation_z,rotation_w,fov\n";csv<<std::setprecision(10);
        for(auto &f:results){csv<<f.id<<','<<f.repeat+1<<','<<f.index<<','<<f.simulation<<','<<f.cpu<<','<<f.update<<','<<f.render<<','<<f.present<<','<<f.interval<<',';if(f.gpu.status==GpuPassProfile::Complete){csv<<f.gpu.frameId<<','<<f.gpu.frameMs<<',';if(f.gpu.mask&(1u<<GpuPassProfile::Scene))csv<<f.gpu.ms[GpuPassProfile::Scene];csv<<',';if(f.gpu.mask&(1u<<GpuPassProfile::Postprocess))csv<<f.gpu.ms[GpuPassProfile::Postprocess];}else csv<<",,,";for(double v:f.camera.position)csv<<','<<v;for(double v:f.camera.rotation)csv<<','<<v;csv<<','<<f.camera.fov<<'\n';}csv.close();Check(bool(csv),"Cannot write frame CSV");DebugJson::Save(run/"summary.json",Summary(complete,reason));
    }
    void CollectGpu(){
        GpuPassProfile::Result sample;
        if(rw::vulkan::getGpuPassProfile(&sample)){
            auto row=rows.find(sample.frameId);
            if(row!=rows.end())results[row->second].gpu=sample;
        }
    }
    void BeginBatch(){
        try{scenario=DecodeScenario(Encode(scenario));}catch(const std::exception &e){throw std::invalid_argument(e.what());}CheckInput(scenario.settings.width==activeSettings.width&&scenario.settings.height==activeSettings.height&&scenario.settings.views==activeSettings.views,"Apply restart-required settings before running this scenario");
        size_t count=0;auto controls=RendererSettings::Controls(count);
        for(size_t i=0;i<count;++i)if(controls[i].restart){auto requested=scenario.settings.graphics.find(controls[i].name);if(requested!=scenario.settings.graphics.end())CheckInput(requested->second==activeSettings.graphics.at(controls[i].name),"Restart to apply pending renderer settings before measurement");}
        auto msaa=scenario.settings.graphics.find("MsaaSamples");if(msaa!=scenario.settings.graphics.end())CheckInput(msaa->second==int(rw::vulkan::getSceneSampleCount()),"Restart to apply pending MSAA before measurement");
        CheckInput(scenario.questProfile==activeProfile && activeProfile==std::string(profile),"Restart to apply the selected Quest profile before measurement");
        const std::string content=QuestUpstate::Active()?"upstate":ModelSets::IsModernActive()?"xbox":"classic";
        CheckInput(scenario.content=="selected"||scenario.content==content,"Restart to apply the selected content before measurement");
        Graphics(false);activeSettings=scenario.settings;activeSettings.graphics["MsaaSamples"]=int(rw::vulkan::getSceneSampleCount());
        scenario.settings.vsync=false;scenario.settings.fps=0;ApplyDisplay();results.clear();rows.clear();repeat=0;frame=-scenario.warmup;camera=scenario.path.empty()?scenario.camera:scenario.path.front().camera;simulation=0;playing=false;recording=false;
        activeSettings.vsync=false;activeSettings.fps=0;
        run=NewRun(output.empty()?std::filesystem::u8path(user)/"runs":std::filesystem::u8path(output));DebugJson::Save(run/"scenario.json",Encode(scenario));DebugJson::Save(run/"summary.json",Summary(false,"Running"));
        contentIdentity=BenchmarkIdentity::Assets(ModelSets::ContentRoot());DebugJson::Save(run/"identity.json",Summary(false,"Prepared"));
        Desktop::Settings().panel=false;ResetWorld(scenario.settings,camera);batch=true;startBatch=false;
    }
};
static Client *current=nullptr;
static void DrawUi(){try{current->Ui();}catch(const std::exception &e){current->error=e.what();}}
int Run(Scenario scenario,const std::string &user,const std::string &output,bool batch,unsigned frameLimit){
    Client c(std::move(scenario));c.user=user;c.output=output;c.batch=false;c.frameLimit=frameLimit;current=&c;
    Vulkan gpu;bool engine=false;int result=0;
    try{
        c.Graphics(true);c.SnapshotSettings();c.activeSettings=c.scenario.settings;c.activeProfile=c.scenario.questProfile;std::snprintf(c.profile,sizeof(c.profile),"%s",c.scenario.questProfile.c_str());
        auto &o=Desktop::Settings();o.panel=!batch;o.capture=false;o.statistics=!batch;o.fitSource=true;o.vsync=c.scenario.settings.vsync;c.ApplyDisplay();Desktop::SetMode(Desktop::WindowMode(c.scenario.settings.windowMode));SDL_SetWindowSize(static_cast<SDL_Window*>(Desktop::WindowHandle()),c.scenario.settings.windowWidth,c.scenario.settings.windowHeight);SDL_SetWindowTitle(static_cast<SDL_Window*>(Desktop::WindowHandle()),"Liberty City renderer debug viewer");
        gpu.Open(c.activeSettings);c.device=gpu.deviceName;
        auto &a=gpu.context;sessionGraphics={a.instance,a.physicalDevice,a.device,a.fragmentStoresAndAtomicsEnabled,a.enabledFeatures,a.maxSamplerAnisotropy,a.queue,a.queueFamilyIndex,a.width,a.height,a.renderScaleEffectivePercent,a.viewCount,a.colourFormat};
        androidgame::QuestProfilerSetLogDirectory(user.c_str());Check(androidgame::Initialise(a),"Initialize shared renderer");engine=true;InitializeWorld();if(!batch)ResetWorld(c.scenario.settings,c.camera);c.AnglesFromCamera();
        auto bookmarks=std::filesystem::u8path(user)/"bookmarks.json";if(std::filesystem::exists(bookmarks)){auto j=DebugJson::Load(bookmarks);j.At("version").Int(1,1);for(auto &entry:j.At("bookmarks").object)c.bookmarks[entry.first]=DecodeCamera(entry.second);}
        Desktop::SetToolUi(DrawUi);androidgame::QuestProfilerSetEnabled(false);androidgame::QuestProfilerSetDisplayMode(0);androidgame::QuestProfilerSetDesktopObserver(c.scenario.settings.profiling);
        if(batch)c.BeginBatch();
        auto previous=Clock::now();unsigned rendered=0;bool finished=false;
        while(!platform::exitRequested()&&!Desktop::QuitRequested()&&(!frameLimit||rendered<frameLimit)){
            auto start=Clock::now();const double interval=Milliseconds(previous,start);double dt=std::clamp(interval/1000.,.001,.1);previous=start;Desktop::Pump();Desktop::BeginUi();c.UpdateMouseCapture();
            if(c.startBatch){c.startBatch=false;try{c.BeginBatch();}catch(const std::invalid_argument &e){c.error=e.what();Desktop::Settings().panel=true;}}if(c.stopBatch){c.Export(false,"Cancelled by user");c.batch=false;c.stopBatch=false;if(batch){result=1;break;}}
            auto &s=c.scenario.settings;Settings effective=s;effective.width=c.activeSettings.width;effective.height=c.activeSettings.height;effective.views=c.activeSettings.views;
            if(c.batch){dt=c.scenario.timestep;c.pathTime=c.frame<0?0:PlaybackTime(uint64_t(c.frame),dt);c.camera=c.scenario.path.empty()?c.scenario.camera:Sample(c.scenario.path,c.pathTime);}
            else {c.Fly(dt);if(c.playing&&!c.pausedPath){c.pathTime+=dt*c.playbackSpeed;if(c.pathTime>c.scenario.path.back().time){if(c.loop&&c.scenario.path.back().time>0)c.pathTime=std::fmod(c.pathTime,c.scenario.path.back().time);else{c.pathTime=c.scenario.path.back().time;c.playing=false;}}c.camera=Sample(c.scenario.path,c.pathTime);}if(c.recording){c.recordTime+=dt;if(c.recordTime>c.scenario.path.back().time)c.scenario.path.push_back({c.recordTime,c.camera});}}
            if(!s.paused)c.simulation=c.batch?double(c.frame+c.scenario.warmup+1)*dt:c.simulation+dt;
            androidgame::QuestProfilerSetDesktopObserver(s.profiling);androidgame::QuestProfilerBeginAppFrame();rw::vulkan::setGpuProfileFrame(c.nextFrame,androidgame::QuestProfilerNowNanoseconds());
            auto updateStart=Clock::now();UpdateWorld(effective,c.camera,c.simulation,dt);auto updateEnd=Clock::now();
            bool doc=c.captureRenderDoc;if(doc){androidgame::QuestRenderDocBegin(reinterpret_cast<void*>(gpu.context.instance),(std::filesystem::u8path(user)/"renderdoc-captures/viewer").u8string().c_str());c.captureRenderDoc=false;}
            androidgame::QuestProfilerBeginVkBegin();Check(rw::vulkan::beginFrame(gpu.image,gpu.view),"Begin viewer frame");androidgame::QuestProfilerEndVkBegin();androidgame::QuestProfilerBeginStep();RenderWorld(effective);androidgame::QuestProfilerEndStep();androidgame::QuestProfilerBeginVkEnd();rw::vulkan::endFrame();androidgame::QuestProfilerEndVkEnd();auto renderEnd=Clock::now();
            if(c.batch&&c.frame>=0&&std::binary_search(c.scenario.screenshots.begin(),c.scenario.screenshots.end(),c.frame))Desktop::RequestSnapshot((c.run/("pass-"+std::to_string(c.repeat+1)+"-frame-"+std::to_string(c.frame)+".bmp")).u8string());
            unsigned presented=Desktop::PresentedFrames();Desktop::Present(gpu.image,gpu.view,gpu.context.width,gpu.context.height,0,gpu.context.viewCount);auto end=Clock::now();
            if(doc)androidgame::QuestRenderDocEnd(true);androidgame::QuestProfilerEndAppFrame();
            c.lastCpu=Milliseconds(start,end);c.lastUpdate=Milliseconds(updateStart,updateEnd);c.lastRender=Milliseconds(updateEnd,renderEnd);c.lastPresent=Milliseconds(renderEnd,end);c.history.push_back(float(c.lastCpu));if(c.history.size()>240)c.history.erase(c.history.begin());
            if(c.batch){Check(Desktop::PresentedFrames()>presented,"Batch presentation unavailable (window minimized or display lost)");if(c.frame>=0){Frame f;f.id=c.nextFrame;f.repeat=c.repeat;f.index=c.frame;f.simulation=c.pathTime;f.cpu=c.lastCpu;f.update=c.lastUpdate;f.render=c.lastRender;f.present=c.lastPresent;f.interval=interval;f.camera=c.camera;c.rows[f.id]=c.results.size();c.results.push_back(f);}
                c.CollectGpu();
                ++c.frame;if(c.frame>=int(std::ceil(c.scenario.duration/dt))){
                    // Retire the renderer's delayed query slots without adding
                    // drain frames to the measured workload or its statistics.
                    for(int drain=0;drain<3;++drain){rw::vulkan::setGpuProfileFrame(0,0);Check(rw::vulkan::beginFrame(gpu.image,gpu.view),"Drain GPU timestamps");c.CollectGpu();rw::vulkan::endFrame();}
                    c.Export(false,"Pass complete");++c.repeat;if(c.repeat>=c.scenario.repeats){c.Export(true,"");c.batch=false;finished=true;if(batch)break;}else{c.frame=-c.scenario.warmup;c.simulation=0;c.camera=c.scenario.path.empty()?c.scenario.camera:c.scenario.path.front().camera;ResetWorld(effective,c.camera);}
                }
            }
            ++c.nextFrame;++rendered;Desktop::EndUi();if(!c.batch&&s.fps>0)std::this_thread::sleep_until(start+std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1/s.fps)));
        }
        if(c.batch){c.Export(false,"Interrupted before completion");result=1;}if(batch&&!finished)result=1;
        if(!batch)c.SavePreferences();androidgame::QuestProfilerSetReplayProfiling(1);androidgame::QuestProfilerSetDesktopObserver(false);Desktop::SetToolUi(nullptr);ShutdownWorld();androidgame::Shutdown();engine=false;Desktop::ShutdownGpu();gpu.Close();current=nullptr;return result;
    }catch(const std::exception &e){
        try{c.Export(false,e.what());}catch(...){}Desktop::SetToolUi(nullptr);if(engine){ShutdownWorld();androidgame::Shutdown();}Desktop::ShutdownGpu();gpu.Close();current=nullptr;throw;
    }
}
}
