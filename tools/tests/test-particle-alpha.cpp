#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

using uint32 = uint32_t;
enum { DRAW_OPAQUE=4, DRAW_DARK=128 };
enum { rwRENDERSTATESRCBLEND, rwRENDERSTATEDESTBLEND };
enum { rwBLENDONE=1, rwBLENDSRCALPHA, rwBLENDINVSRCALPHA };
static int src, dst, flushes;
static bool xbox;
namespace ModelSets { bool UseXboxParticles() { return xbox; } }
namespace rw {
enum { ALPHATESTFUNC, ALPHAALWAYS=1, ALPHAGREATEREQUAL=7 };
static uint32 alphaTest;
uint32 GetRenderState(int) { return alphaTest; }
void SetRenderState(int, uint32 value) { alphaTest=value; }
}
namespace CSprite { void FlushSpriteBuffer() { ++flushes; } }
void RwRenderStateSet(int state, void* value) {
    (state==rwRENDERSTATESRCBLEND ? src : dst)=int(reinterpret_cast<intptr_t>(value));
}

static void render() {
#include "particle-alpha-begin.inc"
    assert(rw::alphaTest==uint32(xbox ? rw::ALPHAALWAYS : rw::ALPHAGREATEREQUAL));
    uint32 flags=DRAW_OPAQUE;
    src=rwBLENDSRCALPHA; dst=rwBLENDINVSRCALPHA;
    // Exercise transitions in both directions, including a repeated blend mode.
    for(uint32 next : {0u, 0u, 4u, 128u, 0u, 4u, 132u, 0u}) {
        struct { uint32 Flags; } system{next};
        auto psystem=&system;
        const int previousFlushes=flushes;
        const bool shouldFlush=((flags^next)&(DRAW_OPAQUE|DRAW_DARK))!=0;
#include "particle-alpha-blend.inc"
        assert(flushes==previousFlushes+(shouldFlush ? 1 : 0));
        assert(src==((xbox || (next&(DRAW_OPAQUE|DRAW_DARK))) ? rwBLENDSRCALPHA : rwBLENDONE));
        assert(dst==((next&DRAW_OPAQUE) ? rwBLENDINVSRCALPHA : rwBLENDONE));
        if(xbox && !(next&DRAW_OPAQUE)) {
            // Bright RGB in an invisible texel must leave the background intact.
            // Texture and vertex/lifetime alpha both attenuate additive energy.
            for(float textureAlpha : {0.0f, 0.1f, 0.5f, 1.0f})
            for(float vertexAlpha : {0.0f, 0.25f, 1.0f}) {
                float alpha=textureAlpha*vertexAlpha;
                float result=0.2f+0.6f*(src==rwBLENDSRCALPHA ? alpha : 1.0f);
                assert(std::abs(result-(0.2f+0.6f*alpha))<0.00001f);
            }
        }
    }
#include "particle-alpha-end.inc"
    assert(rw::alphaTest==rw::ALPHAGREATEREQUAL);
}

int main() {
    for(bool useXbox : {false,true}) {
        xbox=useXbox; rw::alphaTest=rw::ALPHAGREATEREQUAL; flushes=0;
        render();
    }
    std::cout << "Particle alpha blending, fades, transitions and state restoration passed\n";
}
