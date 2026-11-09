#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <initializer_list>
#define nil nullptr

static bool ownership;
struct CPlayerPed { int GetModelIndex() const { return 0; } };
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

int main()
{
    unsigned checks = 0;
    // Exercise a live outfit change in both directions, without resetting state.
    for (bool present : {false, true}) {
        currentPlayer = present ? &instance : nullptr;
        for (bool active : {false, true}) {
            ownership = active;
            for (const char *name : {"player", "playerp", "player", "playerp", "special"}) {
                model.name = name;
                const bool expected = present && active && std::strcmp(name, "playerp") == 0;
                if (UsesOutfitHands() != expected) return EXIT_FAILURE;
                ++checks;
            }
        }
    }
    std::printf("PASS %u live outfit/IK ownership checks\n", checks);
}
