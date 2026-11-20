#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#if defined(__aarch64__)
#include <arm_neon.h>
#elif defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#endif

// Measured, diffuse-field equalized MIT KEMAR HRIRs. Positive azimuth is
// clockwise toward the RIGHT ear; game listener X points LEFT.
namespace QuestHrtf {
constexpr int Taps = 144;
constexpr int HistorySize = 256;
struct Ring { int count, offset; };
#include "QuestHrtfData.inc"

struct Filter {
    float ear[2][Taps] = {};
    bool active = false;
};

inline float Coefficient(const Ring &ring, int azimuth, int ear, int tap)
{
    azimuth %= ring.count;
    if(azimuth > ring.count / 2){
        azimuth = ring.count - azimuth;
        ear = 1 - ear;
    }
    return float(Responses[ring.offset + azimuth][ear][tap]) / 32768.0f;
}

inline void Build(Filter &filter, float left, float forward, float up, bool spatial)
{
    const double horizontal = std::hypot(double(left), double(forward));
    filter.active = spatial && std::isfinite(horizontal) && std::isfinite(up) &&
        std::hypot(horizontal, double(up)) > 0.01;
    if(!filter.active) return;
    constexpr double Degrees = 57.29577951308232;
    double azimuth = std::atan2(-double(left), double(forward)) * Degrees;
    if(azimuth < 0) azimuth += 360;
    const double elevation = std::max(0.0, std::min(13.0,
        (std::atan2(double(up), horizontal) * Degrees + 40.0) / 10.0));
    const int low = int(elevation), high = std::min(13, low + 1);
    const float vertical = float(elevation - low);
    for(int ear = 0; ear < 2; ++ear){
        for(int tap = 0; tap < Taps; ++tap){
            float values[2];
            int row = 0;
            for(int ringIndex : {low, high}){
                const Ring &ring = Rings[ringIndex];
                const double bearing = azimuth * ring.count / 360.0;
                const int first = int(bearing);
                const float fraction = float(bearing - first);
                const float a = Coefficient(ring, first, ear, tap);
                const float b = Coefficient(ring, first + 1, ear, tap);
                values[row++] = a + (b - a) * fraction;
            }
            // Reverse the FIR to convolve contiguous, aligned history with SIMD.
            filter.ear[ear][Taps - 1 - tap] = values[0] + (values[1] - values[0]) * vertical;
        }
    }
}

struct History {
    float samples[HistorySize * 2] = {};
    Filter current;
    uint32_t revision = ~uint32_t(0);
    int cursor = 0;

    void Reset(uint32_t nextRevision, const Filter &filter)
    {
        memset(samples, 0, sizeof(samples));
        current = filter;
        revision = nextRevision;
        cursor = 0;
    }

    void Process(float sample, const Filter &target, float &left, float &right)
    {
        samples[cursor] = samples[cursor + HistorySize] = sample;
        const float *input = samples + cursor + HistorySize - Taps + 1;
        float output[2] = {};
        for(int ear = 0; ear < 2; ++ear){
            // Smooth moving filters over approximately 5 ms. This preserves
            // playback history across bearing changes without zipper clicks.
#if defined(__aarch64__)
            float32x4_t sum = vdupq_n_f32(0);
            for(int tap = 0; tap < Taps; tap += 4){
                float32x4_t coeff = vld1q_f32(current.ear[ear] + tap);
                const float32x4_t goal = vld1q_f32(target.ear[ear] + tap);
                coeff = vmlaq_n_f32(coeff, vsubq_f32(goal, coeff), 1.0f / 240.0f);
                vst1q_f32(current.ear[ear] + tap, coeff);
                sum = vmlaq_f32(sum, coeff, vld1q_f32(input + tap));
            }
            output[ear] = vaddvq_f32(sum);
#elif defined(__SSE2__) || defined(_M_X64)
            __m128 sum = _mm_setzero_ps();
            const __m128 smoothing = _mm_set1_ps(1.0f / 240.0f);
            for(int tap = 0; tap < Taps; tap += 4){
                __m128 coeff = _mm_loadu_ps(current.ear[ear] + tap);
                const __m128 goal = _mm_loadu_ps(target.ear[ear] + tap);
                coeff = _mm_add_ps(coeff, _mm_mul_ps(_mm_sub_ps(goal, coeff), smoothing));
                _mm_storeu_ps(current.ear[ear] + tap, coeff);
                sum = _mm_add_ps(sum, _mm_mul_ps(coeff, _mm_loadu_ps(input + tap)));
            }
            const __m128 pairs = _mm_add_ps(sum, _mm_movehl_ps(sum, sum));
            output[ear] = _mm_cvtss_f32(_mm_add_ss(pairs, _mm_shuffle_ps(pairs, pairs, 0x55)));
#else
            for(int tap = 0; tap < Taps; ++tap){
                float &coeff = current.ear[ear][tap];
                coeff += (target.ear[ear][tap] - coeff) / 240.0f;
                output[ear] += coeff * input[tap];
            }
#endif
        }
        cursor = (cursor + 1) % HistorySize;
        left = output[0];
        right = output[1];
    }
};
}
