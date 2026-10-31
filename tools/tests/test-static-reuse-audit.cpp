#include "../../librw/src/vulkan/StaticReuseAudit.h"
#include <cassert>
#include <limits>
int main(){
    StaticReuseAudit::Recording<16,4> r;
    StaticReuseAudit::Key k;k.geometry=11;k.pipeline=12;k.texture=13;
    k.first=0;k.count=12;k.model=1416;
    const float near[3]={0,0,0},edge[3]={32,0,0},far[3]={33,0,0};
    r.Reset(true,7);
    r.Observe(k,1,near);r.FinishDraw();
    r.Observe(k,2,edge);r.FinishDraw();
    assert(r.stats.sourceFrame==7 && r.stats.sampled==1);
    assert(r.stats.candidates==2 && r.stats.groups==1 && r.stats.compatibleInstances==1);
    assert(r.stats.nearbyInstances==1 && r.stats.adjacentInstances==1);
    r.Observe(k,2,edge);r.FinishDraw(); // same object is not another instance
    assert(r.stats.compatibleInstances==1);
    r.FinishDraw(); // unsupported draw intervenes
    r.Observe(k,3,far);r.FinishDraw();
    assert(r.stats.compatibleInstances==2 && r.stats.nearbyInstances==1);
    assert(r.stats.adjacentInstances==1 && r.stats.maxInstances==3 && r.stats.topModel==1416);
    r.Boundary();r.Observe(k,4,near);r.FinishDraw();
    assert(r.stats.groups==2 && r.stats.compatibleInstances==2);
    // Lighting, descriptor, pipeline, range and model boundaries stay distinct.
    auto other=k;other.state[8]=0x3f800000;r.Observe(other,5,near);r.FinishDraw();
    other=k;other.texture=55;r.Observe(other,6,near);r.FinishDraw();
    other=k;other.pipeline=66;r.Observe(other,7,near);r.FinishDraw();
    other=k;other.first=12;r.Observe(other,8,near);r.FinishDraw();
    other=k;other.model=1417;r.Observe(other,9,near);r.FinishDraw();
    assert(r.stats.groups==7 && r.stats.compatibleInstances==2);
    assert(r.stats.geometryRepeats>0);
    r.Reset(true,8);r.Observe(k,2,near);r.FinishDraw();
    assert(r.stats.groups==1 && r.stats.compatibleInstances==0); // no stale cross-frame groups
    float invalid[3]={std::numeric_limits<float>::infinity(),0,0};
    r.Observe(k,3,invalid);r.FinishDraw();assert(r.stats.rejectedTransform==1);
    r.Observe(k,0,near);r.FinishDraw();assert(r.stats.rejectedOwner==1);
    r.Reset(false,9);r.Observe(k,2,near);
    assert(r.stats.sampled==0 && r.stats.sourceFrame==0 && r.stats.candidates==0);
    StaticReuseAudit::Recording<4,2> bounded;bounded.Reset(true,10);
    for(uint64_t owner=1;owner<=3;++owner){bounded.Observe(k,owner,near);bounded.FinishDraw();}
    assert(bounded.stats.compatibleInstances==1 && bounded.stats.ownerOverflow==1);
    for(unsigned i=0;i<20;++i){other=k;other.geometry=i+50;bounded.Observe(other,i+4,near);bounded.FinishDraw();}
    assert(bounded.stats.tableOverflow>0);
    assert(bounded.stats.groups==4); // bounded memory, no unreported eviction
}
