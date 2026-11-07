#include "vulkan/NeoRim.h"
#define RW_NEO_INLINE inline
#include "vulkan/shaders/rw_neo_rim.glsl"
#include <cassert>
#include <cmath>
#include <cstdio>

static void near(float a,float b) { assert(std::fabs(a-b)<0.00001f); }
int main()
{
    NeoRim::Selection selection;
    assert(selection.Eligible(true,true,true)); // skinned body
    assert(!selection.Eligible(false,true,true)); // unrelated rigid prop
    const bool previous=selection.SetPedestrian(true);
    assert(!previous && selection.Eligible(false,true,true)); // rigid head/hands
    assert(selection.Eligible(true,true,true));
    assert(!selection.Eligible(false,false,true)); // authored unlit part
    assert(!selection.Eligible(false,true,false)); // no normal, no rim
    const bool body=selection.SetPedestrian(false); // held weapon submission
    assert(body && !selection.Eligible(false,true,true));
    const bool nested=selection.SetPedestrian(true); // another ped/limb
    assert(!nested && selection.Eligible(false,true,true));
    selection.SetPedestrian(nested);assert(!selection.Eligible(false,true,true));
    selection.SetPedestrian(body);assert(selection.Eligible(false,true,true));
    selection.SetPedestrian(previous);assert(!selection.Eligible(false,true,true));

    float direction[4];
    const float identity[4]={0,0,0,1};NeoRim::CameraForward(identity,direction);
    near(direction[0],0);near(direction[1],0);near(direction[2],-1);
    // The actual shader scalar is compiled here, not a rewritten test formula.
    // Neo's dark-to-bright ramp should not flood a front-facing torso white.
    const float front=RwNeoRimChannel(1,0,0.5f,1.5f,2,direction[2]);
    near(front,0);near(RwNeoRimChannel(1,0,0.5f,1.5f,2,0),1);
    assert(RwNeoRimChannel(1,0,0.5f,1.5f,2,-direction[2])>front);
    const float rotated[4]={0,0.70710678f,0,0.70710678f};
    NeoRim::CameraForward(rotated,direction);near(direction[0],-1);near(direction[2],0);
    near(RwNeoRimChannel(1,0,0.5f,1.5f,2,direction[0]),front);
    near(RwNeoRimChannel(1,0,0.5f,1.5f,0,0),0); // disabled strength
    for(int i=-100;i<=100;i++) {
        const float facing=i/100.0f;
        const float head=RwNeoRimChannel(0.4f,0.1f,0.5f,1.5f,2,facing);
        assert(std::isfinite(head) && head>=0 && head<=1);
    }
    std::puts("PASS Neo rim: rigid limbs, nested entity/weapon context, shared shader ramp and camera-forward convention");
}
