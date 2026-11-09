#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

// Numeric, allowlisted workload description. No paths, device IDs, calibration,
// game assets or performance estimates are serialized here.
namespace MatchedProfile {
using Values = std::map<std::string, double>;
inline Values captured, requested;
inline bool active = false, dirty = false;
inline bool benchmark = false;
inline bool IsSetting(const std::string &key) {
    static const std::string keys =
        "|RenderScalePercent|MsaaSamples|Sgsr2Mode|AntiAliasing|ColorFilter|ViceCityColor|"
        "GenerateMipmaps|Ps2AlphaTest|LodFadeTransitions|FxaaIntegerStencil|WorldLodMode|"
        "OpaqueOrderExperiment|WorldOcclusion|WorldBackface|StaticInstances|OpaqueMeshBatch|"
        "MaskedAlphaPassSkip|OpaqueWorldAlphaFastPath|DynamicLights|DynamicLightIntensity|"
        "DynamicLightGlowIntensity|DynamicLightMax|XboxNeoEffects|XboxNeoDroplets|"
        "CarReflections|CarReflectionIntensity|CarReflectionSsr|CarReflectionScale|CarReflectionSsrDistance|"
        "ModernWater|WaterWaves|WaterSpeed|WaterDistortion|WaterReflection|WaterSheen|WaterGlint|WaterSparks|"
        "FoliageSoftness|ModelSet|ModelSetWorld|ModelSetVegetation|ModelSetVehicles|ModelSetPeds|ModelSetWeapons|"
        "PhysicsDirectorMode|PhysicsDirectorPreset|VehicleVisualBudgetMode|OcclusionCulling|OcclusionCullingModeV2|"
        "CpuPerformanceMode|GpuPerformanceMode|RefreshRate|ShadowsEnabled|DistanceFog|FountainQuality|"
        "CarTrafficPercent|PedTrafficPercent|Ragdolls|RagdollBrakePercent|RagdollGripPercent|RagdollShotPercent|RagdollWeightPercent|"
        "VehicleDeformation|VehicleDeformationStrengthPercent|VehicleDeformationRadiusPercent|VehicleDeformationMaxDentCm|VehicleDeformationThresholdPercent|"
        "GameplayHud|HudClock|HudWeaponPanel|WristAmmo|WristClock|WristRadar|WristStatus|WristPanelsInVehicle|";
    return keys.find("|" + key + "|") != std::string::npos;
}
inline bool IsRuntime(const std::string &key) {
    static const std::string keys = "|schema|quest3|width|height|scale|hz|anisotropy|atomics|bc|astc|etc2|max_anisotropy|";
    if (keys.find("|" + key + "|") != std::string::npos) return true;
    for (int eye=0;eye<2;++eye)
        for (const char *part : {"px","py","pz","qx","qy","qz","qw","left","right","up","down"})
            if (key == "eye" + std::to_string(eye) + "_" + part) return true;
    return false;
}
inline std::string Text(const Values &values) {
    std::ostringstream out; out << std::setprecision(9);
    for (const auto &entry : values) out << entry.first << '=' << entry.second << '\n';
    return out.str();
}
inline Values Parse(std::istream &in) {
    Values values; std::string line; size_t total=0;
    while (std::getline(in,line)) {
        total += line.size(); if(total>32768) throw std::runtime_error("Quest profile exceeds 32 KiB.");
        if (!line.empty() && line.back()=='\r') line.pop_back();
        const auto split=line.find('=');
        if(split==std::string::npos) throw std::runtime_error("Invalid Quest profile row.");
        const auto key=line.substr(0,split);
        if (!IsRuntime(key) && !(key.rfind("setting_",0)==0 && IsSetting(key.substr(8))))
            throw std::runtime_error("Unknown Quest profile field.");
        size_t used=0; const auto text=line.substr(split+1); double value=std::stod(text,&used);
        if(used!=text.size() || !std::isfinite(value) || std::abs(value)>1000000 || !values.emplace(key,value).second)
            throw std::runtime_error("Invalid or duplicate Quest profile value.");
    }
    auto get=[&](const char *key) { return values.at(key); };
    if(get("schema")!=1 || get("quest3")!=1 || get("width")<256 || get("width")>8192 ||
       get("height")<256 || get("height")>8192 || get("hz")<60 || get("hz")>144 ||
       get("scale")<50 || get("scale")>400 || get("max_anisotropy")<1 || get("max_anisotropy")>16)
        throw std::runtime_error("Unsupported Quest profile or render extent.");
    for(const char *flag : {"anisotropy","atomics","bc","astc","etc2"})
        if(get(flag)!=0 && get(flag)!=1) throw std::runtime_error("Invalid Quest feature flag.");
    for(int eye=0;eye<2;++eye) {
        auto eyeValue=[&](const char *part) { return values.at("eye"+std::to_string(eye)+"_"+part); };
        double norm=0;
        for(const char *part : {"qx","qy","qz","qw"}) norm+=eyeValue(part)*eyeValue(part);
        if(std::abs(norm-1)>.001 || std::abs(eyeValue("px"))>.1 || std::abs(eyeValue("py"))>.1 || std::abs(eyeValue("pz"))>.1 ||
           eyeValue("left")>=0 || eyeValue("left")< -1.55 || eyeValue("right")<=0 || eyeValue("right")>1.55 ||
           eyeValue("down")>=0 || eyeValue("down")< -1.55 || eyeValue("up")<=0 || eyeValue("up")>1.55)
            throw std::runtime_error("Invalid Quest stereo geometry.");
    }
    if(!values.count("setting_MsaaSamples") || !values.count("setting_RenderScalePercent"))
        throw std::runtime_error("Quest profile lacks rendering settings.");
    if(get("width")!=std::floor(get("width")) || get("height")!=std::floor(get("height")) ||
       (get("setting_MsaaSamples")!=1 && get("setting_MsaaSamples")!=2 && get("setting_MsaaSamples")!=4))
        throw std::runtime_error("Invalid Quest extent or MSAA count.");
    for(const auto &entry:values)
        if(entry.first.rfind("setting_",0)==0 && entry.second!=std::floor(entry.second))
            throw std::runtime_error("Noninteger Quest setting.");
    return values;
}
inline void Load(const std::string &path) {
    std::ifstream in(path); if(!in) throw std::runtime_error("Cannot open Quest profile.");
    requested=Parse(in); active=true; dirty=false;
}
inline int Read(const char *section,const char *key,int value) {
    if(!section || !key || std::string(section)!="VR" || !IsSetting(key)) return value;
    const auto name="setting_"+std::string(key);
    if(active) {
        const auto found=requested.find(name);
        if(found!=requested.end()) value=int(found->second);
        else dirty=true; // A newer setting cannot silently qualify as matched.
    }
    captured.emplace(name,value); return value;
}
inline bool Write(const char *section,const char *key,const char *value) {
    if(!section || !key || std::string(section)!="VR" || !IsSetting(key)) return true;
    if(active) {
        const auto found=requested.find("setting_"+std::string(key));
        if(!value || found==requested.end() || value!=std::to_string(int(found->second))) dirty=true;
        return false; // Profile runs never overwrite saved graphics preferences.
    }
    if(value) { try { captured["setting_"+std::string(key)]=std::stoi(value); } catch(...) {} }
    return true;
}
inline uint64_t Hash(const std::string &text) {
    uint64_t hash=14695981039346656037ULL;
    for(unsigned char c:text) hash=(hash^c)*1099511628211ULL;
    return hash;
}
inline std::string Identity(const Values &values) {
    std::ostringstream out; out<<std::hex<<Hash(Text(values)); return out.str();
}
template<class View> inline void StoreEyes(const View views[2]) {
    for(int eye=0;eye<2;++eye) {
        const auto &p=views[eye].pose; const auto &f=views[eye].fov;
        const double values[]={p.position.x,p.position.y,p.position.z,p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w,
                               f.angleLeft,f.angleRight,f.angleUp,f.angleDown};
        const char *parts[]={"px","py","pz","qx","qy","qz","qw","left","right","up","down"};
        for(int i=0;i<11;++i) captured["eye"+std::to_string(eye)+"_"+parts[i]]=values[i];
    }
}
template<class View> inline void ApplyEyes(View views[2]) {
    if(!active) return;
    for(int eye=0;eye<2;++eye) {
        auto get=[&](const char *part){return float(requested.at("eye"+std::to_string(eye)+"_"+part));};
        views[eye].pose.position={get("px"),get("py"),get("pz")};
        views[eye].pose.orientation={get("qx"),get("qy"),get("qz"),get("qw")};
        views[eye].fov={get("left"),get("right"),get("up"),get("down")};
    }
}
inline bool Save(const std::string &path) {
    try {
        std::istringstream check(Text(captured)); Parse(check);
        std::ofstream out(path); out<<Text(captured); return bool(out);
    } catch(...) { return false; }
}
} // namespace MatchedProfile
