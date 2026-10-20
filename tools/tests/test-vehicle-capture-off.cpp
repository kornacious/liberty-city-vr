#include "../../overlay/src/vehicles/VehicleCapture.h"
#include <cstdio>
int main(){
    // Link without the runtime implementation: normal builds need only no-op hooks.
    androidgame::VehicleCaptureFrame frame;
    androidgame::VehicleCaptureInputs(nullptr);
    androidgame::VehicleCaptureCollision(nullptr,0);
    androidgame::VehicleCaptureRetry(nullptr);
    androidgame::VehicleCaptureSubsteps(nullptr,0);
    std::puts("PASS: disabled capture hooks link without runtime or tape code");
}
