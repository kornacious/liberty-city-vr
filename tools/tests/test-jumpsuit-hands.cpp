#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <initializer_list>
#define nil nullptr

static bool ownership;
static bool transition, skinned, hasClump;
static bool VehicleTransitionActive() { return transition; }
static bool IsClumpSkinned(void *) { return skinned; }
struct CPlayerPed {
    int GetModelIndex() const { return 0; }
    void *GetClump() const { return hasClump ? (void *)this : nullptr; }
};
static CPlayerPed instance;
static CPlayerPed *currentPlayer;
static CPlayerPed *FindPlayerPed() { return currentPlayer; }
struct ModelInfo {
    const char *name;
    const char *GetModelName() const { return name; }
};
static ModelInfo model;
namespace CModelInfo {
static ModelInfo *GetModelInfo(int) { return &model; }
}
static bool WeaponOwnershipActive() { return ownership; }
#include "jumpsuit-hands-production.inc"

struct CMatrix {};
static bool gHolsteredWeaponsVisible, train, tracked, vehicle, immersive, inUse;
static unsigned gWeaponHolsterMask;
static void LoadSettings() {}
static bool IsQuestTrainPassenger() { return train; }
static bool AreTrackedHandsEnabled() { return tracked; }
static bool FindPlayerVehicle() { return vehicle; }
static bool IsImmersiveDrivingActive() { return immersive; }
static bool CopyInUse(int, int) { return inUse; }
static bool BuildHolsterMatrix(int, CMatrix *, int) { return true; }
namespace VrBodyIK { static bool VehicleTransitionActive() { return transition; } }
#include "vehicle-holsters-production.inc"

int main()
{
    unsigned checks = 0;
    // Exercise a live outfit change in both directions, without resetting state.
    for (bool present : {false, true}) {
        currentPlayer = present ? &instance : nullptr;
        for (bool active : {false, true}) {
            ownership = active;
            for (bool skin : {false, true}) {
                skinned = skin;
                for (bool clump : {false, true}) {
                    hasClump = clump;
                    // Enter, finish, exit, finish: no outfit or ownership reset.
                    for (bool moving : {false, true, false, true, false}) {
                        transition = moving;
                        for (const char *name : {"player", "playerp", "player", "playerp", "special"}) {
                            model.name = name;
                            const bool expected = present && ((moving && skin && clump) ||
                                (active && std::strcmp(name, "playerp") == 0));
                            if (UsesOutfitHands() != expected) return EXIT_FAILURE;
                            ++checks;
                        }
                    }
                }
            }
        }
    }
    for (unsigned flags = 0; flags < 256; ++flags) {
        gHolsteredWeaponsVisible = flags & 1; train = flags & 2;
        tracked = flags & 4; vehicle = flags & 8; immersive = flags & 16;
        inUse = flags & 32; gWeaponHolsterMask = flags & 64 ? 1u << 2 : 0;
        transition = flags & 128;
        CMatrix matrix;
        const bool expected = gHolsteredWeaponsVisible && !train && tracked &&
            (!vehicle || immersive) && !inUse && gWeaponHolsterMask && !transition;
        if (GetWeaponHolsterMatrix(2, &matrix, -1) != expected) return EXIT_FAILURE;
        ++checks;
    }
    std::printf("PASS %u outfit/transition hand and holster rendering checks\n", checks);
}
