// Run production action reads, focus transitions, pose gates and logic-only
// pad publication against the bundled OpenXR ABI and a controllable runtime.
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <initializer_list>
#include "../../overlay/vendor/openxr-1.1.58/include/openxr/openxr.h"
#include "xr-focus-input-types.inc"
#define LOGI(...) ((void)0)
#define nil nullptr
using uint32 = uint32_t;

template<class T> static T Handle(uintptr_t id) { return reinterpret_cast<T>(id); }
static struct {
    XrSession session = Handle<XrSession>(1);
    XrActionSet actionSet = Handle<XrActionSet>(1);
    XrSessionState sessionState = XR_SESSION_STATE_FOCUSED;
    XrPath handPath[2] = {1,2};
    XrAction thumbstickAction = Handle<XrAction>(1), triggerAction = Handle<XrAction>(2);
    XrAction squeezeAction = Handle<XrAction>(3), thumbClickAction = Handle<XrAction>(4);
    XrAction aAction = Handle<XrAction>(5), bAction = Handle<XrAction>(6);
    XrAction xAction = Handle<XrAction>(7), yAction = Handle<XrAction>(8), menuAction = Handle<XrAction>(9);
    XrAction gripPoseAction = Handle<XrAction>(10), aimPoseAction = Handle<XrAction>(11);
    XrSpace gripSpace[2] = {Handle<XrSpace>(1),Handle<XrSpace>(2)};
    XrSpace aimSpace[2] = {Handle<XrSpace>(3),Handle<XrSpace>(4)};
    ControllerInput input = {};
    bool inputSynced = false, running = true, headPoseTrackedForFrame = true;
} g;
static XrResult syncResult = XR_SUCCESS, readResult = XR_SUCCESS;
static bool actionsActive = true;
static unsigned syncCalls, readCalls, poseCalls, checks, steps;
static androidgame::PadInput gPadInput{};
static bool gVrRawCrouchJustDown, gVrRawCrouchConsumed;
static void Check(bool value, const char *message) {
    ++checks;
    if(!value) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
template<class T> static bool Neutral(const T &in) {
    if(in.leftStickX || in.leftStickY || in.rightStickX || in.rightStickY ||
        in.leftTrigger || in.rightTrigger || in.leftGrip || in.rightGrip ||
        in.a || in.b || in.x || in.y || in.menu || in.leftStickClick || in.rightStickClick)
        return false;
    for(int hand=0;hand<2;++hand) {
        if(in.gripPose[hand].valid || in.aimPose[hand].valid) return false;
        for(int i=0;i<3;++i)
            if(in.gripPose[hand].position[i] || in.aimPose[hand].position[i]) return false;
        for(int i=0;i<4;++i)
            if(in.gripPose[hand].orientation[i] || in.aimPose[hand].orientation[i]) return false;
    }
    return true;
}
XRAPI_ATTR XrResult XRAPI_CALL xrSyncActions(XrSession, const XrActionsSyncInfo *) {
    ++syncCalls; return syncResult;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateFloat(XrSession, const XrActionStateGetInfo *, XrActionStateFloat *state) {
    ++readCalls; state->currentState=.8f; state->isActive=actionsActive; return readResult;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateBoolean(XrSession, const XrActionStateGetInfo *, XrActionStateBoolean *state) {
    ++readCalls; state->currentState=XR_TRUE; state->isActive=actionsActive; return readResult;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateVector2f(XrSession, const XrActionStateGetInfo *, XrActionStateVector2f *state) {
    ++readCalls; state->currentState={.7f,-.4f}; state->isActive=actionsActive; return readResult;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStatePose(XrSession, const XrActionStateGetInfo *, XrActionStatePose *state) {
    ++poseCalls; state->isActive=actionsActive; return readResult;
}
XRAPI_ATTR XrResult XRAPI_CALL xrLocateSpace(XrSpace, XrSpace, XrTime, XrSpaceLocation *location) {
    ++poseCalls;
    location->locationFlags=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    location->pose.position={1,2,3}; location->pose.orientation={0,0,0,1}; return readResult;
}
namespace androidgame {
void SetPadInput(const PadInput &input);
void Step() { ++steps; Check(Neutral(gPadInput),"logic-only step receives neutral controls and poses"); }
}
static const int VR_HAND_COUNT=2;
static bool gPoseValid[2],gTriggerPressed[2],gTriggerJustPressed[2],gTriggerJustReleased[2],gGripLock;
static int gHeldSlot[2]={-1,-1},gHolsterSelection[2]={-1,-1};
static float gTrigger[2],gGrip[2];
static unsigned dropped;
static void *FindPlayerVehicle() { return nullptr; }
static bool IsImmersiveDrivingActive() { return false; }
struct Position {
    Position operator-(Position) const { return {}; }
    float Magnitude() const { return 0; }
};
struct CMatrix { Position GetPosition() const { return {}; } };
static CMatrix gGripMatrix[2];
static int gHeldCopy[2] = {0,1};
static bool BuildHolsterMatrix(int,CMatrix *,int = -1) { return false; }
static void ClearSupportForHand(int) {}
static void StartDroppedWeapon(int,int) { ++dropped; }
#include "xr-focus-input-production.inc"

static void Held() {
    g.sessionState=XR_SESSION_STATE_FOCUSED;
    syncResult=readResult=XR_SUCCESS; actionsActive=true;
    syncInput(); locateControllerPoses(Handle<XrSpace>(5),100);
    ControllerInput input{}; getInput(&input);
    Check(input.leftStickX==.7f && input.rightStickY==-.4f && input.leftTrigger==.8f &&
        input.rightGrip==.8f && input.a && input.b && input.x && input.y && input.menu &&
        input.leftStickClick && input.rightStickClick && input.gripPose[0].valid &&
        input.aimPose[1].valid,"focused controller sample reaches gameplay");
}
static void ExpectNeutral(const char *message) {
    ControllerInput input{}; input.a=true; getInput(&input); Check(Neutral(input),message);
}
int main() {
    const XrSessionState states[]={XR_SESSION_STATE_VISIBLE,XR_SESSION_STATE_SYNCHRONIZED,
        XR_SESSION_STATE_STOPPING,XR_SESSION_STATE_IDLE,XR_SESSION_STATE_READY,
        XR_SESSION_STATE_EXITING,XR_SESSION_STATE_LOSS_PENDING};
    for(XrSessionState state:states) {
        Held(); SessionEvent(state);
        Check(Neutral(g.input) && !g.inputSynced,"state transition clears the stored held sample immediately");
        ExpectNeutral("unfocused reads are neutral before another frame");
        const unsigned syncBefore=syncCalls, posesBefore=poseCalls;
        syncInput(); locateControllerPoses(Handle<XrSpace>(5),100);
        Check(syncCalls==syncBefore && poseCalls==posesBefore,"unfocused session cannot query gameplay actions or poses");
        SessionEvent(XR_SESSION_STATE_FOCUSED);
        ExpectNeutral("focus regain alone cannot restore old input");
        Held();
    }
    for(XrResult result:{XR_ERROR_RUNTIME_FAILURE,XR_SESSION_NOT_FOCUSED,XR_SESSION_LOSS_PENDING}) {
        Held(); syncResult=result;
        const unsigned before=readCalls;
        syncInput(); locateControllerPoses(Handle<XrSpace>(5),100);
        ExpectNeutral("failed or non-focused sync cannot retain controls or reacquire gameplay poses");
        Check(readCalls==before && !g.inputSynced,"only XR_SUCCESS admits action reads");
    }
    Held(); g.actionSet=XR_NULL_HANDLE; syncInput(); ExpectNeutral("missing action set clears held controls");
    g.actionSet=Handle<XrActionSet>(1);
    Held(); g.sessionState=XR_SESSION_STATE_VISIBLE;
    ExpectNeutral("getInput independently refuses an unfocused cached sample");
    Held(); StartFrame(); ExpectNeutral("frame start clears input before wait/begin can fail");
    Check(!g.headPoseTrackedForFrame,"frame start also invalidates gameplay head tracking");
    Held(); g.running=false; StartFrame(); ExpectNeutral("non-running frame also clears input"); g.running=true;
    Held(); syncInput(); Check(!g.input.gripPose[0].valid && !g.input.aimPose[1].valid,
        "new action sync does not retain previous frame poses");
    Held(); actionsActive=false; syncInput(); locateControllerPoses(Handle<XrSpace>(5),100);
    ExpectNeutral("inactive actions produce neutral input");
    Held(); readResult=XR_ERROR_RUNTIME_FAILURE; syncInput(); locateControllerPoses(Handle<XrSpace>(5),100);
    ExpectNeutral("failed action reads produce neutral input");
    getInput(nullptr);
    gPadInput.leftTrigger=1; gPadInput.leftStickY=1; gPadInput.leftStickClick=true;
    gPadInput.gripPose[0].valid=true; gVrRawCrouchJustDown=true;
    LogicOnly(true,true);
    Check(steps==1 && !gVrRawCrouchJustDown,"logic-only update clears raw crouch edge as well as the pad");
    LogicOnly(false,true); LogicOnly(true,false);
    Check(steps==1,"paused or uninitialized game is not advanced");
    gHeldSlot[0]=2; gGrip[0]=0; gPoseValid[0]=false;
    ReleaseWeapon(0);
    Check(gHeldSlot[0]==2 && dropped==0,"missing pose cannot turn neutral grip into a dropped gun");
    gTriggerPressed[0]=true;
    UpdateWeaponTriggerEdges(false,0,androidgame::PadInput{});
    Check(!gTriggerPressed[0] && !gTriggerJustReleased[0],"unavailable pose clears firing without a synthetic trigger release");
    gPoseValid[0]=true;
    gTriggerPressed[0]=true;
    UpdateWeaponTriggerEdges(false,0,androidgame::PadInput{});
    Check(gTriggerJustReleased[0],"tracked physical trigger release still reaches gameplay");
    ReleaseWeapon(0);
    Check(gHeldSlot[0]==-1 && dropped==1,"tracked physical grip release still drops a weapon");
    std::printf("PASS: focus loss/regain, sync failures, pose gating, frame invalidation and logic-only input: %u checks\n",checks);
}
