#include <cstdio>
#include <cstdlib>

static bool gVrCameraInsidePlayerHead, gVrInVehicle, gVrHidePlayerBody;
static bool hands, bodyIK, vehicleIK, immersive, knockdown, vehicleTransition;
static bool trainPassenger;
namespace androidgame {
bool VrVehicleBodyIKEnabled() { return vehicleIK; }
bool VrBodyIKEnabled() { return bodyIK; }
bool VrKnockdownPresentationActive() { return knockdown; }
}
namespace OculusVR {
bool AreTrackedHandsEnabled() { return hands; }
bool IsImmersiveDrivingActive() { return immersive; }
}
static void UpdateVisibility()
{
#include "vr-body-visibility-production.inc"
}

int main()
{
    // Run all combinations, including live toggle changes on the same state.
    // Outside-head views cover cinematics, death, remote and third-person views.
    unsigned checks = 0;
    for (unsigned state = 0; state < 512; ++state) {
        gVrCameraInsidePlayerHead = (state & 1) != 0;
        gVrInVehicle = (state & 2) != 0;
        bodyIK = (state & 4) != 0;
        vehicleIK = (state & 8) != 0;
        immersive = (state & 16) != 0;
        knockdown = (state & 32) != 0;
        vehicleTransition = (state & 64) != 0;
        hands = (state & 128) != 0;
        trainPassenger = (state & 256) != 0;
        for (int toggle = 0; toggle < 3; ++toggle) {
            bool expected = false;
            if (gVrCameraInsidePlayerHead && trainPassenger) {
                expected = false;
            } else if (!trainPassenger && gVrCameraInsidePlayerHead && !vehicleTransition) {
                if (!hands) expected = true;
                else if (gVrInVehicle) expected = immersive && !vehicleIK;
                else expected = !bodyIK && !knockdown;
            }
            UpdateVisibility();
            if (gVrHidePlayerBody != expected) {
                std::fprintf(stderr, "Visibility failed: state=%u toggle=%d\n", state, toggle);
                return EXIT_FAILURE;
            }
            ++checks;
            hands = !hands;
        }
    }
    std::printf("Body visibility: %u context/toggle checks passed.\n", checks);
}
