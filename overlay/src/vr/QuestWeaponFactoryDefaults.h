#pragma once

// Headset-authored Save As Default checkpoints, captured locally 2026-10-21.
// Exact half-centimetres / half-degrees; each hand is independent.
// Classic and Xbox use these generic GTA III weapons. Modern stays separate.
namespace QuestWeaponFactoryDefaults {
struct Profile {
    int weaponType, hand;
    // Weapon XYZ, aim XYZ, aim rotation XYZ, weapon rotation XYZ.
    int main[12];
    // Support XYZ, support rotation XYZ, grip style.
    int support[7];
    int supportPoseVersion;
};
static const Profile profiles[] = {
    { WEAPONTYPE_BASEBALLBAT, 0, // LEFT
        { 0,-5,0,0,0,0,0,0,0,0,0,0 },
        { 0,60,-10,0,360,0,0 }, 5 },
    { WEAPONTYPE_BASEBALLBAT, 1, // RIGHT
        { 0,-6,-6,2,0,0,0,0,0,0,0,0 },
        { 0,60,-10,0,360,0,0 }, 5 },
    { WEAPONTYPE_COLT45, 0, // LEFT
        { 1,-7,-1,-2,10,-8,0,0,0,0,0,0 },
        { 0,60,-10,0,360,0,0 }, 5 },
    { WEAPONTYPE_COLT45, 1, // RIGHT
        { 2,-8,-7,-2,10,-8,0,0,0,0,0,0 },
        { 0,60,-10,0,360,0,0 }, 5 },
    { WEAPONTYPE_UZI, 0, // LEFT
        { 2,-8,3,11,8,0,-10,-2,0,0,0,0 },
        { 0,60,-10,0,360,0,0 }, 5 },
    { WEAPONTYPE_UZI, 1, // RIGHT
        { 2,-8,-7,11,8,0,-10,-2,0,0,0,0 },
        { 0,60,-10,0,360,0,0 }, 5 },
    { WEAPONTYPE_SHOTGUN, 0, // LEFT
        { 2,-8,6,-4,12,0,0,0,0,0,0,0 },
        { -3,91,5,354,358,208,1 }, 6 },
    { WEAPONTYPE_SHOTGUN, 1, // RIGHT
        { 2,-8,-4,-4,12,0,0,0,0,0,0,0 },
        { -15,91,5,-25,338,228,1 }, 6 },
    { WEAPONTYPE_AK47, 0, // LEFT
        { 3,-7,0,0,20,0,0,12,13,-1,0,0 },
        { 6,66,9,-14,15,-144,1 }, 6 },
    { WEAPONTYPE_AK47, 1, // RIGHT
        { 3,-7,-7,0,20,0,0,12,13,-1,0,0 },
        { -9,65,4,26,345,216,1 }, 6 },
    { WEAPONTYPE_M16, 0, // LEFT
        { -2,-8,2,-6,25,65,0,0,0,0,0,0 },
        { 2,79,13,-2,0,-132,1 }, 6 },
    { WEAPONTYPE_M16, 1, // RIGHT
        { -2,-8,-7,-6,25,65,0,0,0,0,0,0 },
        { -10,79,13,358,0,-132,1 }, 6 },
    { WEAPONTYPE_SNIPERRIFLE, 0, // LEFT
        { -1,-11,2,0,10,100,0,0,0,0,0,0 },
        { -1,82,7,14,0,-152,1 }, 6 },
    { WEAPONTYPE_SNIPERRIFLE, 1, // RIGHT
        { -1,-11,-3,0,10,100,0,0,0,0,0,0 },
        { -13,82,7,315,0,-152,1 }, 6 },
    { WEAPONTYPE_ROCKETLAUNCHER, 0, // LEFT
        { 4,-3,12,0,0,0,0,0,0,-360,0,60 },
        { 12,120,-23,-300,20,-132,1 }, 6 },
    { WEAPONTYPE_ROCKETLAUNCHER, 1, // RIGHT
        { 4,-6,3,0,0,0,0,0,0,-360,0,60 },
        { 29,120,-24,100,-60,-72,1 }, 6 },
    { WEAPONTYPE_FLAMETHROWER, 0, // LEFT
        { -3,-53,6,0,-7,100,0,0,0,0,0,0 },
        { 6,80,11,-14,33,-172,1 }, 6 },
    { WEAPONTYPE_FLAMETHROWER, 1, // RIGHT
        { -3,-47,-3,0,-7,100,0,0,0,0,0,0 },
        { -11,70,10,24,333,208,1 }, 6 },
    { WEAPONTYPE_MOLOTOV, 0, // LEFT
        { -35,-5,-1,0,0,0,0,0,0,0,0,0 },
        { 0,60,-10,0,360,0,0 }, 5 },
    { WEAPONTYPE_MOLOTOV, 1, // RIGHT
        { -35,-2,-5,0,0,0,0,0,0,0,0,0 },
        { 0,60,0,0,360,0,0 }, 5 },
    { WEAPONTYPE_GRENADE, 0, // LEFT
        { 0,-6,2,0,0,0,0,0,0,0,0,0 },
        { 0,60,-10,0,360,0,0 }, 5 },
    { WEAPONTYPE_DETONATOR, 0, // LEFT
        { 0,-2,2,0,1,0,0,0,0,0,0,0 },
        { 0,60,-10,0,360,0,0 }, 5 },
};

inline const Profile *Find(int weaponType, int hand)
{
    for(unsigned i = 0; i < sizeof(profiles)/sizeof(profiles[0]); ++i)
        if(profiles[i].weaponType == weaponType && profiles[i].hand == hand)
            return &profiles[i];
    return 0;
}
}
