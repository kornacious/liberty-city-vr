#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#define GTA_VR_WEAPONS
#define nil nullptr
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
enum { HOLSTER_WAIST_LEFT, HOLSTER_WAIST_RIGHT, HOLSTER_CHEST_LEFT,
    HOLSTER_CHEST_RIGHT, HOLSTER_CHEST_CENTRE, HOLSTER_BACK_LEFT,
    HOLSTER_BACK_RIGHT, HOLSTER_POINT_COUNT };
struct CPed {
    bool owned[14] = {};
    bool HasWeapon(int type) { return owned[type]; }
    void GiveWeapon(int type, int) { owned[type] = true; }
};
CPed testPlayer;
bool available = true;
CPed *FindPlayerPed() { return available ? &testPlayer : nullptr; }
const char *GetVrWeaponName(int type) {
    static const char *names[] = {"UNARMED","BAT","PISTOL","UZI","SHOTGUN",
        "AK47","M16","SNIPER","ROCKET","FLAME","MOLOTOV","GRENADE","DETONATOR"};
    return type >= 0 && type < 13 ? names[type] : "NONE";
}
struct CHud { static void SetHelpMessage(const char *, bool) {} };
struct Text { const char *Get(const char *s) { return s; } } TheText;
int gHolsterPointSlot[HOLSTER_POINT_COUNT] = {-1,-1,-1,-1,2,-1,-1};
const char *kSettingsPath = "test";
void LoadSettings() {}
char savedKey[64], savedValue[16];
void WritePrivateProfileStringA(const char *, const char *key, const char *value, const char *) {
    std::snprintf(savedKey,sizeof(savedKey),"%s",key);
    std::snprintf(savedValue,sizeof(savedValue),"%s",value);
}
#include "holster-inventory-production.inc"
void Check(bool ok, const char *message) {
    if (!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
int main() {
    for(int slot : {-1,(int)WEAPONSLOT_PROJECTILE,(int)WEAPONSLOT_GRENADE,(int)WEAPONSLOT_HANDGUN}) {
        gHolsterPointSlot[4]=slot;
        NormalizeLoadedCentre();
        Check(gHolsterPointSlot[4]==(slot==WEAPONSLOT_HANDGUN ? WEAPONSLOT_PROJECTILE : slot),
            "reload preserves empty and throwables while rejecting a non-throwable centre");
    }
    WeaponCheat();
    bool seen[13] = {};
    for (int slot=1; slot<TOTAL_WEAPON_SLOTS; ++slot) {
        const int type=GetVrWeaponTypeForSlot(slot);
        Check(type>0 && type<13,"cheat owns every selectable inventory weapon");
        Check(!seen[type],"slots never hide or alias another weapon");
        seen[type]=true;
        Check(IsVrWeaponSlotOwned(slot) && IsPhysicalWeaponType(type),"every weapon can enter the holster mask");
    }
    for(int type=1;type<13;++type) Check(seen[type],"all twelve weapons covered");
    testPlayer.owned[WEAPONTYPE_M16]=false;
    Check(GetVrWeaponTypeForSlot(WEAPONSLOT_RIFLE)==-1,"unowned slot stays unowned");
    Check(std::strcmp(GetVrHolsterSlotName(WEAPONSLOT_RIFLE),"M16")==0,"unowned assignments retain useful names");
    Check(GetVrWeaponTypeForSlot(WEAPONSLOT_AK47)==WEAPONTYPE_AK47,"AK remains independently accessible");
    available=false;
    Check(GetVrWeaponTypeForSlot(1)==-1 && !IsVrWeaponSlotOwned(1),"missing player is safe");
    Check(std::strcmp(GetVrHolsterSlotName(-1),"EMPTY")==0,"single empty label");
    available=true;
    // Cycle a free point through all choices; the centre's Molotov is reserved.
    bool choices[TOTAL_WEAPON_SLOTS] = {};
    int emptyCount=0;
    for(int i=0;i<TOTAL_WEAPON_SLOTS-1;++i) {
        CycleQuestHolsterPointSlot(0,1);
        const int slot=gHolsterPointSlot[0];
        if(slot<0) ++emptyCount;
        else { Check(!choices[slot],"cycle contains no duplicate assignments"); choices[slot]=true; }
    }
    Check(emptyCount==1 && !choices[WEAPONSLOT_PROJECTILE],"one empty option; skips occupied centre");
    Check(choices[WEAPONSLOT_GRENADE] && choices[WEAPONSLOT_ROCKET] && choices[WEAPONSLOT_OTHER] && choices[WEAPONSLOT_AK47],"missing weapons are selectable");
    CycleQuestHolsterPointSlot(4,1);
    Check(gHolsterPointSlot[4]==WEAPONSLOT_GRENADE,"centre selects grenade");
    CycleQuestHolsterPointSlot(4,1);
    Check(gHolsterPointSlot[4]==-1,"centre selects empty after grenade");
    Check(std::strcmp(savedKey,"HolsterChestCentreSlot")==0 && std::strcmp(savedValue,"-1")==0,
        "empty centre persists under its own settings key");
    CycleQuestHolsterPointSlot(4,1);
    Check(gHolsterPointSlot[4]==WEAPONSLOT_PROJECTILE,"centre wraps empty to Molotov");
    CycleQuestHolsterPointSlot(4,-1);
    Check(gHolsterPointSlot[4]==-1,"centre selects empty in reverse");
    CycleQuestHolsterPointSlot(4,-1);
    Check(gHolsterPointSlot[4]==WEAPONSLOT_GRENADE,"centre wraps empty to grenade in reverse");
    CycleQuestHolsterPointSlot(4,-1);
    Check(gHolsterPointSlot[4]==WEAPONSLOT_PROJECTILE,"centre selects Molotov in reverse");
    gHolsterPointSlot[1]=WEAPONSLOT_GRENADE;
    CycleQuestHolsterPointSlot(4,1);
    Check(gHolsterPointSlot[4]==-1,"centre skips assigned grenade to empty");
    gHolsterPointSlot[2]=WEAPONSLOT_PROJECTILE;
    CycleQuestHolsterPointSlot(4,1);
    Check(gHolsterPointSlot[4]==-1,"centre stays empty when both throwables are assigned elsewhere");
    CycleQuestHolsterPointSlot(4,-1);
    Check(gHolsterPointSlot[4]==-1,"centre stays empty in reverse when both throwables are assigned");
    std::puts("Holster inventory: all twelve weapons, cheat, ownership and loadout cycling passed.");
}
