# Positional audio on Quest and PCVR

Quest uses AAudio for effects output. Windows and Linux PCVR use SDL3 with
the shared spatial mixer. Effects accumulate in a 32-bit buffer before the
final stereo PCM output is clipped. The x64 filter uses SIMD for crowded scenes.

World mono effects use a listener captured at the start of
`cAudioManager::Service`, after game simulation and before audio source
processing. The first-person anchor is refreshed without replacing the
simulation camera. Position and orientation come from the same predicted
headset frame and world basis used by the renderer, including room movement,
vehicle pitch/roll, and immersive cutscenes. Theater and third-person views
fall back to the game view. Distance and occlusion queries use the same
listener position.

The game-facing listener coordinate convention is **X left, Y forward, Z up**.
The measured HRTFs use clockwise azimuth toward the right ear; the conversion
is `atan2(-x, y)`. Do not infer handedness from `CCamera::GetRight` or negate
the axes a second time.

`QuestAudio::SetChannel2D` explicitly identifies synthetic frontend stereo
positions. These retain their existing pan and bypass the HRTF. Police radio
and other dedicated 2D channels also bypass it. Music and long platform-decoded
streams retain their existing stereo routing. This change spatializes world
SFX, not inherently stereo recordings or streamed mission dialogue.

The AAudio output is explicitly 48 kHz. Source resampling still respects the
effect's original playback frequency. Each world voice convolves with 144-tap
left/right HRIRs, preserving the measured timing and frequency differences.
Filters interpolate in azimuth and elevation, mirror ears for the other
hemisphere, and smooth changes over about 5 ms. Elevations below the measured
-40-degree limit use the lowest measured ring. The callback retains history
between bursts, clears it on restart/reuse, and drains the FIR tail before
finishing a one-shot. ARM64 uses NEON for the convolution and coefficient
smoothing. The callback performs no allocation, file access, or table generation.

## Measurement attribution and permission

The embedded measurements are **Copyright 1994 MIT Media Laboratory**.
Authors: **Bill Gardner and Keith Martin**, MIT Media Lab.
Research citation: *HRTF Measurements of a KEMAR Dummy-Head Microphone*,
MIT Media Lab Perceptual Computing Technical Report #280, May 1994.

MIT supplies these measurements without use restrictions, subject to citing
the authors in research and commercial applications. This attribution applies
to `src/audio/QuestHrtfData.inc`; it does not change the code license.

Original source and permission:
<https://sound.media.mit.edu/resources/KEMAR.html>

Input archive:
<https://sound.media.mit.edu/resources/KEMAR/diffuse.zip>

SHA256: `a48e73137390626ef1e45d1de6a4d98530c6aa570e71b40f7c4442241ed5fa22`

The 368 stereo, 128-frame, diffuse-field equalized responses at 44.1 kHz are
resampled to 48 kHz with a 32-point Hann-windowed sinc kernel, preserving FIR
DC gain. They are padded to 144 taps and quantized to signed 16-bit coefficients.
No individual-ear normalization is applied. The other hemisphere is mirrored
at runtime, including the odd 45-position elevation ring. No original game
data is involved in generation or distribution of this table.

To reproduce the table (Python 3 and numpy):

```text
dotnet run --project tools/manager -c Release -- --tool tools/generate-quest-hrtf.py -- PATH_TO_LOCAL_DIFFUSE_ZIP
```

The generator validates the archive checksum and never downloads inputs.
The generated table is committed, so ordinary APK builds need neither numpy
nor network access for HRTF preparation.

## Verification

`tools/tests/test-android-audio.ps1` exercises the actual production Android
setters, callback, headset-frame composition, listener transform, and HRTF.
It covers exact original 2D/resampling/attenuation behavior, front/rear
separation, left/right energy and arrival ordering, elevation, callback
partitioning, impulse-response tails, restart/reuse, coefficient smoothing,
azimuth/elevation seams, independent head/body yaw, room movement and a tilted
cockpit basis. The APK build compiles the ARM64 NEON path separately.

Perceived localization depends on the listener and playback hardware; these
generic measured HRTFs are not personalized. Headset listening remains a
separate check. Compare a stationary source ahead and behind while physically
turning the head, then repeat on foot, inside a moving vehicle, and during a
cutscene. Check that menu sounds and music retain their stereo balance, and
check busy scenes for audio breakup or frame-time regression. Headphones give
the clearest isolation between the rendered ear signals.
