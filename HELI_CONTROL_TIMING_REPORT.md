# Helicopter control timing repair

Date: 2026-10-20. Patch: `14-re3-quest-heli-control-timing.patch`.

## Findings

The avoidance/shot-down controller used `(renderFrame + seed) & 8` to select
alternating eight-frame windows. A complete cycle lasted about 533 ms at
30 FPS but 222 ms at 72 FPS. Side-obstacle responses could flip direction and
change target height by five units every rendered update in the checked
window. Normal-state target-offset recovery also subtracted two units per
rendered update: recovering an offset of 17 to its legacy remainder of 1 took
eight updates (267 ms at 30 FPS, 111 ms at 72 FPS).

Catalina's path-state-6 landing approach separately blended its horizontal
position 1% toward a fixed target per update. Reaching halfway took 69 updates,
or about 2.30 seconds at 30 FPS versus 0.96 seconds at 72 FPS.

## Implementation

`ProcessAvoidance()` now runs Android/FIX_BUGS decisions on crossed ticks of
the scaled, clipped, pause-aware 30 Hz gameplay clock. Each crossed tick uses
its own tick/seed phase. The side test, direction flip, five-unit offset
response, upper offset cap and shot-down forward-contact checks retain the
original 30 FPS rules. Already-scaled shot-down descent is evaluated with the
reference timestep on those ticks, preserving its clear-path rate of 2.5
offset units per game second.

The latest signed lateral speed is cached independently on each helicopter
and held between decision ticks. The current target direction and normal
timestep-scaled flight integration still update every rendered frame. Leaving
avoidance clears that cached speed. Normal offset recovery uses two units per
gameplay tick, preserving the legacy sub-two-unit terminal remainder and
avoiding overshoot during catch-up.

Landing blending uses `1 - pow(0.99, gameSeconds * 30)`. At 30 FPS this gives
the original 0.01 response; over equal game time it produces the same
constant-target exponential at other rates. Scripted altitude handling stays
in its existing path. Desktop keeps the original avoidance body and landing
blend through the fallback branches.

The cache adds one transient float to Android helicopter instances and is
initialized in the constructor. Vehicle save serialization stores cars and
boats, not helicopters; save formats are unchanged.

## Validation and limits

The regression fails against pre-repair production code on offset timing.
All 38 supported current-source suites pass. The final focused fixture passes
997,367 checks covering 15–144 FPS, steady/uneven pacing, half-speed time,
the native 60 ms clip, multiple seeds, hover/recovery/shot-down states,
clear/right-blocked/both-blocked scenes, forward contact, pause, held lateral
decisions, crossed decision windows and independent helicopter state.
An independent legacy tick controller supplies expected offset, direction,
query counts and explosion-timer behavior; an analytic exponential supplies
the landing reference. The arm64 debug APK builds successfully.

Decisions are sampled at 30 Hz and observed on rendered frames. Catch-up uses
current geometry/position, not reconstructed historical collisions. These
tests prescribe collision answers and isolate control logic; they do not
establish FPS-independent full flight trajectories or mission completion.
Headset verification of hovering near buildings, obstacle avoidance, shot-down
collisions and Catalina's approach is still needed. Rotor-dust render work and
the existing timestamp-based shooting/bomb schedules are outside this repair.

Local artifacts: `.cache/heli-control-verified/results.json`,
`.cache/heli-control-verified/apk-build.log`, and the final focused run under
`.cache/heli-control-final-focused/`. No original game data was required for
implementation or automated validation.
