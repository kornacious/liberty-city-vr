#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#define GTA_VR_WEAPONS
#define nil nullptr
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
enum { VR_HAND_COUNT = 2 };
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
bool gDualWieldPistol = false, gDualWieldSmg = false;
const char *kSettingsPath = "test";
void LoadSettings() {}
char savedKey[64], savedValue[16];
void WritePrivateProfileStringA(const char *, const char *key, const char *value, const char *) {
    std::snprintf(savedKey,sizeof(savedKey),"%s",key);
    std::snprintf(savedValue,sizeof(savedValue),"%s",value);
}
struct ManualReloadState {};
struct DroppedWeapon { int slot=-1,copy=1; };
int gHeldSlot[2]={-1,-1},gHeldCopy[2]={0,1},gHolsterSelection[2]={-1,-1};
int gWeaponRenderSlot[2]={-1,-1},gWeaponContactSlot[2]={-1,-1};
ManualReloadState gManualReloadState[2];
DroppedWeapon gDropped[2];
void ClearSupportForHand(int) {}
namespace QuestDualWield { void Reset(int) {} }
#include "holster-inventory-production.inc"
void Check(bool ok, const char *message) {
    if (!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
int main() {
    const float frontRadii[] = {0.27f, 0.20f, 0.14f};
    for(int setting=0; setting<3; ++setting) {
        gHolsterGrabDistance=setting;
        for(int point=0; point<HOLSTER_POINT_COUNT; ++point) {
            const bool back=point==HOLSTER_BACK_LEFT || point==HOLSTER_BACK_RIGHT;
            const float radius=back ? 0.27f : frontRadii[setting];
            Check(IsHolsterWithinGrabDistance(point,radius),"grab accepts each holster boundary");
            Check(!IsHolsterWithinGrabDistance(point,radius+0.001f),"grab rejects outside each holster boundary");
        }
        Check(IsHolsterWithinGrabDistance(HOLSTER_BACK_LEFT,0.26f),"left shoulder retains far reach");
        Check(IsHolsterWithinGrabDistance(HOLSTER_BACK_RIGHT,0.26f),"right shoulder retains far reach");
    }
    Check(!IsHolsterWithinGrabDistance(-1,0.0f),"unassigned holster cannot be grabbed");
    Check(!IsHolsterWithinGrabDistance(HOLSTER_POINT_COUNT,0.0f),"invalid holster cannot be grabbed");
    gHolsterGrabDistance=0;
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
    for(int slot : {(int)WEAPONSLOT_HANDGUN,(int)WEAPONSLOT_SUBMACHINEGUN}) {
        for(int &point:gHolsterPointSlot) point=-1;
        gDualWieldPistol=gDualWieldSmg=true;
        gHolsterPointSlot[0]=slot;
        for(int attempt=0;attempt<30&&gHolsterPointSlot[1]!=slot;attempt++) CycleQuestHolsterPointSlot(1,1);
        Check(gHolsterPointSlot[1]==slot,"enabled sidearm can occupy opposite-side holsters");
        gHeldSlot[0]=slot;gHeldSlot[1]=-1;gHeldCopy[0]=0;gHeldCopy[1]=1;
        Check(CopyInUse(slot,0)&&!CopyInUse(slot,1),"drawing one copy leaves the opposite holster available");
        gHeldSlot[1]=slot;
        Check(CopyInUse(slot,0)&&CopyInUse(slot,1),"two hands can own distinct copies of the same slot");
        gHeldSlot[0]=gHeldSlot[1]=-1;
        gHolsterPointSlot[2]=-1;
        for(int attempt=0;attempt<30;attempt++) {
            CycleQuestHolsterPointSlot(2,1);
            Check(gHolsterPointSlot[2]!=slot,"cannot assign a third sidearm on the same side");
        }
        // Persisted settings are validated as well as interactive selection.
        gHolsterPointSlot[2]=slot;NormalizeDualWieldHolsters();
        Check(gHolsterPointSlot[0]==slot&&gHolsterPointSlot[1]==slot&&gHolsterPointSlot[2]==-1,"load normalization retains one copy per side");
        gHeldSlot[0]=gHeldSlot[1]=slot;
        gDropped[0].slot=slot;gDropped[0].copy=0;
        const int type=slot==WEAPONSLOT_HANDGUN?WEAPONTYPE_COLT45:WEAPONTYPE_UZI;
        ToggleQuestDualWield(type);
        Check(!IsQuestDualWieldEnabled(type),"toggle disables only selected sidearm type");
        Check(gHeldSlot[0]==-1&&gHeldSlot[1]==slot&&gDropped[0].slot==-1,"disabling releases the secondary held or dropped copy");
        Check(gHolsterPointSlot[0]==slot&&gHolsterPointSlot[1]==-1,"disabling clears the duplicate assignment");
        Check(std::strcmp(savedKey,"HolsterWaistRightSlot")==0&&std::strcmp(savedValue,"-1")==0,"duplicate clearing persists EMPTY");
    }
    for(int type=WEAPONTYPE_UNARMED;type<WEAPONTYPE_TOTALWEAPONS;type++) {
        const bool eligible=type==WEAPONTYPE_COLT45||type==WEAPONTYPE_UZI;
        Check(IsQuestDualWieldEligible(type)==eligible,"dual wield eligibility is limited to pistol and SMG");
        if(!eligible) {
            ToggleQuestDualWield(type);
            Check(!IsQuestDualWieldEnabled(type),"unsupported weapons cannot enable dual wield");
        }
    }
    std::puts("Holster inventory: all twelve weapons, cheat, ownership and loadout cycling passed.");
}
