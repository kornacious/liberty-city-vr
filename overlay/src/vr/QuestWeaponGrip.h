#pragma once

// GTA III generic/weapons.dff coordinates: X=barrel, Y=width, Z=height.
// These are grip centres, not model bounds or muzzle positions. Xbox uses
// these same generic weapons; the optional Modern pack has its own profiles.
namespace QuestWeaponGrip {
struct Profile {
	int type;
	float x, y, z;
	float pitch;
};
static const Profile profiles[] = {
	{ WEAPONTYPE_COLT45,       0.055f, 0.010f, -0.020f,   0.0f },
	{ WEAPONTYPE_UZI,          0.065f, 0.010f, -0.030f,   0.0f },
	{ WEAPONTYPE_SHOTGUN,      0.045f,-0.015f, -0.040f,   0.0f },
	{ WEAPONTYPE_AK47,         0.045f, 0.010f, -0.025f,   0.0f },
	{ WEAPONTYPE_M16,          0.045f, 0.010f, -0.005f,   0.0f },
	{ WEAPONTYPE_SNIPERRIFLE,  0.000f,-0.010f,  0.015f,   0.0f },
	{ WEAPONTYPE_ROCKETLAUNCHER,0.100f,0.040f, -0.015f, 107.5f },
	{ WEAPONTYPE_FLAMETHROWER, 0.290f, 0.040f,  0.005f,   0.0f },
	{ WEAPONTYPE_GRENADE,      0.083f, 0.036f,  0.000f,   0.0f },
	{ WEAPONTYPE_MOLOTOV,      0.065f, 0.022f,  0.160f,   0.0f },
	{ WEAPONTYPE_DETONATOR,    0.065f, 0.035f,  0.000f,   0.0f },
};

inline const Profile *Find(int type)
{
	for(unsigned i = 0; i < sizeof(profiles)/sizeof(profiles[0]); ++i)
		if(profiles[i].type == type) return &profiles[i];
	return 0;
}
}
