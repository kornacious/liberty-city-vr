#include <cstdio>
#include <cstdlib>
#include <cmath>

#define nil nullptr
#ifndef GARAGE_DESKTOP
#define __ANDROID__ 1
#endif
[[maybe_unused]] static float Abs(float v) { return std::fabs(v); }
enum { GARAGE_MISSION=1, GARAGE_CRUSHER=13, MI_MRWHOOP=113,
    GS_FULLYCLOSED, GS_OPENED, GS_CLOSING, GS_OPENING,
    GS_OPENEDCONTAINSCAR, GS_CLOSEDCONTAINSCAR,
    PLAYERCONTROL_GARAGE, DISTANCE_FOR_MRWHOOP_HACK=2 };
struct Wanted { bool m_bIgnoredByCops=true; };
static Wanted wanted;
struct CPad {
    bool disabled=true;
    static CPad *GetPad(int) { static CPad pad; return &pad; }
    void SetEnablePlayerControls(int) { disabled=false; }
};
struct CVector { float x, y, z; };
struct Sphere { CVector center; float radius; };
struct CColModel { int numSpheres; Sphere *spheres; };
struct Matrix {
    CVector position;
    CVector operator*(CVector v) const { return {v.x+position.x, v.y+position.y, v.z+position.z}; }
};
struct CEntity {
    Wanted *m_pWanted=&wanted;
    Matrix matrix;
    CColModel model;
    CVector GetPosition() { return matrix.position; }
    Matrix GetMatrix() { return matrix; }
    CColModel *GetColModel() { return &model; }
    int GetModelIndex() { return 0; }
    bool IsWithinArea(float,float,float,float) { return false; }
};
using CVehicle=CEntity;
[[maybe_unused]] static CVehicle *FindPlayerVehicle() { return nullptr; }
struct { void *pToGarageWeAreIn=nullptr, *pToGarageWeAreInForHackAvoidFirstPerson=nullptr; } TheCamera;
struct CGarages { static bool bCamShouldBeOutisde; };
bool CGarages::bCamShouldBeOutisde=false;
static CEntity player;
[[maybe_unused]] static CEntity *FindPlayerPed() { return &player; }
static bool tracked;
static CVector trackedEye;
namespace androidgame {
bool VrGetViewBasis(float *position, void *, void *, void *) {
    position[0]=trackedEye.x; position[1]=trackedEye.y; position[2]=trackedEye.z;
    return tracked;
}
}
struct CGarage {
    float m_fX1=0, m_fX2=10, m_fY1=0, m_fY2=10;
    int m_eGarageType=GARAGE_MISSION;
    int m_eGarageState=GS_OPENED;
    bool m_bCameraFollowsPlayer=false, m_bClosingWithoutTargetCar=false;
    bool advancedDoor=false;
    CEntity *m_pDoor1=nullptr, *m_pDoor2=nullptr;
    float m_fDoor1X=0, m_fDoor1Y=5, m_fDoor2X=10, m_fDoor2Y=5;
    float GetGarageCenterX() { return (m_fX1+m_fX2)*0.5f; }
    float GetGarageCenterY() { return (m_fY1+m_fY2)*0.5f; }
    bool IsEntityEntirelyOutside(CEntity *, float);
    bool IsEntityEntirelyInside3D(CEntity *, float) { return true; }
    void UpdateCamera();
    void UpdateClosing();
};
// Extract this function from the patched src/control/Garages.cpp, ending
// before IsGarageEmpty, to exercise the production implementation verbatim.
#include "garage-exit-helper.inc"
#include "garage-camera-helper.inc"
#include "garage-closing-helper.inc"
static unsigned checks;
static void Check(bool value, const char *message) {
    ++checks;
    if (!value) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}
int main() {
    CGarage garage;
    Sphere sphere={{0,0,0},0.5f};
    player.model={1,&sphere};
    player.matrix.position={-4,5,0};
    tracked=true;
    garage.UpdateCamera();
#ifdef __ANDROID__
    Check(TheCamera.pToGarageWeAreIn==nullptr && !CGarages::bCamShouldBeOutisde,
        "Quest garage cannot force fixed camera or movement heading");
#else
    Check(TheCamera.pToGarageWeAreIn==&garage && CGarages::bCamShouldBeOutisde,
        "desktop preserves stock garage camera");
#endif
    // Each side: collision body is clear, room-scale eye remains inside.
    const CVector bodies[]={{-4,5,0},{14,5,0},{5,-4,0},{5,14,0}};
    for (CVector body : bodies) {
        player.matrix.position=body;
        trackedEye={5,5,100}; // Exit clearance remains horizontal, as in stock code.
#ifdef __ANDROID__
        Check(!garage.IsEntityEntirelyOutside(&player,2),"eye still inside must keep delivery open");
#else
        Check(garage.IsEntityEntirelyOutside(&player,2),"desktop retains collision-body policy");
#endif
        trackedEye=body;
        Check(garage.IsEntityEntirelyOutside(&player,2),"body and eye clear accepts delivery");
    }
    player.matrix.position={-4,5,0};
    trackedEye={-2,5,0};
#ifdef __ANDROID__
    Check(!garage.IsEntityEntirelyOutside(&player,2),"eye on clearance boundary must wait");
    trackedEye={-1,5,0};
    Check(!garage.IsEntityEntirelyOutside(&player,2),"delivery preserves two metre clearance");
    Check(garage.IsEntityEntirelyOutside(&player,0),"zero-margin callers retain their margin");
#endif
    tracked=false;
    trackedEye={5,5,0};
    Check(garage.IsEntityEntirelyOutside(&player,2),"unavailable tracking falls back to body");
    tracked=true;
    CEntity car=player;
    Check(garage.IsEntityEntirelyOutside(&car,2),"head position does not affect other entities");
    player.matrix.position={5,5,0}; trackedEye={-4,5,0};
    Check(!garage.IsEntityEntirelyOutside(&player,2),"eye outside cannot bypass body inside");
    player.matrix.position={-2.4f,5,0};
    Check(!garage.IsEntityEntirelyOutside(&player,2),"collision sphere overlap still blocks delivery");
#ifdef __ANDROID__
    CEntity door;
    garage.m_pDoor1=&door;
    const CVector wrongSides[]={{14,5,0},{5,-4,0},{5,14,0}};
    for (CVector body : wrongSides) {
        player.matrix.position=trackedEye=body;
        Check(!garage.IsEntityEntirelyOutside(&player,2),"side/back exit cannot complete delivery");
    }
    // Physical doorway is outside the scripted car acceptance rectangle.
    garage.m_fDoor1X=-3;
    player.matrix.position=trackedEye={-4,5,0};
    Check(!garage.IsEntityEntirelyOutside(&player,2),"body must clear actual doorway margin");
    player.matrix.position=trackedEye={-6,5,0};
    Check(garage.IsEntityEntirelyOutside(&player,2),"actual doorway clear accepts delivery");
    trackedEye={-4,5,0};
    Check(!garage.IsEntityEntirelyOutside(&player,2),"eye must also clear actual doorway");
    tracked=false;
    Check(garage.IsEntityEntirelyOutside(&player,2),"doorway guard works without eye tracking");
    tracked=true;
    garage.m_pDoor2=&door;
    player.matrix.position=trackedEye={14,5,0};
    Check(garage.IsEntityEntirelyOutside(&player,2),"second doorway permits delivery");
    garage.m_pDoor1=nullptr;
    garage.m_fDoor2X=5; garage.m_fDoor2Y=10;
    player.matrix.position=trackedEye={5,14,0};
    Check(garage.IsEntityEntirelyOutside(&player,2),"north doorway orientation");
    garage.m_fDoor2Y=0;
    player.matrix.position=trackedEye={5,-4,0};
    Check(garage.IsEntityEntirelyOutside(&player,2),"south doorway orientation");
#endif
    garage.m_pDoor1=garage.m_pDoor2=nullptr;
    player.matrix.position=trackedEye={5,5,0};
    garage.m_eGarageState=GS_CLOSING;
    garage.UpdateClosing();
#ifdef __ANDROID__
    Check(!garage.advancedDoor && garage.m_eGarageState==GS_OPENING,
        "returning inside reverses delivery closure before consuming car");
    Check(!CPad::GetPad(0)->disabled && !wanted.m_bIgnoredByCops,
        "reversing closure restores controls and wanted policy");
#else
    Check(garage.advancedDoor,"desktop closing behavior preserved");
#endif
    garage.advancedDoor=false; garage.m_eGarageState=GS_CLOSING;
    player.matrix.position=trackedEye={-4,5,0};
    garage.UpdateClosing();
    Check(garage.advancedDoor,"clear exit permits closure");
    garage.advancedDoor=false; garage.m_bClosingWithoutTargetCar=true;
    player.matrix.position=trackedEye={5,5,0};
    garage.UpdateClosing();
    Check(garage.advancedDoor,"non-delivery closure preserves stock behavior");
    std::printf("Garage exit: %u checks passed.\n",checks);
}
