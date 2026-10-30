#include "vulkan/OpaqueOrder.h"
#include <cassert>
#include <limits>
#include <random>
#include <vector>
#include <algorithm>

struct Item { int id; OpaqueOrder::Interval depth; bool eligible=true; };
static Item item(int id,float lo,float hi) { return {id,{{lo,lo},{hi,hi}},true}; }
int main()
{
    Item separated[]={item(0,.8f,.9f),item(1,.4f,.5f),item(2,.1f,.2f)};
    assert(OpaqueOrder::Sort(separated,3)==3);
    assert(separated[0].id==2 && separated[2].id==0);
    Item barrier[]={item(0,.8f,.9f),item(1,.4f,.5f),item(2,.1f,.2f)};
    barrier[1].eligible=false;
    assert(OpaqueOrder::Sort(barrier,3)==0);
    Item overlap[]={item(0,.2f,.8f),item(1,.3f,.4f)};
    assert(OpaqueOrder::Sort(overlap,2)==0);
    auto a=item(0,.1f,.2f),b=item(1,.3f,.4f);
    assert(OpaqueOrder::Before(a.depth,b.depth));
    a.depth.farDepth[1]=.35f; // One eye alone is insufficient.
    assert(!OpaqueOrder::Before(a.depth,b.depth));
    a=item(0,.1f,.29995f);
    assert(!OpaqueOrder::Before(a.depth,b.depth));
    a.depth.nearDepth[0]=std::numeric_limits<float>::quiet_NaN();
    assert(!OpaqueOrder::Before(a.depth,b.depth));
    a=item(0,-.1f,.2f);assert(!OpaqueOrder::Before(a.depth,b.depth));
    a=item(0,.3f,.2f);assert(!OpaqueOrder::Before(a.depth,b.depth));
    b.depth.farDepth[1]=std::numeric_limits<float>::infinity();
    assert(!OpaqueOrder::Before(item(0,.1f,.2f).depth,b.depth));
    std::mt19937 rng(123);
    for(int trial=0;trial<500;++trial){
        std::vector<Item> original;
        for(int i=0;i<32;++i){
            float lo=float(rng()%80)/100.f;
            original.push_back(item(i,lo,lo+float(rng()%20)/100.f));
            original.back().eligible=rng()%7!=0;
        }
        auto sorted=original;
        OpaqueOrder::Sort(sorted.data(),unsigned(sorted.size()));
        int positions[32];bool seen[32]{};
        for(int i=0;i<32;++i){assert(!seen[sorted[i].id]);seen[sorted[i].id]=true;positions[sorted[i].id]=i;}
        for(int i=0;i<32;++i)for(int j=i+1;j<32;++j){
            // Every pair lacking proof to reverse must preserve its order.
            if(!original[i].eligible || !original[j].eligible ||
               !OpaqueOrder::Before(original[j].depth,original[i].depth))
                assert(positions[i]<positions[j]);
        }
    }
}
