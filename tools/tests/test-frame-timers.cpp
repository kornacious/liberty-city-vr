#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

// Exercise the Android production path with deterministic display timestamps.
#undef _WIN32
#define __ANDROID__
#define GTA_VR_WEAPONS
#define FIX_BUGS
using uint32 = uint32_t;
using uint64 = uint64_t;
using int32 = int32_t;
template<class T> T Min(T a, T b) { return a < b ? a : b; }
static double clockMs;
static double RsTimer() { return clockMs; }
#define debug(...) ((void)0)
static void OutputDebugString(const char *) {}
static struct Audio { void ResetTimers(uint32) {} } DMAudio;
struct CRecordDataForGame { static bool IsPlayingBack() { return false; } };
struct CRecordDataForChase { static bool IsRecording() { return false; } };
#pragma warning(push)
#pragma warning(disable: 4244) // Existing production numeric conversions.
#include "frame-timer-header.inc"
#include "frame-timer-production.inc"

struct ScriptTimers {
    enum { NUM_LOCAL_VARS = 0 };
    int32 m_anLocalVariables[2] = {0, 0};
#include "frame-timer-script.inc"
};
static void Drain(float &m_fHealth, int engineStatus) {
    enum { ENGINE_STATUS_ON_FIRE = 225 };
#include "frame-timer-engine.inc"
}
#pragma warning(pop)
static unsigned checks;
static void Check(bool ok, const char *message) {
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static void Tick(double milliseconds) {
    clockMs += milliseconds;
    CTimer::Update();
}
static void Reset(float scale = 1.0f) {
    CTimer::Initialise();
    CTimer::SetTimeScale(scale);
}
static void RunRate(int fps, float scale, bool jitter) {
    Reset(scale);
    ScriptTimers scripts;
    scripts.m_anLocalVariables[1] = -100000;
    int countdown = 60000;
    uint32 alarm = 60000, accumulated = 0;
    double expected = 0;
    for (int frame = 0; frame < fps * 60; ++frame) {
        const double dt = 1000.0 / fps * (jitter ? (frame % 2 ? 1.15 : 0.85) : 1.0);
        Tick(dt);
        expected += dt * scale;
        const uint32 ms = CTimer::GetTimeStepInMilliseconds();
        Check(ms == CTimer::GetTimeStepInMilliseconds(), "multiple consumers receive the same delta");
        accumulated += ms;
        scripts.UpdateTimers(float(ms));
        countdown = countdown > int(ms) ? countdown - int(ms) : 0;
        alarm = alarm > ms ? alarm - ms : 0;
    }
    Check(std::fabs(double(accumulated) - expected) < 2.0, "fractional milliseconds survive 60 seconds of updates");
    Check(scripts.m_anLocalVariables[0] == int(accumulated), "script timer matches accumulated frame time");
    Check(scripts.m_anLocalVariables[1] == -100000 + int(accumulated), "negative script timers advance consistently");
    Check(std::abs(countdown - int(std::fmax(0.0, 60000.0 - expected))) <= 1, "mission countdown preserves milliseconds");
    Check(std::abs(int(alarm) - countdown) <= 1, "unsigned alarm agrees with countdown");

    Reset(scale);
    float health = 1000.0f;
    for (int frame = 0; frame < fps * 5; ++frame) {
        Tick(1000.0 / fps);
        Drain(health, 226);
    }
    Check(std::fabs(health - (1000.0f - 300.0f * scale)) < 0.15f, "engine damage preserves 60 health per game second");
    float healthy = 1000.0f, low = 249.0f;
    Drain(healthy, 225); Drain(low, 226);
    Check(healthy == 1000.0f && low == 249.0f, "engine status and low-health gates remain intact");
    std::printf("PASS: %d FPS scale=%.2f jitter=%d accumulated=%u expected=%.3f health=%.3f\n", fps, scale, jitter, accumulated, expected, health);
}
int main() {
    for (int fps : {72, 20, 30, 36, 60, 80, 90, 120, 144}) {
        RunRate(fps, 1.0f, false);
        RunRate(fps, 0.5f, false);
        RunRate(fps, 1.0f, true);
    }
    Reset(); Tick(13.888888);
    CTimer::StartUserPause(); Tick(1000);
    Check(CTimer::GetTimeStepInMilliseconds() == 0, "user pause has no timer advance");
    float health = 500;
    Drain(health, 226);
    Check(health == 500, "paused engine loses no health");
    CTimer::EndUserPause(); Tick(13.888888);
    Check(CTimer::GetTimeStepInMilliseconds() <= 14, "resume does not catch up paused time");
    CTimer::SetCodePause(true); Tick(1000);
    Check(CTimer::GetTimeStepInMilliseconds() == 0, "code pause has no timer advance");
    CTimer::SetCodePause(false); Tick(500);
    Check(CTimer::GetTimeStepInMilliseconds() == 60, "stall retains the existing 60ms cap");
    health = 500; Drain(health, 226);
    Check(std::fabs(health - 496.4f) < 0.001f, "engine respects clipped stall time");
    Reset();
    Check(CTimer::GetTimeStepInMilliseconds() == 0, "initialization starts with no elapsed milliseconds");
    CTimer::SetTimeInMilliseconds(UINT32_MAX - 5); Tick(20);
    Check(CTimer::GetTimeStepInMilliseconds() == 20, "clock rollover preserves the frame delta");
    CTimer::SetTimeInMilliseconds(10000); Tick(20);
    CTimer::SetTimeInMilliseconds(1);
    Check(CTimer::GetTimeStepInMilliseconds() == 20, "load/replay clock replacement cannot underflow timers");
    std::printf("PASS: %u frame timer checks\n", checks);
}
