#pragma once

#include "QuestWeaponTiming.h"

// The inventory total includes loaded rounds. Temporarily hide the other
// magazine from native Reload/Fire so neither gun can spend its partner's ammo.
namespace QuestDualWield {
struct State {
	bool valid = false;
	const void *owner = nullptr;
	CWeapon secondary;
	QuestWeaponTiming::Cadence cadence;
};
inline State &ForType(int type)
{
	static State states[WEAPONTYPE_TOTALWEAPONS];
	return states[type];
}
inline void Reset(int type) { ForType(type).valid = false; }

class Scope {
	CWeapon &weapon;
	State &state;
	CWeapon primary;
	bool enabled, secondary;
	int reserved = 0;
public:
	Scope(CWeapon &inventory, bool dual, int copy, const void *owner,
	      bool manualReload, uint32 now, int audio) :
		weapon(inventory), state(ForType(inventory.m_eWeaponType)),
		primary(inventory), enabled(dual), secondary(copy == 0)
	{
		if(!enabled){ state.valid = false; return; }
		if(!state.valid || state.owner != owner){
			state.secondary = inventory;
			state.secondary.m_nAmmoInClip = 0;
			state.secondary.m_eWeaponState = WEAPONSTATE_READY;
			state.secondary.m_nTimer = now;
			state.cadence = {};
			state.valid = true;
			state.owner = owner;
		}
		// Inventory scripts can reduce total ammo independently of firing.
		weapon.m_nAmmoInClip = Max(0, Min(weapon.m_nAmmoInClip, weapon.m_nAmmoTotal));
		state.secondary.m_nAmmoInClip = Max(0, Min(state.secondary.m_nAmmoInClip,
			weapon.m_nAmmoTotal-weapon.m_nAmmoInClip));
		primary = weapon;
		reserved = secondary ? primary.m_nAmmoInClip : state.secondary.m_nAmmoInClip;
		if(secondary) weapon = state.secondary;
		weapon.m_nAmmoTotal = Max(0, primary.m_nAmmoTotal-reserved);
		// A newly drawn empty copy uses the usual timed reload (or a physical
		// magazine when manual reload is enabled), never a free cloned clip.
		if(!manualReload && weapon.m_nAmmoInClip == 0 && weapon.m_nAmmoTotal > 0 &&
		   weapon.m_eWeaponState != WEAPONSTATE_RELOADING){
			weapon.m_eWeaponState = WEAPONSTATE_RELOADING;
			weapon.m_nTimer = now+weapon.GetInfo()->m_nReload;
		}
		if(!(manualReload && weapon.m_nAmmoInClip == 0)) weapon.Update(audio);
	}
	~Scope()
	{
		if(!enabled) return;
		const int total = weapon.m_nAmmoTotal+reserved;
		if(secondary){ state.secondary = weapon; weapon = primary; }
		weapon.m_nAmmoTotal = total;
	}
	Scope(const Scope &) = delete;
	Scope &operator=(const Scope &) = delete;
};

inline QuestWeaponTiming::Cadence &Clock(int type, bool dual, int copy)
{
	return dual && copy == 0 ? ForType(type).cadence :
		QuestWeaponTiming::ForWeapon<WEAPONTYPE_TOTALWEAPONS>(type);
}
}
