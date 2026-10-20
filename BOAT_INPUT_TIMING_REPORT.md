# Boat input timing repair

Date: 2026-10-20. Patch: `12-re3-quest-boat-input-timing.patch`.

## Cause and behavior

Boat throttle and brake filters used a fixed 0.1 response per rendered update;
steering used 0.2. The same held input therefore converged faster at higher
FPS. The old throttle/brake smoothing required 22 updates to reach 90% of a
constant full target (733 ms at 30 FPS, 306 ms at 72 FPS). Steering required
11 updates (367 ms at 30 FPS, 153 ms at 72 FPS).

The brake filter also resets values below 0.05 to zero and selects either
forward acceleration or reverse thrust. Merely changing its 0.1 multiplier to
an elapsed-time exponential would break this recurrence: a full brake input
starting at zero produces less than 0.05 per update at 72 FPS and above, so the
existing reset could prevent braking from engaging at all.

## Implementation

Throttle and brake filtering now run once for each crossed 30 Hz gameplay tick,
using `QuestGameTime::Count()`. That clock already follows scaled, clipped,
pause-aware game time. The original brake threshold, throttle clamp, reverse
thrust factor and order of operations remain intact. Missed ticks are processed
in order with the current input sample. Multiple boat instances each process
the same tick count; none consumes a shared counter.

Steering uses `1 - pow(0.8, gameSeconds * 30)` on Android with `FIX_BUGS`.
This reproduces the original 0.2 response at 30 FPS and the same exponential
response over equal game time at other rates. Both immersive helm steering and
the pad fallback use it. The signed squared steering curve and handling-defined
steering lock are preserved. Desktop uses its existing 0.2 response and one
pedal update per call through the helper's existing fallback.

Throttle/brake outputs deliberately hold between 30 Hz ticks. Their update
phase has a period of about 33 ms, and threshold observations remain quantized
to the frame that crosses a tick. Steering updates smoothly every frame.
Catch-up uses the latest available input; it cannot reconstruct input changes
during a stall. No new boat fields, save layout, handling settings, hull forces
or water resistance calculations are introduced.

## Validation

`tools/tests/test-boat-input-timing.ps1` extracts the production input function
and gameplay clock helper. An independent legacy recurrence supplies the pedal
reference, and an analytic exponential supplies the steering reference.

The regression fails against the pre-repair source on pedal timing. After the
repair, 232,172 checks pass, covering:

- Original 30 Hz response and alternating immersive/pad steering sources.
- 15, 18, 20, 30, 36, 60, 72, 80, 90, 120 and 144 FPS.
- Constant and uneven pacing, half-speed time and the native 60 ms clip.
- Full braking, weak-brake deadzone, reverse thrust and release transitions.
- Steering reversals, distinct helm/pad targets, and smooth between-tick steering.
- Paused controls from rest and from an active boat, two-tick catch-up,
  finite/bounded outputs and the existing steering-lock curve.

Local artifacts are under `.cache/boat-input-verified/` and
`.cache/boat-input-final-focused/`. These are isolated control-filter checks;
full hull dynamics and headset feel still require gameplay verification.
