#pragma once
#include <cstdint>

namespace AtomicSubmission {
struct Stats {
    uint32_t atomics=0,pipelineRequests=0,pipelineBinds=0;
    uint32_t textureRequests=0,textureBinds=0,textureLookups=0;
    void Add(const Stats &s){
        atomics+=s.atomics;pipelineRequests+=s.pipelineRequests;pipelineBinds+=s.pipelineBinds;
        textureRequests+=s.textureRequests;textureBinds+=s.textureBinds;textureLookups+=s.textureLookups;
    }
};

// One instance per synchronous atomic submission. Never retain this state over
// another renderer, command buffer, subpass, frame or object boundary. Compare
// resolved handles (including the layout), not raster/material identities.
template<class Pipeline,class Descriptor,class Layout> class Cache {
    bool enabled,measure,pipelineKnown=false,textureKnown=false;
    Pipeline pipeline{};
    Descriptor texture{};
    Layout layout{};
public:
    Stats stats;
    Cache(bool enable,bool profile):enabled(enable),measure(profile){if(measure)stats.atomics=1;}
    bool PipelineBind(Pipeline next){
        const bool emit=!enabled||!pipelineKnown||pipeline!=next;
        if(measure){++stats.pipelineRequests;if(emit)++stats.pipelineBinds;}
        if(enabled&&emit){pipeline=next;pipelineKnown=true;}
        return emit;
    }
    bool TextureBind(Layout nextLayout,Descriptor next){
        const bool emit=!enabled||!textureKnown||texture!=next||layout!=nextLayout;
        if(measure){++stats.textureRequests;if(emit)++stats.textureBinds;}
        if(enabled&&emit){texture=next;layout=nextLayout;textureKnown=true;}
        return emit;
    }
    void TextureLookup(){if(measure)++stats.textureLookups;}
};
}
