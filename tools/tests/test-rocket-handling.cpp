#include <cstdio>
#include <cstdlib>
#include "weapon-grip-types.inc"
static const int VR_HAND_COUNT = 2;
static int gHeldSlot[2], gSupportHand[2];
static bool gPoseValid[2], gTriggerPressed[2], gTriggerJustPressed[2];
static float gGrip[2];
static bool gameplay, vehicle, detonator[2];
static bool IsGameplayAvailable() { return gameplay; }
static bool FindPlayerVehicle() { return vehicle; }
static bool IsTrackedDetonatorActive(int hand) { return detonator[hand]; }
#include "rocket-trigger-production.inc"
static void Check(bool ok, const char *message) {
    if(!ok) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}
static void Reset(int hand) {
    gameplay = true; vehicle = false;
    for(int h = 0; h < 2; h++) {
        gHeldSlot[h] = h == hand ? 1 : -1;
        gSupportHand[h] = -1; gPoseValid[h] = true;
        gGrip[h] = 1.0f; detonator[h] = false;
        gTriggerPressed[h] = gTriggerJustPressed[h] = true;
    }
}
int main() {
    const int rocket = WEAPONTYPE_ROCKETLAUNCHER;
    for(int hand = 0; hand < 2; hand++) {
        const int support = 1-hand;
        Reset(hand);
        Check(!IsTrackedWeaponFireTriggerPressed(hand, rocket), "one-handed rocket blocked");
        Check(!IsTrackedWeaponFireTriggerJustPressed(hand, rocket), "one-handed edge blocked");
        gSupportHand[hand] = support;
        Check(GetTrackedWeaponFireTriggerHand(hand, rocket) == support, "two-handed rocket uses support trigger");
        Check(IsTrackedWeaponFireTriggerJustPressed(hand, rocket), "two-handed edge accepted");
        gTriggerJustPressed[support] = false;
        Check(!IsTrackedWeaponFireTriggerJustPressed(hand, rocket), "gripping with trigger held does not create an edge");
        gPoseValid[support] = false;
        Check(!IsTrackedWeaponFireTriggerPressed(hand, rocket), "lost support tracking blocked");
        gPoseValid[support] = true;
        for(int released = 0; released < 2; released++) {
            gGrip[released] = 0.30f;
            Check(!IsTrackedWeaponFireTriggerPressed(hand, rocket), "either released grip blocked despite latched ownership");
            gGrip[released] = 1.0f;
        }
        gHeldSlot[support] = 2;
        Check(!IsTrackedWeaponFireTriggerPressed(hand, rocket), "occupied support hand blocked");
        gHeldSlot[support] = -1; detonator[support] = true;
        Check(!IsTrackedWeaponFireTriggerPressed(hand, rocket), "detonator hand blocked");
        detonator[support] = false; vehicle = true;
        Check(GetTrackedWeaponFireTriggerHand(hand, rocket) == hand, "vehicle keeps existing trigger mapping");
        gSupportHand[hand] = -1;
        Check(!IsTrackedWeaponFireTriggerPressed(hand, rocket), "vehicle cannot bypass two-hand requirement");
        vehicle = false;
        Check(GetTrackedWeaponFireTriggerHand(hand, WEAPONTYPE_COLT45) == hand, "pistol remains one-handed");
        gPoseValid[hand] = false;
        Check(!IsTrackedWeaponFireTriggerPressed(hand, rocket), "lost primary tracking blocked");
        Reset(hand); gSupportHand[hand] = support; gameplay = false;
        Check(!IsTrackedWeaponFireTriggerPressed(hand, rocket), "menus cannot fire");
    }
    Check(GetTrackedWeaponFireTriggerHand(-1, rocket) == -1, "invalid hand blocked");
    Check(GetTrackedWeaponFireTriggerHand(2, rocket) == -1, "out-of-range hand blocked");
    std::puts("Rocket handling: both hands, grip release, tracking, trigger edges, vehicles and pistol checks passed.");
}
