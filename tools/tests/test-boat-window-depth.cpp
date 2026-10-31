#include "vulkan/VehicleAlphaPolicy.h"
#include <cassert>
#include <cstdio>

#define RW_VULKAN
#define TRUE 1
#define FALSE 0
static bool vehiclePass;
static int bodyDraws;
namespace rw { namespace vulkan {
void setVehicleAlphaPass(bool enabled) { vehiclePass = enabled; }
} }
struct CVehicle { static bool bWheelsOnlyCheat; };
bool CVehicle::bWheelsOnlyCheat;
struct CEntity {
    void Render() {
        ++bodyDraws;
        // Exercise all possible material alpha values in a mixed hull/glass
        // clump. Water drawn behind glass must pass depth; hull must block it.
        for (int alpha = 0; alpha <= 255; ++alpha) {
            const bool blended = alpha < 255;
            const bool split = rw::vulkan::splitVehicleAlpha(vehiclePass, blended, true);
            const bool writesDepth = !blended || !split ||
                alpha / 255.0f >= rw::vulkan::vehicleOpaqueReference();
            const float depth = writesDepth ? 0.2f : 1.0f;
            const bool waterVisible = 0.8f < depth;
            assert(waterVisible == (alpha < 254));
        }
        assert(!rw::vulkan::splitVehicleAlpha(vehiclePass, true, false));
    }
};
struct CBoat : CEntity {
    void Render() {
#include "boat-window-production.inc"
        // The hull water-exclusion draw and subsequent world draws must not
        // inherit the boat's alpha policy.
        assert(!vehiclePass);
    }
};
int main() {
    CBoat boat;
    boat.Render();
    assert(bodyDraws == 1);
    CVehicle::bWheelsOnlyCheat = true;
    boat.Render();
    assert(bodyDraws == 1);
    CVehicle::bWheelsOnlyCheat = false;
    boat.Render();
    assert(bodyDraws == 2);
    assert(!rw::vulkan::splitVehicleAlpha(vehiclePass, true, true));
    std::puts("Boat windows preserve water visibility, hull depth and render state.");
}
