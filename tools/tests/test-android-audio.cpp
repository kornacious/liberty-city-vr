#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
using uint8 = uint8_t;
using uint32 = uint32_t;
using uint64 = uint64_t;
using int32 = int32_t;
using bool8 = bool;
constexpr int MAXCHANNELS = 2, MAX2DCHANNELS = 1, CHANNEL2D = 2;
constexpr uint32 MAX_VOLUME = 127, DIGITALRATE = 48000, TOTAL_AUDIO_SAMPLES = 2;
constexpr int MUSICMODE_CUTSCENE = 1, AAUDIO_CALLBACK_RESULT_CONTINUE = 0;
using aaudio_data_callback_result_t = int;
struct AAudioStream {};
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#include "android-audio-types.inc"
AndroidChannel gChannels[MAXCHANNELS + MAX2DCHANNELS];
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
int Mix(int channel = 0) {
    manager.StartChannel(uint32(channel));
    int16_t output[2] = {};
    Require(audioCallback(nullptr, nullptr, output, 1) == AAUDIO_CALLBACK_RESULT_CONTINUE);
    gChannels[channel].playing = false;
    return output[0];
}
int main() {
    std::fill(std::begin(raw), std::end(raw), int16_t(10000));
    gAndroidSampleTable = manager.m_aSamples;
    Require(manager.InitialiseChannel(0, 0, 0));
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
    Require(manager.InitialiseChannel(1, 0, 0));
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
    audioCallback(nullptr, nullptr, silent, 8);
    Require(gChannels[0].cursor == 8 && gChannels[0].playing);
    for(int i = 0; i < 16; ++i) Require(silent[i] == 0);
    manager.SetChannel3DPosition(0, 0, 0, 0);
    Require(gChannels[0].cursor == 8);
    manager.SetChannel3DPosition(0, 0, 120, 0);
    audioCallback(nullptr, nullptr, silent, 64);
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
    Require(manager.InitialiseChannel(CHANNEL2D, 0, 0));
    manager.m_nEffectsFadeVolume = manager.m_nEffectsVolume = 127;
    manager.SetChannelVolume(CHANNEL2D, 127);
    manager.SetChannelPan(CHANNEL2D, 63);
    manager.SetChannel3DPosition(CHANNEL2D, 1000, 0, 0);
    manager.SetChannel3DDistances(CHANNEL2D, 1, 0);
    Require(Mix(CHANNEL2D) == nearOutput);
    // Reinitialisation clears previous position/range/gain on channel reuse.
    Require(manager.InitialiseChannel(0, 0, 0));
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
                Require(manager.InitialiseChannel(CHANNEL2D, 0, 0));
                manager.SetChannelFrequency(CHANNEL2D, playbackRate);
                manager.StartChannel(CHANNEL2D);
                int frames = 0;
                while(gChannels[CHANNEL2D].playing && frames < 600) {
                    int16_t output[2];
                    audioCallback(nullptr, nullptr, output, 1);
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
    Require(manager.InitialiseChannel(CHANNEL2D, 0, 0));
    manager.SetChannelFrequency(CHANNEL2D, 6000);
    manager.SetChannelLoopPoints(CHANNEL2D, 4, 8); // source frames [2,4)
    manager.SetChannelLoopCount(CHANNEL2D, 0);
    manager.StartChannel(CHANNEL2D);
    gChannels[CHANNEL2D].cursor = 3.5;
    int16_t seam[2];
    audioCallback(nullptr, nullptr, seam, 1);
    Require(std::abs(int(seam[0]) - 1000) <= 1); // halfway from 1200 to 800
    Require(gChannels[CHANNEL2D].cursor == 3.625);
    // A one-shot holds its final value instead of reading past the buffer.
    manager.SetChannelLoopCount(CHANNEL2D, 1);
    gChannels[CHANNEL2D].cursor = 3.5;
    audioCallback(nullptr, nullptr, seam, 1);
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
        Require(manager.InitialiseChannel(uint32(i), uint32(i), 0));
        manager.SetChannel3DPosition(uint32(i), i == 0 ? -1000.0f : 1000.0f, 0, 0);
        manager.SetChannel3DDistances(uint32(i), 100000, 25000);
        manager.StartChannel(uint32(i));
    }
    int16_t stereo[2];
    audioCallback(nullptr, nullptr, stereo, 1);
    Require(stereo[0] > 9800 && stereo[1] == -10000);
    // Changing audible range cannot change the bearing or stereo separation.
    const uint32 rightPan = gChannels[1].pan;
    manager.SetChannel3DDistances(1, 200000, 50000);
    Require(gChannels[1].pan == rightPan);
    manager.SetChannel3DPosition(1, 0, 1000, 0);
    Require(gChannels[1].pan == 63);
    manager.SetChannel3DPosition(1, 0, 0, 1000);
    Require(gChannels[1].pan == 63);
    std::printf("Android audio: %d assertions passed against production setters and AAudio callback.\n", checks);
}
