#pragma once
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

// Portable input tape. No engine pointers, game assets, or raw struct dumps.
namespace VehicleCaptureCore {
constexpr unsigned MaxSamples=20000;
constexpr uint64_t MaxTimeUs=120000000;
struct Input {uint64_t us=0;float gas=0,brake=0,steer=0;int handbrake=0;};
struct Initial {int model=0;unsigned signature=0;float pose[18]={};};
inline bool Valid(const Input &s){
    return s.us<=MaxTimeUs&&std::isfinite(s.gas)&&std::isfinite(s.brake)&&std::isfinite(s.steer)&&
        s.gas>=-1&&s.gas<=1&&s.brake>=0&&s.brake<=1&&std::fabs(s.steer)<=2&&
        (s.handbrake==0||s.handbrake==1);
}
inline bool Valid(const Initial &s){
    if(s.model<0||s.model>65535)return false;
    for(float v:s.pose)if(!std::isfinite(v)||std::fabs(v)>100000)return false;
    for(int i=0;i<3;++i)if(std::fabs(s.pose[i])>10000)return false;
    // Position, right/forward/up axes, move/turn velocity. Reject bad transforms.
    for(int a=3;a<12;a+=3){float n=0;for(int i=0;i<3;++i)n+=s.pose[a+i]*s.pose[a+i];if(std::fabs(n-1)>0.01f)return false;}
    for(int a=3;a<9;a+=3)for(int b=a+3;b<12;b+=3){float d=0;for(int i=0;i<3;++i)d+=s.pose[a+i]*s.pose[b+i];if(std::fabs(d)>0.01f)return false;}
    const float *r=s.pose+3,*f=s.pose+6,*u=s.pose+9;
    if((r[1]*f[2]-r[2]*f[1])*u[0]+(r[2]*f[0]-r[0]*f[2])*u[1]+(r[0]*f[1]-r[1]*f[0])*u[2]<0.99f)return false;
    for(int i=12;i<18;++i)if(std::fabs(s.pose[i])>10)return false;
    return true;
}
inline bool WriteHeader(FILE *f,const Initial &s){
    if(!Valid(s)||std::fprintf(f,"LCVR_INPUT_V1,%d,%u",s.model,s.signature)<0)return false;
    for(float v:s.pose)if(std::fprintf(f,",%.9g",double(v))<0)return false;
    return std::fputc('\n',f)!=EOF;
}
inline bool Write(FILE *f,const Input &s){
    return Valid(s)&&std::fprintf(f,"%llu,%.9g,%.9g,%.9g,%d\n",(unsigned long long)s.us,double(s.gas),double(s.brake),double(s.steer),s.handbrake)>0;
}
class Tape {
public:
    Initial initial;std::vector<Input> samples;uint64_t endUs=0;size_t cursor=0;
    bool Load(FILE *f){
        samples.clear();cursor=0;endUs=0;char line[512],tail;int used=0;
        if(!std::fgets(line,sizeof(line),f)||std::sscanf(line,"LCVR_INPUT_V1,%d,%u%n",&initial.model,&initial.signature,&used)!=2)return false;
        const char *p=line+used;
        for(float &v:initial.pose){if(std::sscanf(p,",%f%n",&v,&used)!=1)return false;p+=used;}
        if((std::strcmp(p,"\n")!=0&&std::strcmp(p,"\r\n")!=0)||!Valid(initial))return false;
        while(std::fgets(line,sizeof(line),f)){
            unsigned long long end=0;
            if(std::sscanf(line,"END,%llu %c",&end,&tail)==1){
                endUs=uint64_t(end);
                return !samples.empty()&&endUs>samples.back().us&&endUs<=MaxTimeUs&&std::fgetc(f)==EOF&&!std::ferror(f);
            }
            Input s;unsigned long long time=0;
            if(std::sscanf(line,"%llu,%f,%f,%f,%d %c",&time,&s.gas,&s.brake,&s.steer,&s.handbrake,&tail)!=5)return false;
            s.us=uint64_t(time);
            if(!Valid(s)||samples.size()>=MaxSamples||(!samples.empty()&&s.us<=samples.back().us)|| (samples.empty()&&s.us!=0))return false;
            samples.push_back(s);
        }
        return false; // Truncated/unclosed captures cannot be replayed.
    }
    bool At(uint64_t us,Input &out){
        if(samples.empty()||us>=endUs)return false;
        while(cursor+1<samples.size()&&samples[cursor+1].us<=us)cursor++;
        // The runtime clock is monotonic; reject accidental backwards seeking.
        if(samples[cursor].us>us)return false;
        out=samples[cursor];return true;
    }
};
inline bool Label(const char *s){
    unsigned n=0;for(;s[n];++n)if(n>=32||!((s[n]>='a'&&s[n]<='z')||(s[n]>='A'&&s[n]<='Z')||(s[n]>='0'&&s[n]<='9')||s[n]=='_'||s[n]=='-'))return false;
    return n>0;
}
}
