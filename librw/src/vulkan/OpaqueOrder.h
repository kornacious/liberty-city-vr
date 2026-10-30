#pragma once
#include <cmath>
#include <cstdint>

namespace OpaqueOrder {
struct Interval { float nearDepth[2], farDepth[2]; };
struct Stats { uint32_t candidates=0,eligible=0,swaps=0; };
inline bool Before(const Interval &a,const Interval &b)
{
    for(int eye=0;eye<2;++eye){
        if(!std::isfinite(a.nearDepth[eye])||!std::isfinite(a.farDepth[eye])||
           !std::isfinite(b.nearDepth[eye])||!std::isfinite(b.farDepth[eye])||
           a.nearDepth[eye]<0||a.farDepth[eye]>1||a.nearDepth[eye]>a.farDepth[eye]||
           b.nearDepth[eye]<0||b.farDepth[eye]>1||b.nearDepth[eye]>b.farDepth[eye]||
           !(a.farDepth[eye]+0.0001f < b.nearDepth[eye]))return false;
    }
    return true;
}

// This is a partial order, not a strict-weak comparator for std::sort.
// Only swap adjacent, provably separated items. Every overlapping/ambiguous
// pair retains its relative order; transparent entries are explicit barriers.
template<class Item> uint32_t Sort(Item *items,unsigned count)
{
    uint32_t swaps=0;
    for(unsigned i=1;i<count;++i){
        unsigned j=i;
        while(j>0 && items[j].eligible && items[j-1].eligible &&
              Before(items[j].depth,items[j-1].depth)){
            Item tmp=items[j];items[j]=items[j-1];items[j-1]=tmp;
            --j;++swaps;
        }
    }
    return swaps;
}
}
