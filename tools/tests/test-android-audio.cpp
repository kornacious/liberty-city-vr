#include <algorithm>
#include <iterator>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <array>
#include <chrono>
#include "../../overlay/src/audio/QuestAudio.h"
#include "../../overlay/src/audio/QuestHrtf.h"
#include "../../overlay/src/audio/QuestAudioListener.h"
using uint8 = uint8_t;
using uint32 = uint32_t;
using uint64 = uint64_t;
using int32 = int32_t;
using bool8 = bool;
constexpr int MAXCHANNELS = 2, MAX2DCHANNELS = 1, CHANNEL2D = 2;
constexpr uint32 MAX_VOLUME = 127, DIGITALRATE = 48000, TOTAL_AUDIO_SAMPLES = 2;
constexpr int MUSICMODE_CUTSCENE = 1;
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#include "android-audio-types.inc"
AndroidChannel gChannels[MAXCHANNELS + MAX2DCHANNELS];
QuestHrtf::History gHrtfHistory[MAXCHANNELS + MAX2DCHANNELS];
std::mutex gMixerMutex;
int16_t raw[64];
uint8 *gRawSamples = reinterpret_cast<uint8 *>(raw);
size_t gRawSize = sizeof(raw);
int gOutputRate = DIGITALRATE;
struct tSample { int32 nOffset = 0; uint32 nSize = sizeof(raw), nFrequency = DIGITALRATE; };
tSample *gAndroidSampleTable;
struct { int mode = 0; int GetMusicMode() { return mode; } } MusicManager;
struct cSampleManager {
    tSample m_aSamples[TOTAL_AUDIO_SAMPLES];
    uint32 m_nEffectsFadeVolume = 127, m_nEffectsVolume = 127;
    bool IsSampleBankLoaded(uint8) { return true; }
    bool8 InitialiseChannel(uint32, uint32, uint8);
    void SetChannelEmittingVolume(uint32, uint32);
    void SetChannel3DPosition(uint32, float, float, float);
    void SetChannel3DDistances(uint32, float, float);
    void SetChannelVolume(uint32, uint32);
    void SetChannelPan(uint32, uint32);
    void SetChannelFrequency(uint32, uint32);
    void SetChannelLoopPoints(uint32, uint32, int32);
    void SetChannelLoopCount(uint32, uint32);
    void StartChannel(uint32);
} manager;
#include "android-audio-production.inc"
int checks = 0;
void Require(bool ok) { ++checks; if(!ok) { std::fprintf(stderr, "Failed audio assertion %d\n", checks); std::exit(1); } }
void Near(float a, float b) { Require(std::fabs(a - b) < 0.00001f); }
bool InitialiseLegacyChannel(uint32 channel, uint32 sample, uint8 bank) {
    const bool ok = manager.InitialiseChannel(channel, sample, bank);
    // Existing exact-PCM tests isolate attenuation/resampling from the HRIR.
    // Real positional rendering is exercised separately below.
    QuestAudio::SetChannel2D(channel, true);
    return ok;
}
int Mix(int channel = 0) {
    manager.StartChannel(uint32(channel));
    int16_t output[2] = {};
    Require(audioCallback(nullptr, output, 1) == 0);
    gChannels[channel].playing = false;
    return output[0];
}
using float32 = float;
using bool32 = bool;
struct {
    bool firstPersonActive = true, fpUseFullBasis = false;
    float fpAnchorYaw = 1.57079632679f;
    float fpPlayX[3] = {}, fpPlayY[3] = {}, fpPlayZ[3] = {};
    float headQuat[4] = {0, 0, 0, 1};
    float headPosition[3] = {}, fpLatchedHeadPos[3] = {};
    float fpHeadWorld[3] = {20, 30, 2};
} gvk;
#include "android-audio-view.inc"

using Stereo = std::array<int16_t, 512>;
Stereo RenderPosition(float left, float forward, float up, bool split = false) {
    for(auto &channel : gChannels) channel.playing = false;
    Require(manager.InitialiseChannel(0, 0, 0));
    manager.SetChannel3DPosition(0, left, forward, up);
    manager.SetChannel3DDistances(0, 100, 25);
    manager.SetChannelEmittingVolume(0, 127);
    manager.StartChannel(0);
    Stereo output{};
    if(split) {
        int offset = 0;
        for(int frames : {1, 7, 31, 64, 153}) {
            audioCallback(nullptr, output.data() + offset * 2, frames);
            offset += frames;
        }
    } else audioCallback(nullptr, output.data(), 256);
    Require(!gChannels[0].playing); // Includes the complete HRIR tail.
    return output;
}
double EarEnergy(const Stereo &pcm, int ear) {
    double energy = 0;
    for(size_t i = size_t(ear); i < pcm.size(); i += 2) energy += double(pcm[i]) * pcm[i];
    return energy;
}
int EarPeak(const Stereo &pcm, int ear) {
    int peak = 0;
    for(int frame = 1; frame < 256; ++frame)
        if(std::abs(int(pcm[size_t(frame * 2 + ear)])) > std::abs(int(pcm[size_t(peak * 2 + ear)]))) peak = frame;
    return peak;
}
void SpatialRegressions() {
    gOutputRate = 48000;
    MusicManager.mode = 0;
    manager.m_nEffectsVolume = manager.m_nEffectsFadeVolume = 127;
    manager.m_aSamples[0] = tSample{};
    manager.m_aSamples[0].nSize = 2; // One-frame impulse exposes the exact FIR.
    std::fill(std::begin(raw), std::end(raw), int16_t(0));
    raw[0] = 12000;
    const auto front = RenderPosition(0, 16, 0);
    const auto rear = RenderPosition(0, -16, 0);
    Require(front != rear && EarEnergy(front, 0) > 0 && EarEnergy(rear, 0) > 0);
    const auto distant = RenderPosition(0, 62.5f, 0);
    for(size_t i = 0; i < front.size(); ++i) Require(std::abs(int(front[i]) - 4 * int(distant[i])) <= 4);
    Require(front == RenderPosition(0, 16, 0, true));
    Require(rear == RenderPosition(0, -16, 0, true));
    const auto left = RenderPosition(16, 0, 0), right = RenderPosition(-16, 0, 0);
    Require(EarEnergy(left, 0) > 4 * EarEnergy(left, 1));
    Require(EarEnergy(right, 1) > 4 * EarEnergy(right, 0));
    Require(EarPeak(left, 0) < EarPeak(left, 1));
    Require(EarPeak(right, 1) < EarPeak(right, 0));
    for(int frame = 0; frame < 256; ++frame) {
        Require(left[size_t(frame * 2)] == right[size_t(frame * 2 + 1)]);
        Require(left[size_t(frame * 2 + 1)] == right[size_t(frame * 2)]);
    }
    Require(front != RenderPosition(0, 0, 16));
    Require(front != RenderPosition(0, 0, -16));
    // The tail must not be cut at the one-frame source boundary. Compare
    // against the actual selected FIR, including its interaural delay.
    QuestHrtf::Filter filter;
    QuestHrtf::Build(filter, 16, 0, 0, true);
    const float volume = float(gChannels[0].volume) / MAX_VOLUME;
    for(int frame = 0; frame < QuestHrtf::Taps; ++frame)
        for(int ear = 0; ear < 2; ++ear)
            Require(std::abs(int(left[size_t(frame * 2 + ear)]) -
                int(12000 * volume * filter.ear[ear][QuestHrtf::Taps - 1 - frame])) <= 1);
    // Reuse/restart cannot inherit a former voice's convolution history.
    Require(left == RenderPosition(16, 0, 0));
    Require(manager.InitialiseChannel(0, 0, 0));
    manager.SetChannel3DPosition(0, 16, 0, 0);
    manager.StartChannel(0);
    int16_t partial[16];
    audioCallback(nullptr, partial, 8); // Interrupt a live HRIR tail.
    Require(right == RenderPosition(-16, 0, 0));
    raw[0] = 0;
    manager.StartChannel(0);
    Stereo restarted{};
    audioCallback(nullptr, restarted.data(), 256);
    for(int16_t value : restarted) Require(value == 0);
    raw[0] = 12000;
    Require(manager.InitialiseChannel(0, 0, 0));
    QuestAudio::SetChannel2D(0, true);
    manager.SetChannel3DPosition(0, 1000, 0, 0);
    manager.SetChannel3DDistances(0, 100000, 25000);
    Require(!gChannels[0].filter.active);
    manager.StartChannel(0);
    Stereo dry{};
    audioCallback(nullptr, dry.data(), 256);
    Require(dry[0] == 0 && dry[1] > 11000);
    for(size_t i = 2; i < dry.size(); ++i) Require(dry[i] == 0);
    // All directions and elevation-ring seams must remain finite, including
    // the odd 45-position ring, rear seam, poles, and out-of-range elevation.
    for(int elevation = -90; elevation <= 90; elevation += 5)
        for(int azimuth = -180; azimuth <= 180; azimuth += 5) {
            const double e = elevation * 0.0174532925199433, a = azimuth * 0.0174532925199433;
            QuestHrtf::Build(filter, float(-std::sin(a)*std::cos(e)), float(std::cos(a)*std::cos(e)), float(std::sin(e)), true);
            Require(filter.active);
            for(const auto &ear : filter.ear) for(float coefficient : ear) Require(std::isfinite(coefficient));
        }
    // The actual renderer's quaternion + anchor composition feeds the listener.
    // Check stationary world sources as the headset rotates independently of
    // the game camera, with body yaw, room translation, roll and cockpit pitch.
    for(int body = 0; body < 360; body += 45) {
        gvk.fpAnchorYaw = float(body * 0.0174532925199433);
        for(int yaw = 0; yaw < 360; yaw += 15) {
            const double half = yaw * 0.00872664625997165;
            gvk.headQuat[0] = 0; gvk.headQuat[1] = float(std::sin(half));
            gvk.headQuat[2] = 0; gvk.headQuat[3] = float(std::cos(half));
            QuestAudio::Listener listener;
            Require(getFirstPersonViewFrame(listener.left, listener.up, listener.forward, listener.position));
            const double angle = (body + yaw) * 0.0174532925199433;
            Near(listener.forward[0], float(std::cos(angle)));
            Near(listener.forward[1], float(std::sin(angle)));
            // A source along the actual physical RIGHT axis must reach right.
            float world[3], local[3];
            for(int axis = 0; axis < 3; ++axis) world[axis] = listener.position[axis] - 16 * listener.left[axis];
            listener.Translate(world, local);
            Near(local[0], -16); Near(local[1], 0); Near(local[2], 0);
            const auto actual = RenderPosition(local[0], local[1], local[2]);
            Require(EarEnergy(actual, 1) > EarEnergy(actual, 0));
        }
    }
    gvk.fpUseFullBasis = true;
    gvk.fpPlayX[0] = 1; gvk.fpPlayX[1] = 0; gvk.fpPlayX[2] = 0;
    gvk.fpPlayY[0] = 0; gvk.fpPlayY[1] = 0.6f; gvk.fpPlayY[2] = 0.8f;
    gvk.fpPlayZ[0] = 0; gvk.fpPlayZ[1] = -0.8f; gvk.fpPlayZ[2] = 0.6f;
    gvk.headQuat[0] = 0; gvk.headQuat[1] = 0;
    gvk.headQuat[2] = 0.382683432f; gvk.headQuat[3] = 0.923879533f;
    gvk.headPosition[0] = 0.25f; gvk.headPosition[1] = 0.1f; gvk.headPosition[2] = -0.2f;
    QuestAudio::Listener cockpit;
    Require(getFirstPersonViewFrame(cockpit.left, cockpit.up, cockpit.forward, cockpit.position));
    Near(cockpit.position[0], 20.25f); Near(cockpit.position[1], 30.22f); Near(cockpit.position[2], 1.96f);
    float world[3], local[3];
    for(int axis = 0; axis < 3; ++axis) world[axis] = cockpit.position[axis] - 10 * cockpit.forward[axis];
    cockpit.Translate(world, local);
    Near(local[0], 0); Near(local[1], -10); Near(local[2], 0);
    // Moving filters converge without resetting playback or leaving NaNs.
    QuestHrtf::History history;
    QuestHrtf::Filter start, goal;
    QuestHrtf::Build(start, 16, 0, 0, true); QuestHrtf::Build(goal, -16, 0, 0, true);
    history.Reset(1, start);
    float l = 0, r = 0;
    for(int i = 0; i < 4800; ++i) { history.Process(i % 2 ? 1000.0f : -1000.0f, goal, l, r); Require(std::isfinite(l) && std::isfinite(r)); }
    for(int ear = 0; ear < 2; ++ear) for(int tap = 0; tap < QuestHrtf::Taps; ++tap) Near(history.current.ear[ear][tap], goal.ear[ear][tap]);
}
void BenchmarkSpatialAudio() {
    QuestHrtf::History histories[28];
    QuestHrtf::Filter filters[28];
    for(int voice = 0; voice < 28; ++voice) {
        const float angle = float(voice) * 0.224399475f;
        QuestHrtf::Build(filters[voice], std::sin(angle), std::cos(angle), 0, true);
        histories[voice].Reset(1, filters[voice]);
    }
    double total = 0, maximum = 0;
    volatile float checksum = 0;
    for(int block = 0; block < 100; ++block) {
        const auto start = std::chrono::steady_clock::now();
        for(int voice = 0; voice < 28; ++voice) {
            float l = 0, r = 0;
            for(int frame = 0; frame < 240; ++frame)
                histories[voice].Process(frame % 2 ? 1000.0f : -1000.0f, filters[voice], l, r);
            checksum = checksum + l + r;
        }
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        total += ms; maximum = std::max(maximum, ms);
    }
    std::printf("28-voice HRTF benchmark: %.3f ms mean, %.3f ms max per 5 ms of output (checksum %.3f).\n", total / 100, maximum, double(checksum));
}
int main(int argc, char **) {
    std::fill(std::begin(raw), std::end(raw), int16_t(10000));
    gAndroidSampleTable = manager.m_aSamples;
    Require(InitialiseLegacyChannel(0, 0, 0));
    manager.SetChannelEmittingVolume(0, 127);
    const uint32 emitting = gChannels[0].volume;
    const int nearOutput = Mix();
    Require(nearOutput > 9000);
    manager.SetChannel3DDistances(0, 100, 20);
    manager.SetChannel3DPosition(0, 0, 20, 0);
    Near(gChannels[0].distanceGain, 1);
    Require(Mix() == nearOutput);
    manager.SetChannel3DPosition(0, 0, 60, 0);
    Near(gChannels[0].distanceGain, 0.25f);
    Require(std::abs(Mix() * 4 - nearOutput) <= 4);
    manager.SetChannel3DPosition(0, 0, 0, 60);
    Near(gChannels[0].distanceGain, 0.25f);
    manager.SetChannel3DPosition(0, 36, 0, 48);
    Near(gChannels[0].distanceGain, 0.25f);
    for(float distance : {100.0f, 101.0f, 10000.0f}) {
        manager.SetChannel3DPosition(0, 0, distance, 0);
        Near(gChannels[0].distanceGain, 0);
        Require(Mix() == 0);
    }
    // Motion and range updates must recover full gain without a volume update.
    manager.SetChannel3DPosition(0, 0, 20, 0);
    Require(Mix() == nearOutput && gChannels[0].volume == emitting);
    manager.SetChannel3DPosition(0, 0, 60, 0);
    manager.SetChannel3DDistances(0, 200, 40);
    Near(gChannels[0].distanceGain, 0.765625f);
    manager.SetChannel3DDistances(0, 100, 20);
    Near(gChannels[0].distanceGain, 0.25f);
    // Setter ordering on a fresh/reused channel produces the same final gain.
    Require(InitialiseLegacyChannel(1, 0, 0));
    manager.SetChannel3DDistances(1, 100, 20);
    manager.SetChannel3DPosition(1, 0, 60, 0);
    Near(gChannels[0].distanceGain, gChannels[1].distanceGain);
    float previous = 1;
    for(int i = 0; i <= 120; ++i) {
        manager.SetChannel3DPosition(0, 0, float(i), 0);
        Require(gChannels[0].distanceGain <= previous);
        previous = gChannels[0].distanceGain;
    }
    // Inaudible one-shots still advance and finish; spatial updates must not
    // restart playback or overwrite its cursor.
    manager.StartChannel(0);
    int16_t silent[128];
    std::fill(std::begin(silent), std::end(silent), int16_t(123));
    audioCallback(nullptr, silent, 8);
    Require(gChannels[0].cursor == 8 && gChannels[0].playing);
    for(int i = 0; i < 16; ++i) Require(silent[i] == 0);
    manager.SetChannel3DPosition(0, 0, 0, 0);
    Require(gChannels[0].cursor == 8);
    manager.SetChannel3DPosition(0, 0, 120, 0);
    audioCallback(nullptr, silent, 64);
    Require(!gChannels[0].playing && gChannels[0].cursor == 64);
    // Effects/fade scaling and cutscene ducking still apply exactly once.
    manager.SetChannel3DPosition(0, 0, 0, 0);
    manager.m_nEffectsVolume = 64;
    manager.SetChannelEmittingVolume(0, 127);
    const int half = Mix(); Require(half > 0 && half < nearOutput);
    manager.SetChannel3DPosition(0, 0, 60, 0);
    Require(std::abs(Mix() * 4 - half) <= 4);
    MusicManager.mode = MUSICMODE_CUTSCENE;
    manager.SetChannelEmittingVolume(0, 127);
    Require(gChannels[0].volume < emitting / 4);
    manager.m_nEffectsFadeVolume = 0;
    manager.SetChannelEmittingVolume(0, 127);
    Require(Mix() == 0);
    // Engine 2D sounds on 3D channels use a deliberately huge near radius.
    manager.SetChannel3DPosition(0, 1000, 0, 0);
    manager.SetChannel3DDistances(0, 100000, 25000);
    Near(gChannels[0].distanceGain, 1);
    Require(InitialiseLegacyChannel(CHANNEL2D, 0, 0));
    manager.m_nEffectsFadeVolume = manager.m_nEffectsVolume = 127;
    manager.SetChannelVolume(CHANNEL2D, 127);
    manager.SetChannelPan(CHANNEL2D, 63);
    manager.SetChannel3DPosition(CHANNEL2D, 1000, 0, 0);
    manager.SetChannel3DDistances(CHANNEL2D, 1, 0);
    Require(Mix(CHANNEL2D) == nearOutput);
    // Reinitialisation clears previous position/range/gain on channel reuse.
    Require(InitialiseLegacyChannel(0, 0, 0));
    Near(gChannels[0].distanceGain, 1); Near(gChannels[0].y, 0);
    for(float bad : {0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        manager.SetChannel3DDistances(0, bad, 0);
        Near(gChannels[0].distanceGain, 0);
    }
    manager.SetChannel3DDistances(0, 100, 100);
    Near(gChannels[0].distanceGain, 1);
    manager.SetChannel3DPosition(0, 0, 100, 0);
    Near(gChannels[0].distanceGain, 0);
    manager.SetChannel3DDistances(0, 100, 200);
    Near(gChannels[0].distanceGain, 0);
    manager.SetChannel3DPosition(0, 0, 99, 0);
    Near(gChannels[0].distanceGain, 1);
    manager.SetChannel3DDistances(0, 100, -20);
    Require(std::isfinite(gChannels[0].distanceGain));
    manager.SetChannel3DPosition(0, std::numeric_limits<float>::quiet_NaN(), 0, 0);
    Near(gChannels[0].distanceGain, 0); Require(gChannels[0].pan == 63);
    manager.SetChannel3DPosition(0, 0, std::numeric_limits<float>::max(), 0);
    Near(gChannels[0].distanceGain, 0);
    // A ramp makes fractional resampling measurable, unlike constant-volume
    // fixtures. Include the 6 kHz frontend rate found in the retail SDT, pitch
    // overrides used by weapon pickups, and both common device output rates.
    gChannels[0].playing = gChannels[1].playing = false;
    for(int i = 0; i < 64; ++i) raw[i] = int16_t(i * 400);
    for(int outputRate : {44100, 48000}) {
        gOutputRate = outputRate;
        for(uint32 sourceRate : {6000u, 11025u, 22050u, 32000u, 44100u, 48000u}) {
            manager.m_aSamples[0].nFrequency = sourceRate;
            for(uint32 playbackRate : {sourceRate, 32000u, 48000u}) {
                Require(InitialiseLegacyChannel(CHANNEL2D, 0, 0));
                manager.SetChannelFrequency(CHANNEL2D, playbackRate);
                manager.StartChannel(CHANNEL2D);
                int frames = 0;
                while(gChannels[CHANNEL2D].playing && frames < 600) {
                    int16_t output[2];
                    audioCallback(nullptr, output, 1);
                    const double position = double(frames) * playbackRate / outputRate;
                    if(position < 64.0 - 1e-8) {
                        const double value = std::min(position, 63.0) * 400;
                        const int expected = int(value);
                        Require(std::abs(int(output[0]) - expected) <= 2);
                        Require(output[0] == output[1]);
                    }
                    ++frames;
                }
                // One additional callback observes completion after the final
                // source frame; floating point rounding can add one frame.
                const double duration = 64.0 * outputRate / playbackRate;
                Require(std::abs(double(frames - 1) - std::ceil(duration)) <= 1);
            }
        }
    }
    // Interpolation at the loop seam must use the loop start, not the tail.
    gOutputRate = 48000;
    Require(InitialiseLegacyChannel(CHANNEL2D, 0, 0));
    manager.SetChannelFrequency(CHANNEL2D, 6000);
    manager.SetChannelLoopPoints(CHANNEL2D, 4, 8); // source frames [2,4)
    manager.SetChannelLoopCount(CHANNEL2D, 0);
    manager.StartChannel(CHANNEL2D);
    gChannels[CHANNEL2D].cursor = 3.5;
    int16_t seam[2];
    audioCallback(nullptr, seam, 1);
    Require(std::abs(int(seam[0]) - 1000) <= 1); // halfway from 1200 to 800
    Require(gChannels[CHANNEL2D].cursor == 3.625);
    // A one-shot holds its final value instead of reading past the buffer.
    manager.SetChannelLoopCount(CHANNEL2D, 1);
    gChannels[CHANNEL2D].cursor = 3.5;
    audioCallback(nullptr, seam, 1);
    Require(std::abs(int(seam[0]) - 1200) <= 1);
    // Frontend stereo pairs go through the spatial channels with an enormous
    // attenuation radius. Opposite-polarity halves must remain separate rather
    // than cancelling in the centre. +X is right in the PC OpenAL listener.
    gChannels[CHANNEL2D].playing = false;
    std::fill(std::begin(raw), std::begin(raw) + 32, int16_t(10000));
    std::fill(std::begin(raw) + 32, std::end(raw), int16_t(-10000));
    manager.m_aSamples[0].nSize = 64;
    manager.m_aSamples[1].nOffset = 64;
    manager.m_aSamples[1].nSize = 64;
    for(int i = 0; i < 2; ++i) {
        Require(InitialiseLegacyChannel(uint32(i), uint32(i), 0));
        manager.SetChannel3DPosition(uint32(i), i == 0 ? -1000.0f : 1000.0f, 0, 0);
        manager.SetChannel3DDistances(uint32(i), 100000, 25000);
        manager.StartChannel(uint32(i));
    }
    int16_t stereo[2];
    audioCallback(nullptr, stereo, 1);
    Require(stereo[0] > 9800 && stereo[1] == -10000);
    // Changing audible range cannot change the bearing or stereo separation.
    const uint32 rightPan = gChannels[1].pan;
    manager.SetChannel3DDistances(1, 200000, 50000);
    Require(gChannels[1].pan == rightPan);
    manager.SetChannel3DPosition(1, 0, 1000, 0);
    Require(gChannels[1].pan == 63);
    manager.SetChannel3DPosition(1, 0, 0, 1000);
    Require(gChannels[1].pan == 63);
    // Two loud voices overload the partial sum. The third cancels one exactly.
    std::fill_n(raw, 32, int16_t(28000));
    std::fill_n(raw + 32, 32, int16_t(-28000));
    for(int channel = 0; channel <= CHANNEL2D; ++channel) {
        Require(InitialiseLegacyChannel(channel, channel == CHANNEL2D ? 1 : 0, 0));
        manager.SetChannelLoopCount(channel, 0);
        manager.StartChannel(channel);
    }
    std::array<int16_t, 2050> crowded;
    audioCallback(nullptr, crowded.data(), 1025);
    for(int16_t sample : crowded) Require(sample == 28000);
    for(int16_t peak : {28000, -28000}) {
        std::fill(std::begin(raw), std::end(raw), peak);
        audioCallback(nullptr, stereo, 1);
        Require(stereo[0] == (peak > 0 ? 32767 : -32768));
        Require(stereo[1] == stereo[0]);
    }
    SpatialRegressions();
    std::printf("Android audio: %d assertions passed against production headset frame, listener, HRTF, setters and effects mixer.\n", checks);
    if(argc > 1) BenchmarkSpatialAudio();
    return 0;
}
