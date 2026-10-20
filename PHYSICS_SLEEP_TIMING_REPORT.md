# Physics settling and sleep timing repair

Date: 2026-10-20. Patch: `13-re3-quest-physics-sleep-timing.patch`.

## Cause

Objects and non-ped-physics pedestrians averaged velocities with a fixed 0.5
response on every update, compared them against timestep-scaled speed limits,
and slept after more than ten quiet updates. Abandoned/wrecked cars used the
same averaging/counting pattern with separate speed/angular limits and a
per-update displacement gate. Higher FPS shortened the quiet delay but also
tightened the velocity limits; these effects could compete rather than always
making sleep earlier.

Eleven rendered updates take approximately 367 ms at 30 FPS and 153 ms at
72 FPS. The car displacement came from the previous physics pass, so testing
it against a threshold derived from the current frame would still produce
inconsistent decisions with uneven pacing.

## Repair

`QuestPhysicsSleep.h` centralizes the Android/FIX_BUGS policy:

- Average velocity with `1 - pow(0.5, gameSeconds * 30)`, reproducing the
  original half-response at 30 FPS and equal-time decay for a constant input.
- Use the original 30 FPS velocity thresholds (`legacyLimit * 50 / 30`).
- Count quiet 30 Hz gameplay ticks, respecting pause, slow motion and clipping.
  Eleven ticks qualify. Saturate at eleven and retain that ready value between
  ticks, so settled vehicles do not restart physics on intervening frames.
- Preserve external `m_nStaticFrames = 0` wake resets, existing eligibility,
  moving-map restrictions and the stationary-contact shortcut. The saturated
  counter cannot wrap while a moving map prevents sleep.
- Avoid advancing the average/count twice when the world retries postponed
  vehicle control with `bForceProcessControl` in the same frame.

Displacement is converted to a 30 FPS equivalent when each producer writes
`m_fDistanceTravelled`, using the timestep that produced it. Every reset also
clears that cache. Car sleep checks use the cache; the original distance value
and its other consumers remain intact. The cache aliases an existing unused
float, so physical-object size and save layout do not change. Compatible
vehicle serialization skips this unused region; constructors initialize it.

Desktop retains per-call counting and timestep-dependent thresholds through
the helper's fallback. The Android policy keeps the original threshold values
at the 30 FPS reference rather than retuning settling behavior.

## Verification and limits

The new production-function regression fails against the pre-repair source on
game-time sleep counting. All 37 supported suites pass, and the final focused
suite passes 7,255 checks. Coverage includes 15–144 FPS, steady/uneven pacing,
half-speed time, pause, catch-up, the 30 FPS reference, linear/angular limits,
equal-time averaging, previous/current timestep mismatches, external wake
resets, moving-map gating, counter saturation, contact shortcuts and postponed
control retries. Source checks verify the displacement producers and resets.

The quiet counter is quantized to the existing global 30 Hz gameplay clock.
For a quiet interval starting on a tick boundary, eleven ticks correspond to
about 367 ms of game time. Entry phase can shift that by up to one reference
tick; a rendered frame observes the result after crossing the relevant tick.
Clipped stalls still discard time under the existing native timer policy.

These tests hold or prescribe velocities and displacement; they do not model
full collision stacks, suspension feedback or moving-platform gameplay.
Actual settling appearance and performance still need headset verification.

Local results: `.cache/physics-sleep-verified/results.json`, focused final
checks under `.cache/physics-sleep-retry-check/`, and the final APK build log
at `.cache/physics-sleep-verified/apk-build-final.log`. No original game data
was needed for implementation or automated validation.
