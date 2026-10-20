#include "ragdoll-test-vector.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

#define __ANDROID__
static float Pow(float a, float b) { return std::pow(a, b); }
static float Abs(float a) { return std::fabs(a); }
static float Min(float a, float b) { return a < b ? a : b; }
struct CTimer {
    static float step;
    static float GetTimeStep() { return step; }
    static float GetTimeStepInSeconds() { return step / 50.0f; }
};
float CTimer::step;
struct CMatrix {};
static CVector Multiply3x3(const CMatrix &, const CVector &v) { return v; }
struct CPhysical {
    float m_fAirResistance = 0.01f;
    CVector m_vecMoveSpeed, m_vecTurnSpeed;
    void ApplyAirResistance();
};
struct cBuoyancy {
    bool m_haveVolume = true;
    CMatrix m_matrix;
    CVector m_impulsePoint = CVector(0.2f, -0.7f, -0.2f);
    float m_volumeUnderWater = 0.5f, m_buoyancy = 12.0f;
    bool CalcBuoyancyForce(CPhysical *, CVector *, CVector *);
};
struct Handling {
    float fSuspensionForceLevel = 0.8f;
    float fSuspensionDampingLevel = 0.25f;
    float fSuspensionBias = 0.4f;
};
struct CBoat : CPhysical {
    Handling handling;
    Handling *pHandling = &handling;
    float m_fGasPedal = 0.7f, m_fMass = 1000.0f;
    bool bBoatInWater = true;
    CVector up = CVector(0.0f, 0.0f, 1.0f), forward = CVector(0.0f, 1.0f, 0.0f);
    CVector force, torque;
    const CVector &GetUp() const { return up; }
    const CVector &GetForward() const { return forward; }
    // Capture impulses without feeding velocity back: isolate rate dependence
    // at the same speed, immersion and orientation rather than changing inputs.
    void ApplyMoveForce(const CVector &v) { force += v; }
    void ApplyTurnForce(const CVector &v, const CVector &point) { torque += CrossProduct(point, v); }
    void ExtractedLift(bool, const CVector &, const CVector &);
};
#pragma warning(push)
#pragma warning(disable: 4100) // Existing unused production buoyancy argument.
#include "angular-drag-boat-lift-production.inc"
#pragma warning(pop)

static unsigned checks, failures;
static void Check(bool ok, const char *message) {
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); }
}
static bool Near(float a, float b, float tolerance = 0.0001f) {
    return std::fabs(a-b) <= tolerance * (1.0f + std::fabs(b));
}
static bool Near(const CVector &a, const CVector &b) {
    return Near(a.x,b.x) && Near(a.y,b.y) && Near(a.z,b.z);
}
static void Drag(int fps, float scale, bool jitter) {
    CPhysical p;
    const CVector initial(0.3f, -0.4f, 0.7f);
    p.m_vecTurnSpeed = initial;
    p.m_vecMoveSpeed = CVector(0.4f, -0.2f, 0.1f);
    double elapsed = 0;
    for (int i = 0; i < fps*10; ++i) {
        const float dt = scale / fps * (jitter ? (i%2 ? 1.15f : 0.85f) : 1.0f);
        CTimer::step = dt*50.0f;
        elapsed += dt;
        p.ApplyAirResistance();
    }
    const float factor = float(std::pow(0.99, elapsed*30.0));
    Check(Near(p.m_vecTurnSpeed, initial*factor), "angular decay follows game time and the 30 FPS reference");
    std::printf("drag %d FPS scale=%.2f jitter=%d spin=%.6f expected=%.6f\n", fps, scale, jitter, p.m_vecTurnSpeed.z/initial.z, factor);
}
static CVector Lift(int fps, float scale, bool jitter, float speed, CVector &torque) {
    CBoat boat;
    boat.m_vecMoveSpeed = CVector(0.0f, speed, 0.0f);
    cBuoyancy buoyancy;
    double elapsed = 0, baseImpulse = 0;
    for (int i = 0; i < fps*4; ++i) {
        const float dt = scale / fps * (jitter ? (i%2 ? 1.15f : 0.85f) : 1.0f);
        CTimer::step = dt*50.0f;
        elapsed += dt;
        CVector point, impulse;
        Check(buoyancy.CalcBuoyancyForce(&boat, &point, &impulse), "submerged hull produces buoyancy");
        baseImpulse += impulse.z;
        boat.ExtractedLift(false, point, impulse);
    }
    // Independent 30 FPS reference: sum the original impulse and its cap at
    // that rate over the same game-time duration. Covers force and pitch torque.
    const double legacyStep = 50.0/30.0;
    const double legacyBuoyancy = buoyancy.m_volumeUnderWater*buoyancy.m_buoyancy*legacyStep;
    const double legacyLift = std::fmin(speed*speed*boat.handling.fSuspensionForceLevel*legacyBuoyancy*legacyStep*0.5*boat.m_fGasPedal,
        0.008*boat.handling.fSuspensionDampingLevel*boat.m_fMass*legacyStep);
    const float expected = float(legacyLift*30.0*elapsed);
    Check(Near(float(baseImpulse), float(buoyancy.m_volumeUnderWater*buoyancy.m_buoyancy*50.0*elapsed)), "base buoyancy is already timestep-scaled");
    Check(Near(boat.force, CVector(0,0,expected)), "boat lift agrees with 30 FPS in uncapped and capped regimes");
    const CVector arm = buoyancy.m_impulsePoint - boat.handling.fSuspensionBias*boat.forward;
    Check(Near(boat.torque, CrossProduct(arm, CVector(0,0,expected))), "boat lift pitch torque retains its original lever arm and rate");
    torque = boat.torque;
    std::printf("lift %d FPS scale=%.2f jitter=%d speed=%.1f impulse=%.6f expected=%.6f\n", fps, scale, jitter, speed, boat.force.z, expected);
    return boat.force;
}
int main() {
    for (int fps : {20,30,36,60,72,80,90,120,144}) {
        for (float scale : {1.0f, 0.5f}) {
            for (bool jitter : {false, true}) {
                Drag(fps,scale,jitter);
                for (float speed : {0.3f, 3.0f}) {
                    CVector torque;
                    Lift(fps,scale,jitter,speed,torque);
                }
            }
        }
    }
    CTimer::step = 50.0f/30.0f;
    CPhysical p;
    p.m_vecTurnSpeed = CVector(1,2,3); p.ApplyAirResistance();
    Check(Near(p.m_vecTurnSpeed, CVector(0.99f,1.98f,2.97f)), "30 FPS angular damping is preserved");
    // The already time-scaled high-resistance branch must retain its behavior.
    p.m_fAirResistance = 0.5f;
    p.m_vecTurnSpeed = p.m_vecMoveSpeed = CVector(1,2,3);
    p.ApplyAirResistance();
    Check(Near(p.m_vecTurnSpeed, CVector(1,2,3)*Pow(0.5f,CTimer::step)), "high-resistance angular damping stays unchanged");
    Check(Near(p.m_vecMoveSpeed,p.m_vecTurnSpeed), "high-resistance linear damping stays unchanged");
    p.m_fAirResistance = 0.01f;
    p.m_vecMoveSpeed = CVector(1,2,3);
    p.ApplyAirResistance();
    Check(Near(p.m_vecMoveSpeed,CVector(1,2,3)*Pow(1.0f/(1.0f+0.01f*0.5f*14.0f),CTimer::step)), "low-resistance linear damping stays unchanged");
    CBoat boat;
    boat.m_vecMoveSpeed = CVector(0,0.3f,0);
    cBuoyancy buoyancy;
    CVector point, impulse;
    buoyancy.CalcBuoyancyForce(&boat,&point,&impulse);
    boat.ExtractedLift(true,point,impulse);
    boat.bBoatInWater = false; boat.ExtractedLift(false,point,impulse);
    boat.bBoatInWater = true; boat.up.z = -1; boat.ExtractedLift(false,point,impulse);
    boat.up.z = 1;
    for (float throttle : {-1.0f, 0.0f, 0.05f}) {
        boat.m_fGasPedal = throttle; boat.ExtractedLift(false,point,impulse);
    }
    Check(Near(boat.force,CVector()) && Near(boat.torque,CVector()), "grounded, dry, inverted, idle and reverse lift gates are preserved");
    buoyancy.m_haveVolume = false;
    Check(!buoyancy.CalcBuoyancyForce(&boat,&point,&impulse), "no submerged volume produces no buoyancy");
    buoyancy.m_haveVolume = true;
    CTimer::step = 0;
    buoyancy.CalcBuoyancyForce(&boat,&point,&impulse);
    boat.m_fGasPedal = 1; boat.ExtractedLift(false,point,impulse);
    p.m_vecTurnSpeed = CVector(1,2,3); p.ApplyAirResistance();
    Check(Near(boat.force,CVector()) && Near(p.m_vecTurnSpeed,CVector(1,2,3)), "zero timestep applies neither lift nor angular drag");
    std::printf("%s: %u drag/buoyancy/lift checks, %u failures\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
