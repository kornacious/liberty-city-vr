#include <cstdio>
#include <cstdlib>
#include <initializer_list>
using uint16 = unsigned short;
struct CVector { float x, y, z; };
struct CEntity {
    enum Kind { Ped, Vehicle, Object, Building } kind;
    int references = 0;
    explicit CEntity(Kind value) : kind(value) {}
    bool IsPed() const { return kind == Ped; }
    bool IsVehicle() const { return kind == Vehicle; }
    void RegisterReference(CEntity **) { ++references; }
};
struct CPhysical : CEntity {
    explicit CPhysical(Kind value) : CEntity(value) {}
    uint16 m_nDamagePieceType = 0;
    float m_fDamageImpulse = 0;
    CEntity *m_pDamageEntity = nullptr;
    CVector m_vecDamageNormal = {};
    void SetDamagedPieceRecord(uint16, float, CEntity *, CVector);
};
#include "ped-vehicle-damage-production.inc"
static void Check(bool value, const char *reason)
{
    if(!value) { std::fprintf(stderr, "FAIL: %s\n", reason); std::exit(1); }
}
int main()
{
    // Repeated sprint/walk contact must stay harmless at every refresh rate,
    // even when the solver's artificial mass factors produce large impulses.
    for(int fps : {30, 60, 72, 90, 120, 144, 240}) {
        for(float impulse : {26.0f, 80.0f, 250.0f, 1000.0f}) {
            CPhysical car(CEntity::Vehicle);
            CEntity ped(CEntity::Ped), wall(CEntity::Building);
            for(int frame = 0; frame < fps * 5; ++frame)
                car.SetDamagedPieceRecord(7, impulse, &ped, {0, 1, 0});
#ifdef FIX_BUGS
            Check(car.m_fDamageImpulse == 0 && car.m_pDamageEntity == nullptr,
                  "body contact recorded vehicle damage");
            Check(ped.references == 0, "ignored contact registered a damage reference");
            car.SetDamagedPieceRecord(2, 50, &wall, {1, 0, 0});
            car.SetDamagedPieceRecord(7, impulse, &ped, {0, 1, 0});
            Check(car.m_nDamagePieceType == 2 && car.m_fDamageImpulse == 50 &&
                  car.m_pDamageEntity == &wall && car.m_vecDamageNormal.x == 1,
                  "body contact overwrote a real crash");
#else
            Check(car.m_fDamageImpulse == impulse && car.m_pDamageEntity == &ped,
                  "legacy collision behavior changed without FIX_BUGS");
#endif
        }
    }
    CEntity car(CEntity::Vehicle), ped(CEntity::Ped), wall(CEntity::Building), prop(CEntity::Object);
    for(auto targetKind : {CEntity::Ped, CEntity::Vehicle, CEntity::Object}) {
        CPhysical target(targetKind);
        for(CEntity *source : {&car, &ped, &wall, &prop}) {
#ifdef FIX_BUGS
            if(targetKind == CEntity::Vehicle && source == &ped) continue;
#endif
            const int before = source->references;
            target.SetDamagedPieceRecord(3, 200, source, {0, 0, 1});
            Check(target.m_fDamageImpulse == 200 && target.m_nDamagePieceType == 3 &&
                  target.m_pDamageEntity == source && target.m_vecDamageNormal.z == 1 &&
                  source->references == before + 1, "legitimate collision record changed");
        }
    }
    std::puts("PASS: pedestrian/car damage records across 30-240 FPS; crash and pedestrian damage preserved.");
}
