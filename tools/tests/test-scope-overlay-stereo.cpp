#include "../../librw/src/vulkan/ScopeOverlayMath.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>

static bool close(float a, float b) { return std::fabs(a-b) < 0.0001f; }

int main()
{
    int cases = 0;
    for(float yaw : {0.0f, 0.8f, 3.0f})
    for(float distance : {1.0f, 10.0f, 100.0f, 300.0f})
    for(float x : {400.0f, 1080.0f, 1700.0f}) {
        const float right[3] = {std::cos(yaw), 0, -std::sin(yaw)};
        const float forward[3] = {std::sin(yaw), 0, std::cos(yaw)};
        const float eye[3] = {2, 1.7f, -3};
        float plane[16] = {}, target[3], out[16];
        plane[15] = 1;
        plane[5] = -0.001f;
        for(int i=0; i<3; ++i){
            plane[i] = right[i]*0.001f;
            plane[12+i] = eye[i]+forward[i]*2.0f-right[i]*1.08f;
            target[i] = eye[i]+forward[i]*distance+right[i]*0.2f;
        }
        assert(ScopeOverlayMath::targetPlane(out, plane, eye, target, x, 970));
        float centre[3];
        for(int i=0; i<3; ++i){
            centre[i] = out[12+i]+out[i]*x+out[4+i]*970;
            assert(close(centre[i], target[i]));
            assert(close(out[i]/distance, plane[i]/2.0f));
        }
        // Project the rendered centre and bullet target independently for
        // each eye, asymmetric frustums and sniper magnification included.
        for(float offset : {-0.032f, 0.032f})
        for(float zoom : {1.0f, 4.0f, 10.0f}) {
            float reticleX=0, hitX=0, reticleZ=0, hitZ=0;
            for(int i=0; i<3; ++i){
                const float e = eye[i]+right[i]*offset;
                reticleX += (centre[i]-e)*right[i];
                hitX += (target[i]-e)*right[i];
                reticleZ += (centre[i]-e)*forward[i];
                hitZ += (target[i]-e)*forward[i];
            }
            const float asymmetry = offset > 0 ? 0.1f : -0.1f;
            assert(close(zoom*reticleX/reticleZ+asymmetry, zoom*hitX/hitZ+asymmetry));
            ++cases;
        }
        for(int i=0; i<3; ++i) target[i] = eye[i]-forward[i];
        assert(!ScopeOverlayMath::targetPlane(out, plane, eye, target, x, 970));
    }
    std::printf("Scope stereo target alignment passed (%d projections).\n", cases);
}
