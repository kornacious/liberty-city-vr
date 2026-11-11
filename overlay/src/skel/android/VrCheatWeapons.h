#pragma once

// Include WeaponType.h first. The native cheat's handheld weapons, excluding
// vehicle guns and the RC-car detonator.
namespace VrCheatWeapons {
struct Entry { eWeaponType type; const char *name; unsigned int ammo; };
static const Entry kWeapons[] = {
	{ WEAPONTYPE_BASEBALLBAT, "BASEBALL BAT", 0 },
	{ WEAPONTYPE_COLT45, "PISTOL", 100 },
	{ WEAPONTYPE_UZI, "UZI", 100 },
	{ WEAPONTYPE_SHOTGUN, "SHOTGUN", 20 },
	{ WEAPONTYPE_AK47, "AK-47", 200 },
	{ WEAPONTYPE_M16, "M16", 200 },
	{ WEAPONTYPE_SNIPERRIFLE, "SNIPER RIFLE", 5 },
	{ WEAPONTYPE_ROCKETLAUNCHER, "ROCKET LAUNCHER", 5 },
	{ WEAPONTYPE_FLAMETHROWER, "FLAMETHROWER", 200 },
	{ WEAPONTYPE_MOLOTOV, "MOLOTOV COCKTAIL", 5 },
	{ WEAPONTYPE_GRENADE, "GRENADE", 5 }
};
inline int Count() { return sizeof(kWeapons)/sizeof(kWeapons[0]); }
template<class Player> inline bool Give(Player *player, int item)
{
	if(!player || item < 0 || item >= Count()) return false;
	const Entry &weapon = kWeapons[item];
	// Native GiveWeapon adds ammo without replacing an existing weapon.
	player->GiveWeapon(weapon.type, weapon.ammo);
	return true;
}
}
