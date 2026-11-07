#include "ParticleFrame.h"
#include <cassert>
#include <climits>
#include <iostream>

int main()
{
    using ParticleFrame::Resolve;
    // Console fireball/flames address ranges in a single-raster effect.
    for(unsigned frame=0; frame<256; ++frame){
        assert(Resolve(frame,30,43,1,true)==0);
        assert(Resolve(frame,0,29,1,true)==0);
        assert(Resolve(frame,32,43,1,true)==0);
    }
    // Longer console explosion sequences span all six available textures.
    assert(Resolve(0,0,14,6,true)==0);
    assert(Resolve(7,0,14,6,true)==2);
    assert(Resolve(14,0,14,6,true)==5);
    assert(Resolve(30,30,43,6,true)==0);
    assert(Resolve(43,30,43,6,true)==5);
    for(unsigned count=1;count<=6;++count){
        for(unsigned start=0;start<45;++start){
            for(unsigned end=0;end<45;++end){
                unsigned previous=0;
                for(unsigned frame=0;frame<60;++frame){
                    auto resolved=Resolve(frame,start,end,count,true);
                    assert(resolved<count);
                    assert(resolved>=previous);
                    previous=resolved;
                    assert(Resolve(frame,start,end,count,false)<count);
                }
            }
        }
        assert(Resolve(UINT_MAX-1,0,UINT_MAX,count,true)<count);
        assert(Resolve(UINT_MAX,0,UINT_MAX,count,true)==count-1);
    }
    // Native Classic and compatible Xbox sequences retain their frame indices.
    for(unsigned frame=0;frame<6;++frame){
        assert(Resolve(frame,0,5,6,false)==frame);
        assert(Resolve(frame,0,5,6,true)==frame);
    }
    assert(Resolve(255,0,0,0,true)==0);
    assert(Resolve(255,0,0,4,false)==3);
    std::cout << "Particle frame regression checks passed\n";
}
