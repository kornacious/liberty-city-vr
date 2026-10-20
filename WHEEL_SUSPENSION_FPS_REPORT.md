# Wheel and suspension frame-rate characterization

Date: 2026-10-20. Runtime baseline: `0e81e2c`, including the previous timer,
ragdoll-update, angular-drag and boat-lift repairs. No production force
corrections, handling parameters or runtime patches were changed for this task.

The tables below retain that historical baseline. The shared skid-state repair
and its current-source rerun are documented at the end of this report.

## Findings

The controlled tests support keeping the current tire corrections intact
while investigating specific behaviors. Straight-line braking varied modestly;
lateral-slip distance, turning losses and suspension transients varied more.
There was no meaningful backward creep, non-finite state or runaway response in
the tested rigs. This is evidence about the sampled synthetic configurations,
not proof that all vehicles and collision situations are stable.

The tests also reproduced shared skid-state contamination between separate
vehicles. That should be investigated independently before tuning tire forces.

## Method and reproducibility

Run against a current assembled Liberty City tree:

```powershell
.\tools\tests\test-wheel-suspension-fps.ps1 -StageRoot <assembled-tree> -OutDir <new-output-directory>
```

The fixture compiles extracted production implementations of `ProcessWheel`,
`GetMass`, point velocity, movement/rotation impulses, gravity, air resistance,
spring force, spring damping, translation and the wheel-contact grace timer.
It retains `FIX_BUGS` and follows the current configuration's
`FIX_SIGNIFICANT_BUGS` definition. That second flag is absent in this baseline.

There are 297 isolated combinations: nine scenarios, eleven rates (15, 18, 20,
30, 36, 60, 72, 80, 90, 120, 144 FPS), and three pacing profiles. Every
combination runs in a fresh executable process because `ProcessWheel` contains
function-static state. Production definitions are extracted rather than copied
into a replacement tire model. Output contains measurements and source hashes.

Profiles are steady intervals, alternating 85%/115% intervals, and a 120 ms
stall every nominal FPS-count of updates. The stall profile is therefore not
exactly one stall per wall-clock second. The fixture applies the existing
60 ms simulation cap and records wall time separately from simulation time.
At 15 FPS, four wall-clock seconds advance 3.6 simulation seconds. Comparisons
below use steady rates at or above 18 FPS unless otherwise specified.

Synthetic parameters: mass 1000, turn mass 2000, traction loss 0.8, wheel
friction 0.9, per-wheel adhesion 0.002, contact offsets +/-0.8 m sideways and
+/-1.2 m longitudinally, 0.5 m below the center for tire tests. These are not a
named vehicle's handling data.

The planar rig constrains height, pitch and roll, while allowing yaw in the
steering test. Native contact speeds are captured before wheel processing;
wheels run in native front-left/front-right/rear-left/rear-right order. Native
force application and effective mass are retained. Linear air drag and engine
thrust are excluded from these tire-only cases. Burst tires, wet surfaces and
handbrake transitions are not covered. Braking uses 0.005 times the
native timestep at each wheel. No reverse throttle or handbrake is applied.

The vertical rig uses four analytic suspension rays of length 0.5 m, spring
force 1.5, damping 0.15 and equal front/rear bias. It constrains pitch/roll,
uses native gravity, air drag, spring/damper ordering and translation, and
samples contact against an analytic horizontal road. It does not include
chassis/world collision, wheel meshes, surface materials or moving platforms.
The drop starts at 0.65 m with 3 m/s downward velocity. The bump raises the
road 8 cm for simulation times 1.0–1.2 seconds. Contact is sampled at updates,
so exact bump timing is quantized differently at each rate.

## Straight-line tire response

Braking and coasting start at 30 m/s. Lateral recovery starts at 4 m/s sideways.
Stopping means total speed falls below 0.05 m/s; distances use the production
post-force translation convention. They are synthetic rig measurements, not
predicted stopping distances for a GTA vehicle.

| FPS | Brake stop, game seconds | Brake distance, m | Coast distance, m | Lateral stopping distance, m |
|---:|---:|---:|---:|---:|
| 18 | 1.889 | 27.296 | 82.500 | 0.3951 |
| 30 | 1.900 | 27.627 | 82.835 | 0.4356 |
| 72 | 1.875 | 27.917 | 83.126 | 0.4722 |
| 90 | 1.878 | 27.959 | 83.169 | 0.4780 |
| 120 | 1.875 | 28.000 | 83.206 | 0.4833 |

At 72 versus 30 FPS, braking distance increased about 1.05%, coasting distance
about 0.35%, and lateral stopping distance about 8.42%. Lateral stopping time
fell from 0.267 to 0.250 game seconds even as distance increased: discrete
post-impulse translation and threshold sampling matter. A simple force-rate
multiplier cannot be justified from these figures alone.

No meaningful forward-speed sign reversals were recorded in the brake/coast
cases, including at 15 and 18 FPS. The commented-out contact-speed timestep
corrections were not enabled or tested, so this does not refute the source's
historical warning about their low-FPS behavior.

## Steering and combined tire forces

The four-second steering case holds front-wheel steer at 0.15 radians with no
thrust. It exercises competing longitudinal/lateral corrections and the tire
adhesion limit, without body roll or weight transfer.

| FPS | Travel, m | Final forward speed, m/s | Final lateral speed magnitude, m/s | Accumulated yaw, degrees |
|---:|---:|---:|---:|---:|
| 18 | 82.922 | 10.730 | 0.387 | 158.35 |
| 30 | 86.194 | 12.342 | 0.602 | 158.08 |
| 72 | 89.799 | 14.607 | 0.886 | 155.76 |
| 90 | 90.413 | 15.039 | 0.940 | 155.05 |
| 120 | 91.046 | 15.507 | 0.998 | 154.17 |

At 72 versus 30 FPS, final forward speed increased about 18.35%, while yaw
changed by only about 1.47%. Tire energy loss and sliding behavior are therefore
more sensitive here than the overall heading change. These are not turn-radius
or rollover measurements for an unconstrained car.

At 72 FPS, uneven pacing changed travel from 89.799 to 91.387 m despite nearly
identical total simulated time. Constant average FPS is insufficient to
characterize the current nonlinear tire solver.

## Suspension response

| FPS | Drop minimum height, m | Drop peak vertical speed, m/s | Bump maximum height, m | Bump peak vertical speed, m/s |
|---:|---:|---:|---:|---:|
| 18 | 0.3700 | 4.111 | 0.4531 | 0.234 |
| 30 | 0.2944 | 4.333 | 0.4870 | 0.442 |
| 72 | 0.3061 | 4.111 | 0.4992 | 0.599 |
| 90 | 0.3294 | 3.889 | 0.5016 | 0.641 |
| 120 | 0.3142 | 4.000 | 0.5029 | 0.674 |

For the bump, 72 FPS produced about 35.6% greater peak vertical speed than
30 FPS; peak height was about 12.2 mm higher. The drop's minimum height varied
non-monotonically. Contact sampling, force integration and damping limits all
interact; these tests do not assign each difference to a particular term.

Both rigs settled. Across these steady 18–120 FPS rows, height variation after
2.5 simulation seconds was below 0.13 mm. The low-rate transient differences
should not be described as perpetual suspension jitter based on this evidence.

The damper includes a timestep-scaled impulse and a velocity/effective-inertia
limit. Blindly multiplying every correction by another timestep would change
their interaction; no such change is supported by this characterization.

## Contact grace and history dependence

The contact timer represents 80 ms (`4 / 50`), but expires only at an update:
111.11 ms at 18 FPS, 100 ms at 30, 83.33 ms at 72, 88.89 ms at 90 and 83.33 ms
at 120. This changes how long a wheel can retain tire processing after losing
ray contact. The test measures the timer in isolation, not a jumping vehicle.

`ProcessWheel` has a function-static `bAlreadySkidding` flag. In this baseline,
once set, it is not cleared by the optional `FIX_SIGNIFICANT_BUGS` block.
A skid on one vehicle can therefore change a later vehicle's first normal
wheel response. With a 0.005 engine-unit lateral contact speed, four wheels
on ground, adhesion 0.002 and 72 FPS, a clean process applies a 0.062500 m/s
one-wheel lateral correction; priming the flag on another vehicle reduces
that to 0.055556 m/s, an 11.11% difference. This effect is state/history
dependent and only appears in particular force-limit regimes. It is not a
universal 11.11% grip loss, nor an FPS-only bug.

## Stalls, conclusions and next validation

At 72 FPS with the injected 120 ms stalls, four wall-clock seconds advanced
only 3.76 simulation seconds. Comparisons must distinguish this existing
slowdown from force-law drift. In the bump case, peak vertical speed increased
from 0.599 to 0.616 m/s. This isolated case remained bounded.

Recommended next steps before altering tire corrections:

1. Isolate and repair shared skid history, then rerun these same measurements.
2. Add unconstrained body rotation, weight transfer and actual collision
   contacts before using these numbers to tune vehicle handling.
3. On a headset, repeat named-vehicle braking, skidpad, curb and landing tests
   at controlled rates with logging. This task did not implement or run a
   deterministic in-headset test setup.
4. Consider a consistent physics-step strategy if full-vehicle tests confirm
   the sensitivity. Do not enable the commented-out timestep multipliers solely
   because the present code mixes velocity corrections and impulse limits.

Passing this characterization means the smoke checks succeeded and all sampled
states remained finite/bounded. It does not assert FPS-independent handling.
No gameplay force correction was changed.

## Shared skid-state repair and validation (2026-10-20)

Patch `11-re3-quest-wheel-skid-state.patch` makes the three `ProcessWheel`
working flags local to each invocation. `bAlreadySkidding` starts false and is
set only from that call's incoming wheel state. Existing per-wheel state,
traction-loss logic, force corrections, handling parameters and suspension
integration remain intact. Normal wheels and recovered wheels no longer inherit
another wheel's or vehicle's skid history.

The new production-function regression failed against the pre-repair source
with `another vehicle does not inherit skid history`. With the patch, all 366
suspension/skid-history checks pass. Coverage includes all three incoming
non-normal wheel states, all four wheel indices, same-wheel recovery,
cross-wheel/cross-vehicle calls and interleaved braking/driving at 15–144 FPS.
Incoming non-normal wheels still receive the existing traction loss.

All 297 controlled-rate scenarios were rerun against a freshly assembled tree.
Every clean/primed history pair now matches across all 11 rates and three pacing
labels. At 72 FPS the previously primed correction changes from 0.055555563 to
0.062500007 m/s, matching the clean case. The history probes are individual
calls; their pacing labels do not simulate a time sequence.

Compared with the saved baseline CSV, 270 of 297 rows are identical. The 27
changed rows are six primed-history probes and 21 steering cases. The steady
72 FPS steering rig now travels 89.640900 m instead of 89.798857 m, with final
forward speed 14.535110 instead of 14.606508 m/s. Its 30 FPS row is unchanged.
The remaining steering and suspension timestep sensitivity still needs the
full-vehicle/collision validation described above.

Validation artifacts are local under `.cache/skid-state-verified/`, including
`wheel-suspension-fps/measurements.csv`, source hashes, suite logs and
`results.json`. All 35 supported current-source suites passed. The arm64 debug
APK compiled successfully; no original game data was needed for these checks.
