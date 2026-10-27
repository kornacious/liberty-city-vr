#pragma once
#include <cstdint>
#include <cmath>

// Portable bookkeeping/decoding shared by the Vulkan backend and host tests.
// Two query slots per marker are reserved even outside a stereo render pass:
// multiview vkCmdWriteTimestamp consumes one query for each active view.
namespace GpuPassProfile {
enum Pass { Sky,Roads,Reflections,World,Water,Fading,Weather,Effects,Ui,Scope,Wrist,Postprocess,Other,Count };
constexpr unsigned SampleInterval=8,MaxMarkers=64,QueryStride=2,QueryCount=2+MaxMarkers*QueryStride,StackSize=8;
inline bool ShouldSample(uint64_t frameId){
    if(!frameId)return false;
    const uint64_t index=frameId-1;
    return index%SampleInterval==(index/SampleInterval)%SampleInterval;
}
enum SampleStatus { NoSample,Complete,Overflow,Unbalanced,Unavailable };
inline const char *Name(unsigned pass){
    static const char *names[]={"sky","roads","reflections","world","water","fading","weather","effects","ui","scope","wrist","resolve_postprocess","other"};
    return pass<Count?names[pass]:"invalid";
}
struct Result {
    uint64_t frameId=0,startNs=0;
    uint32_t mask=0,status=NoSample;
    float frameMs=-1,ms[Count];
    uint32_t draws[Count]={};
    Result(){for(float &v:ms)v=-1;}
};
inline double Milliseconds(uint64_t begin,uint64_t end,unsigned bits,double period){
    if(bits==0||bits>64||!std::isfinite(period)||period<=0) return -1;
    uint64_t delta=end-begin;
    if(bits<64) delta&=(uint64_t(1)<<bits)-1;
    return double(delta)*period/1000000.0;
}
struct Recording {
    struct Segment {unsigned pass=Other,begin=0,end=0;};
    Segment segments[MaxMarkers+1];
    unsigned stack[StackSize]={},depth=0,active=Other,markers=0,lastQuery=0;
    uint32_t seen=uint32_t(1)<<Other,status=NoSample,draws[Count]={};
    bool sampled=false;
    int Transition(unsigned pass){
        if(active==pass)return -1;
        if(markers>=MaxMarkers){status=Overflow;return -1;}
        const unsigned query=2+markers*QueryStride;
        segments[markers++]={active,lastQuery,query};lastQuery=query;active=pass;
        return int(query);
    }
    int Begin(unsigned pass){
        if(!sampled||status!=NoSample)return -1;
        if(pass>=Count||depth>=StackSize){status=Overflow;return -1;}
        stack[depth++]=active;seen|=uint32_t(1)<<pass;
        return Transition(pass);
    }
    int End(unsigned pass){
        if(!sampled||status!=NoSample)return -1;
        if(!depth||pass!=active){status=Unbalanced;return -1;}
        return Transition(stack[--depth]);
    }
    void Draw(){if(sampled&&status==NoSample)++draws[active];}
    Result Decode(const uint64_t (*queries)[2],unsigned bits,double period,uint64_t frameId,uint64_t startNs) const {
        Result out;out.frameId=frameId;out.startNs=startNs;
        if(!sampled)return out;
        out.status=status!=NoSample?status:depth?Unbalanced:Complete;
        for(unsigned i=0;i<Count;++i)out.draws[i]=draws[i];
        if(out.status!=Complete)return out;
        if(!queries[0][1]||!queries[1][1]){out.status=Unavailable;return out;}
        const double total=Milliseconds(queries[0][0],queries[1][0],bits,period);
        if(total<0||!std::isfinite(total)){out.status=Unavailable;return out;}
        float times[Count]={};
        for(unsigned i=0;i<=markers;++i){
            const Segment s=i<markers?segments[i]:Segment{active,lastQuery,1};
            if(!queries[s.begin][1]||!queries[s.end][1]){out.status=Unavailable;return out;}
            const double time=Milliseconds(queries[s.begin][0],queries[s.end][0],bits,period);
            if(time<0||time>total+0.01){out.status=Unavailable;return out;}
            times[s.pass]+=float(time);
        }
        out.frameMs=float(total);out.mask=seen;
        for(unsigned i=0;i<Count;++i)if(seen&(uint32_t(1)<<i))out.ms[i]=times[i];
        return out;
    }
};
}
