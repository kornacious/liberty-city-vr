#pragma once
#include <cstdint>
namespace NativeAudioOutput {
using Callback = int32_t (*)(void *user, void *stereoInt16, int32_t frames);
// Both adapters deliver interleaved 48 kHz stereo PCM to the existing HRTF mixer.
bool Open(Callback callback, void *user);
void Close();
}
