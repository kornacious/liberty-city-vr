#include "VehiclePlacement.h"
#include <iostream>
#include <stdexcept>
#define nil nullptr
struct CVector {
    float x = 0, y = 0, z = 0;
    CVector operator*(float f) const {
        return {x * f, y * f, z * f};
    }
    CVector &operator+=(CVector b) {
        x += b.x;
        y += b.y;
        z += b.z;
        return *this;
    }
};
struct CMatrix {
    CVector p;
    CVector &GetPosition() {
        return p;
    }
};
struct CVehicle {
    int GetModelIndex() {
        return 95;
    }
    bool IsBoat() {
        return false;
    }
    CVector GetForward() {
        return {0, 1, 0};
    }
};
static CVehicle car, *active = &car;
static CVehicle *FindPlayerVehicle() {
    return active;
}
namespace OculusVR {
struct DefaultVehicleViewOffset {
    int seatDistanceCm = 22, seatHeightCm = 31;
};
struct VehicleViewCalibration {
    int defaultSeatHeightCm = 7, defaultSeatDistanceCm = -3, seatHeightCm = 11, seatDistanceCm = -8;
    int wheelCenterXCm = 0, wheelCenterYCm = 0, wheelCenterZCm = 0;
    int carWheelPitchHalfDeg = 0, carWheelYawHalfDeg = 0, carWheelRollHalfDeg = 0;
    int carWheelRadiusCm = 0, carWheelVisibilityOverride = -1;
};
struct VehicleCategoryCalibration {
    int seatHeightCm = 25, seatDistanceCm = 19, wheelCenterXCm = 4, wheelCenterYCm = 3,
        wheelCenterZCm = 2;
    int carWheelPitchHalfDeg = 2, carWheelYawHalfDeg = 0, carWheelRollHalfDeg = 0, carWheelRadiusCm = 18;
};
enum { VR_DEFAULT_VIEW_CAR, VR_DEFAULT_VIEW_BOAT };
static DefaultVehicleViewOffset gDefaultVehicleViewOffset[2];
static VehicleViewCalibration model;
static VehicleCategoryCalibration category;
static int mode = 0;
static void LoadDrivingSettings() {}
static int GetVehicleCategory(CVehicle *) {
    return 0;
}
static int GetDefaultVehicleViewTypeForCurrentVehicle() {
    return mode;
}
static VehicleViewCalibration *GetViewCalibration(int) {
    return &model;
}
static VehicleCategoryCalibration *GetCategoryCalibration(CVehicle *) {
    return &category;
}
static int PlacementSet() {
    return 0;
}
static float PlacementValue(int id, const char *key, float fallback) {
    return VehiclePlacement::runtime().value(0, id, key, fallback);
}
// Extracted at CMake configuration from the production Quest source.
#include "runtime-calibration.inc"
#include "runtime-seat.inc"
} // namespace OculusVR
static void check(float a, float b) {
    if (std::abs(a - b) > 1e-5f)
        throw std::runtime_error("production seat placement mismatch");
}
int main() {
    try {
        auto &store = VehiclePlacement::runtime();
        store = VehiclePlacement::Store();
        auto s = VehiclePlacement::section(0, 95);
        store.personal.entries[s]["MapX"] = 4;
        OculusVR::RefreshViewPlacement(95, OculusVR::model);
        check((float)OculusVR::model.carWheelRadiusCm, 0);
        store.personal.entries[s]["WheelX"] = 50;
        store.personal.entries[s]["WheelRadius"] = 21;
        OculusVR::RefreshViewPlacement(95, OculusVR::model);
        check((float)OculusVR::model.wheelCenterXCm, 46);
        check((float)OculusVR::model.carWheelRadiusCm, 21);
        OculusVR::category.wheelCenterXCm = 12;
        OculusVR::RefreshViewPlacement(95, OculusVR::model);
        check((float)OculusVR::model.wheelCenterXCm, 38);
        store = VehiclePlacement::Store();
        OculusVR::model = OculusVR::VehicleViewCalibration();
        OculusVR::category = OculusVR::VehicleCategoryCalibration();
        CMatrix m;
        OculusVR::ApplyQuestVehicleViewOffset(&m);
        check(m.p.y, .19f);
        check(m.p.z, .38f);
        OculusVR::mode = -1;
        m = {};
        OculusVR::ApplyQuestVehicleViewOffset(&m);
        check(m.p.y, .11f);
        check(m.p.z, .36f);
        store.templates.entries[s]["SeatHeight"] = 42;
        store.personal.entries[s]["SeatDistance"] = -12;
        store.personal.entries[VehiclePlacement::section(2, 95)]["SeatHeight"] = 99;
        m = {};
        OculusVR::ApplyQuestVehicleViewOffset(&m);
        check(m.p.y, -.12f);
        check(m.p.z, .42f);
        store.personal.entries[s]["SeatHeight"] = 50;
        m = {};
        OculusVR::ApplyQuestVehicleViewOffset(&m);
        check(m.p.z, .5f);
        store.personal.entries[s].erase("SeatHeight");
        m = {};
        OculusVR::ApplyQuestVehicleViewOffset(&m);
        check(m.p.z, .42f);
        store.personal.entries[s]["DefaultSeatHeight"] = 60;
        OculusVR::mode = 0;
        m = {};
        OculusVR::ApplyQuestVehicleViewOffset(&m);
        check(m.p.z, .6f);
        check(m.p.y, .19f);
        active = nullptr;
        m = {};
        OculusVR::ApplyQuestVehicleViewOffset(&m);
        check(m.p.z, 0);
        OculusVR::ApplyQuestVehicleViewOffset(nullptr);
        std::cout << "Production seat placement tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
