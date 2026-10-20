#pragma once
class CAutomobile;
class CPhysical;
namespace androidgame {
#if defined(__ANDROID__) && defined(LIBERTYVR_VEHICLE_CAPTURE) && LIBERTYVR_VEHICLE_CAPTURE
void VehicleCaptureBeginFrame();
void VehicleCaptureEndFrame();
void VehicleCaptureInputs(CAutomobile *car);
void VehicleCaptureCollision(const CPhysical *car,int contacts);
void VehicleCaptureRetry(const CPhysical *car);
void VehicleCaptureSubsteps(const CPhysical *car,int steps);
#else
inline void VehicleCaptureBeginFrame(){}
inline void VehicleCaptureEndFrame(){}
inline void VehicleCaptureInputs(CAutomobile *){}
inline void VehicleCaptureCollision(const CPhysical *,int){}
inline void VehicleCaptureRetry(const CPhysical *){}
inline void VehicleCaptureSubsteps(const CPhysical *,int){}
#endif
struct VehicleCaptureFrame {
    VehicleCaptureFrame(){VehicleCaptureBeginFrame();}
    ~VehicleCaptureFrame(){VehicleCaptureEndFrame();}
};
}
