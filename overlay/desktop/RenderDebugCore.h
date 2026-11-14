#pragma once
#include "DebugJson.h"
#include <algorithm>
#include <array>
#include <limits>
#include <cwctype>

namespace RenderDebug {
using Json=DebugJson::Value;
struct Camera {
    std::array<double,3> position{{-180,180,35}};
    // Quaternion rotates camera-local right/+X, forward/+Y, up/+Z into world.
    std::array<double,4> rotation{{0,0,0,1}};
    double fov=90;
};
struct Keyframe {double time=0;Camera camera;};
inline void Check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
inline void CheckInput(bool ok,const char *message){if(!ok)throw std::invalid_argument(message);}
inline double Range(const Json &v,double low,double high){double n=v.Num();Check(n>=low&&n<=high,"Value out of range");return n;}
inline void Fields(const Json &v,std::initializer_list<const char*> allowed){Check(v.type==Json::Object,"Expected JSON object");for(auto &entry:v.object){bool found=false;for(auto key:allowed)found|=entry.first==key;Check(found,"Unknown document field");}}
template<size_t N> inline Json Array(const std::array<double,N> &a){Json r=Json::List();for(double n:a)r.array.emplace_back(n);return r;}
template<size_t N> inline std::array<double,N> Numbers(const Json &v){Check(v.type==Json::Array&&v.array.size()==N,"Invalid vector size");std::array<double,N> r{};for(size_t i=0;i<N;++i)r[i]=Range(v.array[i],-100000,100000);return r;}
inline void Validate(const Camera &c){for(auto n:c.position)Check(std::isfinite(n)&&std::abs(n)<=100000,"Invalid camera position");double norm=0;for(auto n:c.rotation){Check(std::isfinite(n),"Invalid quaternion");norm+=n*n;}Check(std::abs(norm-1)<.001,"Camera quaternion must be normalized");Check(std::isfinite(c.fov)&&c.fov>=10&&c.fov<=170,"Invalid camera FOV");}
inline Json Encode(const Camera &c){Validate(c);Json j=Json::Map();j.object={{"position",Array(c.position)},{"rotation",Array(c.rotation)},{"fov",c.fov}};return j;}
inline Camera DecodeCamera(const Json &j){Fields(j,{"position","rotation","fov"});Camera c;c.position=Numbers<3>(j.At("position"));c.rotation=Numbers<4>(j.At("rotation"));c.fov=Range(j.At("fov"),10,170);Validate(c);return c;}
inline void Validate(const std::vector<Keyframe> &path){Check(!path.empty()&&path.size()<=100000,"Path requires 1..100000 keyframes");double previous=-1;for(const auto &k:path){Check(std::isfinite(k.time)&&k.time>=0&&k.time>previous&&k.time<=86400,"Path times must increase, in 0..86400 seconds");Validate(k.camera);previous=k.time;}Check(path.front().time==0,"Path must start at zero");}
inline Camera Sample(const std::vector<Keyframe> &path,double time){
    Check(!path.empty(),"Empty path");if(time<=path.front().time)return path.front().camera;if(time>=path.back().time)return path.back().camera;
    auto b=std::upper_bound(path.begin(),path.end(),time,[](double t,const Keyframe &k){return t<k.time;});const auto &a=*(b-1);double t=(time-a.time)/(b->time-a.time);Camera c;
    for(int i=0;i<3;++i)c.position[i]=a.camera.position[i]+t*(b->camera.position[i]-a.camera.position[i]);c.fov=a.camera.fov+t*(b->camera.fov-a.camera.fov);
    auto q=b->camera.rotation;double dot=0;for(int i=0;i<4;++i)dot+=a.camera.rotation[i]*q[i];if(dot<0){dot=-dot;for(auto &n:q)n=-n;}
    double wa=1-t,wb=t;if(dot<.9995){double angle=std::acos(std::clamp(dot,-1.,1.));wa=std::sin((1-t)*angle)/std::sin(angle);wb=std::sin(t*angle)/std::sin(angle);}
    double norm=0;for(int i=0;i<4;++i){c.rotation[i]=wa*a.camera.rotation[i]+wb*q[i];norm+=c.rotation[i]*c.rotation[i];}for(auto &n:c.rotation)n/=std::sqrt(norm);return c;
}
inline Json EncodePath(const std::vector<Keyframe> &path){Validate(path);Json j=Json::List();for(auto &k:path){Json row=Json::Map();row.object={{"time",k.time},{"camera",Encode(k.camera)}};j.array.push_back(row);}return j;}
inline std::vector<Keyframe> DecodePath(const Json &j){Check(j.type==Json::Array,"Expected keyframe array");std::vector<Keyframe> r;for(auto &k:j.array){Fields(k,{"time","camera"});r.push_back({k.At("time").Num(),DecodeCamera(k.At("camera"))});}Validate(r);return r;}
struct Settings {
    int width=1280,height=720,views=1,eye=0,windowMode=0,windowWidth=1280,windowHeight=720;
    bool vsync=false,clock=false,paused=false,traffic=false,pedestrians=false,profiling=true;
    double fps=0,nearClip=.1,farClip=1000,ipd=.064,speed=30,sensitivity=.15,hour=12,weatherBlend=0;
    int weather=0,nextWeather=0;
    std::map<std::string,int> graphics;
    int mesh=0,shading=0,range=100;bool bounds=false,collision=false,velocity=false;
    std::array<bool,9> layers{{true,true,true,true,true,true,true,true,true}};
};
inline Json Encode(const Settings &s){Json j=Json::Map();
    j.object={{"width",s.width},{"height",s.height},{"views",s.views},{"eye",s.eye},{"window_mode",s.windowMode},{"window_width",s.windowWidth},{"window_height",s.windowHeight},
    {"vsync",s.vsync},{"clock",s.clock},{"paused",s.paused},{"traffic",s.traffic},{"pedestrians",s.pedestrians},{"profiling",s.profiling},
    {"fps",s.fps},{"near",s.nearClip},{"far",s.farClip},{"ipd",s.ipd},{"speed",s.speed},{"sensitivity",s.sensitivity},{"hour",s.hour},{"weather",s.weather},{"next_weather",s.nextWeather},{"weather_blend",s.weatherBlend},
    {"mesh",s.mesh},{"shading",s.shading},{"range",s.range},{"bounds",s.bounds},{"collision",s.collision},{"velocity",s.velocity}};
    Json g=Json::Map();for(auto &p:s.graphics)g.object[p.first]=p.second;j.object["graphics"]=g;Json l=Json::List();for(bool v:s.layers)l.array.emplace_back(v);j.object["layers"]=l;return j;
}
inline Settings DecodeSettings(const Json &j){
    Fields(j,{"width","height","views","eye","window_mode","window_width","window_height","vsync","clock","paused","traffic","pedestrians","profiling","fps","near","far","ipd","speed","sensitivity","hour","weather","next_weather","weather_blend","graphics","mesh","shading","range","bounds","collision","velocity","layers"});Settings s;
    auto integer=[&](const char *key,int &v,int lo,int hi){if(j.object.count(key))v=j.At(key).Int(lo,hi);};
    auto real=[&](const char *key,double &v,double lo,double hi){if(j.object.count(key))v=Range(j.At(key),lo,hi);};
    auto flag=[&](const char *key,bool &v){if(j.object.count(key))v=j.At(key).Bool();};
    integer("width",s.width,256,8192);integer("height",s.height,256,8192);integer("views",s.views,1,2);integer("eye",s.eye,0,2);integer("window_mode",s.windowMode,0,2);integer("window_width",s.windowWidth,256,8192);integer("window_height",s.windowHeight,256,8192);
    integer("weather",s.weather,0,3);integer("next_weather",s.nextWeather,0,3);integer("mesh",s.mesh,0,3);integer("shading",s.shading,0,9);integer("range",s.range,1,2000);
    real("fps",s.fps,0,1000);real("near",s.nearClip,.01,100);real("far",s.farClip,1,10000);Check(s.farClip>s.nearClip,"Far clip must exceed near clip");real("ipd",s.ipd,0,.2);real("speed",s.speed,.1,1000);real("sensitivity",s.sensitivity,.001,10);real("hour",s.hour,0,23.9999);real("weather_blend",s.weatherBlend,0,1);
    flag("vsync",s.vsync);flag("clock",s.clock);flag("paused",s.paused);flag("traffic",s.traffic);flag("pedestrians",s.pedestrians);flag("profiling",s.profiling);flag("bounds",s.bounds);flag("collision",s.collision);flag("velocity",s.velocity);
    if(j.object.count("graphics")){auto &g=j.At("graphics");Check(g.type==Json::Object,"Expected graphics settings");for(auto &entry:g.object)s.graphics.emplace(entry.first,entry.second.Int(-100000,100000));}
    if(j.object.count("layers")){auto &l=j.At("layers");Check(l.type==Json::Array&&l.array.size()==9,"Expected nine visualization layers");for(int i=0;i<9;++i)s.layers[i]=l.array[i].Bool();}return s;
}
struct Scenario {
    Settings settings;Camera camera;std::vector<Keyframe> path;
    std::string content="selected",questProfile;
    double timestep=1./60,duration=10;
    int warmup=120,repeats=3;std::vector<int> screenshots;
};
inline Json Encode(const Scenario &s){Json j=Json::Map();j.object={{"version",1},{"content",s.content},{"quest_profile",s.questProfile},{"settings",Encode(s.settings)},{"camera",Encode(s.camera)},{"timestep",s.timestep},{"duration",s.duration},{"warmup_frames",s.warmup},{"repetitions",s.repeats}};if(!s.path.empty())j.object["path"]=EncodePath(s.path);Json shots=Json::List();for(auto n:s.screenshots)shots.array.emplace_back(n);j.object["screenshots"]=shots;return j;}
inline Scenario DecodeScenario(const Json &j){
    Fields(j,{"version","content","quest_profile","settings","camera","path","timestep","duration","warmup_frames","repetitions","screenshots"});Check(j.At("version").Int(1,1)==1,"Unsupported scenario version");Scenario s;
    if(j.object.count("settings"))s.settings=DecodeSettings(j.At("settings"));if(j.object.count("camera"))s.camera=DecodeCamera(j.At("camera"));if(j.object.count("path"))s.path=DecodePath(j.At("path"));
    if(j.object.count("content"))s.content=j.At("content").Str();Check(s.content=="selected"||s.content=="classic"||s.content=="xbox"||s.content=="upstate","Unsupported content variant");
    if(j.object.count("quest_profile"))s.questProfile=j.At("quest_profile").Str();if(j.object.count("timestep"))s.timestep=Range(j.At("timestep"),1./1000,1.);
    if(j.object.count("duration"))s.duration=Range(j.At("duration"),s.timestep,86400);else if(!s.path.empty())s.duration=std::max(s.timestep,s.path.back().time);
    if(j.object.count("warmup_frames"))s.warmup=j.At("warmup_frames").Int(0,1000000);if(j.object.count("repetitions"))s.repeats=j.At("repetitions").Int(1,100);
    if(j.object.count("screenshots")){auto &v=j.At("screenshots");Check(v.type==Json::Array,"Expected screenshot frame array");for(auto &n:v.array)s.screenshots.push_back(n.Int(0,int(std::ceil(s.duration/s.timestep))-1));std::sort(s.screenshots.begin(),s.screenshots.end());Check(std::adjacent_find(s.screenshots.begin(),s.screenshots.end())==s.screenshots.end(),"Duplicate screenshot frame");}return s;
}
inline std::filesystem::path NewRun(const std::filesystem::path &root){std::filesystem::create_directories(root);for(unsigned n=1;n<1000000;++n){auto p=root/("run-"+std::to_string(n));if(std::filesystem::create_directory(p))return p;}throw std::runtime_error("No unused run directory");}
inline bool Within(const std::filesystem::path &parent,const std::filesystem::path &child){
    auto base=std::filesystem::weakly_canonical(parent),target=std::filesystem::weakly_canonical(child);
    auto a=base.begin(),b=target.begin();
    for(;a!=base.end();++a,++b){if(b==target.end())return false;
#ifdef _WIN32
        auto left=a->native(),right=b->native();std::transform(left.begin(),left.end(),left.begin(),[](wchar_t c){return std::towlower(c);});std::transform(right.begin(),right.end(),right.begin(),[](wchar_t c){return std::towlower(c);});if(left!=right)return false;
#else
        if(*a!=*b)return false;
#endif
    }return true;
}
inline double PlaybackTime(uint64_t frame,double step){return double(frame)*step;}
}
