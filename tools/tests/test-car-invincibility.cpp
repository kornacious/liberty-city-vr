#include <cstdio>
#include <cstdlib>
#define nil nullptr
struct CPlayerPed {};
struct CEntity {
	CEntity **reference = nullptr;
	void RegisterReference(CEntity **p) { reference = p; }
	void PruneReferences() { if(reference && *reference != this) reference = nullptr; }
	void ResolveReferences() { if(reference && *reference == this) *reference = nullptr; reference = nullptr; }
};
enum { VEHICLE_TYPE_CAR, VEHICLE_TYPE_BOAT };
struct CVehicle : CEntity {
	CPlayerPed *pDriver = nullptr;
	unsigned char bCanBeDamaged = true;
	unsigned bBulletProof : 1;
	unsigned bFireProof : 1;
	unsigned bCollisionProof : 1;
	unsigned bMeleeProof : 1;
	unsigned bExplosionProof : 1;
	CVehicle() : bBulletProof(0), bFireProof(0), bCollisionProof(0), bMeleeProof(0), bExplosionProof(0) {}
	int m_vehType = VEHICLE_TYPE_CAR, extinguished = 0;
	float m_fHealth = 500.0f;
	void ExtinguishCarFire() { ++extinguished; }
};
struct CAutomobile : CVehicle {
	int fixed = 0;
	struct { int engine = 5; void SetEngineStatus(int value) { engine = value; } } Damage;
	void Fix() { ++fixed; }
};
static CPlayerPed claude, other;
static CPlayerPed *activePlayer = &claude;
static CVehicle *current = nullptr;
static CPlayerPed *FindPlayerPed() { return activePlayer; }
static CVehicle *FindPlayerVehicle() { return current; }
#include "car-invincibility-production.inc"
static int checks;
static void Check(bool ok, const char *label) {
	++checks;
	if(!ok) { std::fprintf(stderr, "FAIL: %s\n", label); std::exit(1); }
}
static unsigned Flags(const CVehicle &v) {
	return unsigned(v.bCanBeDamaged) | (unsigned(v.bBulletProof) << 1) |
		(unsigned(v.bFireProof) << 2) | (unsigned(v.bCollisionProof) << 3) |
		(unsigned(v.bMeleeProof) << 4) | (unsigned(v.bExplosionProof) << 5);
}
static void Flags(CVehicle &v, unsigned mask) {
	v.bCanBeDamaged = (mask & 1) != 0; v.bBulletProof = (mask & 2) != 0;
	v.bFireProof = (mask & 4) != 0; v.bCollisionProof = (mask & 8) != 0;
	v.bMeleeProof = (mask & 16) != 0; v.bExplosionProof = (mask & 32) != 0;
}
int main() {
	for(unsigned mask = 0; mask < 64; ++mask) {
		CAutomobile car; Flags(car, mask); car.pDriver = &claude; current = &car;
		SetVrCarInvincibility(false);
		Check(Flags(car) == mask && car.fixed == 0, "OFF preserves all native protection combinations");
		SetVrCarInvincibility(true);
		Check(Flags(car) == 62 && car.m_fHealth == 1000 && car.fixed == 1 && car.Damage.engine == 0,
			"ON protects and repairs the driven car");
		for(int frame = 0; frame < 5; ++frame) ApplyVrCarInvincibility();
		Check(car.fixed == 1, "repeated ticks do not recapture the override or repeatedly repair");
		current = nullptr; ApplyVrCarInvincibility();
		Check(Flags(car) == mask && !car.reference, "exit restores exact mission/model flags and releases reference");
		current = &car; ApplyVrCarInvincibility(); SetVrCarInvincibility(false);
		Check(Flags(car) == mask && !car.reference, "disable while driving restores exact flags");
		Flags(car, mask ^ 63); SetVrCarInvincibility(true); SetVrCarInvincibility(false);
		Check(Flags(car) == (mask ^ 63), "next activation captures fresh native state");
	}
	CAutomobile a, b; a.pDriver = b.pDriver = &claude; Flags(a, 32); Flags(b, 10);
	current = &a; SetVrCarInvincibility(true); current = &b; ApplyVrCarInvincibility();
	Check(Flags(a) == 32 && Flags(b) == 62 && !a.reference, "direct vehicle switch restores previous car");
	b.pDriver = &other; ApplyVrCarInvincibility();
	Check(Flags(b) == 10, "losing driver seat restores car while still a passenger");
	ApplyVrCarInvincibility(); Check(Flags(b) == 10, "passenger receives no cheat protection");
	b.pDriver = &claude; ApplyVrCarInvincibility(); activePlayer = nullptr; ApplyVrCarInvincibility();
	Check(Flags(b) == 10, "missing player restores previous car"); activePlayer = &claude;
	ApplyVrCarInvincibility(); b.ResolveReferences(); current = nullptr; ApplyVrCarInvincibility();
	Check(!gVrInvincibleVehicle, "deleted vehicle clears tracked reference safely");
	Flags(b, 21); current = &b; ApplyVrCarInvincibility(); SetVrCarInvincibility(false);
	Check(Flags(b) == 21, "reused vehicle address captures new state after deletion");
	CVehicle boat; boat.m_vehType = VEHICLE_TYPE_BOAT; boat.pDriver = &claude;
	current = &boat; SetVrCarInvincibility(true); current = nullptr; ApplyVrCarInvincibility();
	Check(Flags(boat) == 1, "non-car vehicle restores without automobile access");
	SetVrCarInvincibility(false);
	std::printf("Car invincibility production regression: %d checks passed\n", checks);
}
