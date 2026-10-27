#pragma once
#include "VehicleCaptureCore.h"
#include <algorithm>

// A simulation tape contains controls and a reference route, never game assets.
// It intentionally does not claim to restore the ambient world or RNG state.
namespace SimulationReplayCore {
constexpr unsigned MaxSamples = VehicleCaptureCore::MaxSamples;
constexpr uint64_t MaxTimeUs = VehicleCaptureCore::MaxTimeUs;
struct Initial {
    VehicleCaptureCore::Initial vehicle;
    int assets[5] = {};
    int gear = 0;
    float gearTime = 0;
};
struct Sample {
    VehicleCaptureCore::Input input;
    float position[3] = {}, forward[3] = {}, speed = 0;
};
inline bool Valid(const Sample &s) {
    if(!VehicleCaptureCore::Valid(s.input) || !std::isfinite(s.speed) || s.speed < 0 || s.speed > 1800) return false;
    float norm = 0;
    for(int i=0;i<3;++i) {
        if(!std::isfinite(s.position[i]) || std::fabs(s.position[i]) > 10000 || !std::isfinite(s.forward[i])) return false;
        norm += s.forward[i]*s.forward[i];
    }
    return std::fabs(norm-1) < 0.02f;
}
inline bool Valid(const Initial &s) {
    if(!VehicleCaptureCore::Valid(s.vehicle) || s.gear < 0 || s.gear > 5 || !std::isfinite(s.gearTime) || std::fabs(s.gearTime)>100000) return false;
    for(int a:s.assets) if(a<0 || a>2) return false;
    return true;
}
inline bool Header(FILE *f,const Initial &s) {
    if(!Valid(s)) return false;
    std::fprintf(f,"LCVR_SIM_V1,%d,%u",s.vehicle.model,s.vehicle.signature);
    for(float v:s.vehicle.pose) std::fprintf(f,",%.9g",double(v));
    for(int a:s.assets) std::fprintf(f,",%d",a);
    return std::fprintf(f,",%d,%.9g\n",s.gear,double(s.gearTime))>0 && !std::ferror(f);
}
inline bool Write(FILE *f,const Sample &s) {
    if(!Valid(s)) return false;
    const auto &i=s.input;
    std::fprintf(f,"%llu,%.9g,%.9g,%.9g,%d",(unsigned long long)i.us,double(i.gas),double(i.brake),double(i.steer),i.handbrake);
    for(float v:s.position) std::fprintf(f,",%.9g",double(v));
    for(float v:s.forward) std::fprintf(f,",%.9g",double(v));
    return std::fprintf(f,",%.9g\n",double(s.speed))>0 && !std::ferror(f);
}
class Tape {
public:
    Initial initial;
    std::vector<Sample> samples;
    uint64_t endUs=0;
    uint64_t lastReadUs=0;
    size_t cursor=0;
    bool read=false;
    bool Load(FILE *f) {
        samples.clear(); endUs=0; cursor=0; lastReadUs=0; read=false;
        char line[1024],tail; int used=0;
        if(!f || !std::fgets(line,sizeof(line),f) || std::sscanf(line,"LCVR_SIM_V1,%d,%u%n",&initial.vehicle.model,&initial.vehicle.signature,&used)!=2) return false;
        const char *p=line+used;
        for(float &v:initial.vehicle.pose) { if(std::sscanf(p,",%f%n",&v,&used)!=1) return false; p+=used; }
        for(int &a:initial.assets) { if(std::sscanf(p,",%d%n",&a,&used)!=1) return false; p+=used; }
        if(std::sscanf(p,",%d,%f %c",&initial.gear,&initial.gearTime,&tail)!=2 || !Valid(initial)) return false;
        while(std::fgets(line,sizeof(line),f)) {
            unsigned long long us=0;
            if(std::strncmp(line,"END,",4)==0) {
                if(std::sscanf(line,"END,%llu %c",&us,&tail)!=1) return false;
                endUs=uint64_t(us);
                return !samples.empty() && endUs>samples.back().input.us && endUs<=MaxTimeUs && std::fgetc(f)==EOF && !std::ferror(f);
            }
            Sample s;
            if(std::sscanf(line,"%llu,%f,%f,%f,%d%n",&us,&s.input.gas,&s.input.brake,&s.input.steer,&s.input.handbrake,&used)!=5) return false;
            s.input.us=uint64_t(us); p=line+used;
            for(float &v:s.position) { if(std::sscanf(p,",%f%n",&v,&used)!=1) return false; p+=used; }
            for(float &v:s.forward) { if(std::sscanf(p,",%f%n",&v,&used)!=1) return false; p+=used; }
            if(std::sscanf(p,",%f %c",&s.speed,&tail)!=1 || !Valid(s) || samples.size()>=MaxSamples ||
                (samples.empty()?us!=0:us<=samples.back().input.us)) return false;
            samples.push_back(s);
        }
        return false; // A crash or unfinished write is never a valid demo.
    }
    bool At(uint64_t us,Sample &out) {
        if(samples.empty() || us>=endUs || (read && us<lastReadUs)) return false;
        read=true;lastReadUs=us;
        while(cursor+1<samples.size() && samples[cursor+1].input.us<=us) ++cursor;
        out=samples[cursor]; // Hold controls; interpolate only the comparison route.
        if(cursor+1<samples.size()) {
            const Sample &next=samples[cursor+1];
            const float t=float(us-out.input.us)/float(next.input.us-out.input.us);
            for(int i=0;i<3;++i) { out.position[i]+=(next.position[i]-out.position[i])*t; out.forward[i]+=(next.forward[i]-out.forward[i])*t; }
            out.speed+=(next.speed-out.speed)*t;
        }
        return true;
    }
};
// The same symmetric policy is used by sector broad phase and entity narrow phase.
inline bool IgnorePair(bool enabled,bool protectedA,bool protectedB,bool dynamicA,bool dynamicB) {
    return enabled && ((protectedA && dynamicB) || (protectedB && dynamicA));
}
struct Drift {
    double squared=0;
    unsigned count=0;
    float maxPosition=0,maxSpeed=0,maxHeading=0;
    bool Add(const Sample &reference,const float *position,const float *forward,float speed) {
        float d=0,dot=0,norm=0;
        for(int i=0;i<3;++i) { const float v=position[i]-reference.position[i]; d+=v*v; dot+=forward[i]*reference.forward[i]; norm+=reference.forward[i]*reference.forward[i]; }
        if(!std::isfinite(d) || !std::isfinite(dot) || !std::isfinite(speed)) return false;
        const float meters=std::sqrt(d);
        const float cosine=norm>0.0001f?dot/std::sqrt(norm):1.0f;
        maxPosition=std::max(maxPosition,meters); maxSpeed=std::max(maxSpeed,std::fabs(speed-reference.speed));
        maxHeading=std::max(maxHeading,std::acos(std::max(-1.0f,std::min(1.0f,cosine)))*57.2957795f);
        squared+=d; ++count;
        return meters<=50.0f;
    }
    double Rms() const { return count?std::sqrt(squared/count):0; }
};
}
