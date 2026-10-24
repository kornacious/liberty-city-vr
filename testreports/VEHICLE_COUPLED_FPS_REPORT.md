# Coupled vehicle frame-rate characterization

Date: 2026-10-20. Runtime baseline: `25e9810`, including the skid-state,
settling/sleep and helicopter timing repairs. This task changes host validation
and documentation; it introduces no runtime force correction or fixed-step loop.

## Findings

Coupling chassis rotation and suspension confirms timestep-sensitive transients.
Small changes in total travel coexist with larger changes in pitch, roll and
curb response. All sampled states stayed finite; none of these rigs inverted.
That is a boundedness result, not proof of correct handling or collision safety.

At steady 72 versus 30 FPS in the native variable-step mode:

| Scenario / metric | 30 FPS | 72 FPS | Change |
|---|---:|---:|---:|
| Braking, six-second total chassis path | 12.3053 m | 12.4189 m | +0.92% |
| Braking, peak pitch magnitude | 1.7818 degrees | 1.4296 degrees | -19.8% |
| Cornering, six-second total chassis path | 24.7390 m | 25.0513 m | +1.26% |
| Cornering, peak roll magnitude | 1.2212 degrees | 1.3001 degrees | +6.47% |
| Curb, peak pitch magnitude | 1.7405 degrees | 2.1561 degrees | +23.9% |
| Curb, peak vertical speed magnitude | 0.3543 m/s | 0.4287 m/s | +21.0% |
| Hard landing, deepest detected body overlap | 162.36 mm | 30.22 mm | -132.14 mm |
| Wall impact, deepest detected body overlap | 46.81 mm | 17.11 mm | -29.70 mm |

Path length includes vertical motion and settling over six game seconds; it is
not a retail-car stopping-distance prediction. Peak velocity includes the whole
transient. Overlap is measured before the fixture's explicit projection and
must not be presented as measured in-game penetration: the engine's world
collision retry/shift pipeline is not running here.

Spring compression redistributes between wheels without artificial roll/pitch
constraints. For braking, peak normalized front/rear compression imbalance was
0.4330 at 30 FPS and 0.3354 at 72 FPS. The curb case reached 0.5961 and 0.7584.
This measures the spring contribution to load redistribution, not total tire
normal force including dampers and body collisions.

## Method

Run against a freshly assembled current Liberty City tree:

```powershell
.\tools\tests\test-vehicle-coupled-fps.ps1 -StageRoot <assembled-tree> -OutDir <new-output-directory>
```

The runner extracts production gravity, air resistance, point velocity, mass,
move/turn impulses, spring collision/damping, wheel forces, move/turn integration,
matrix reorthogonalization, wheel contact grace, line/box and sphere/box narrow
phase, and `ApplyCollisionAlt` static response. It reads the production game
version and optional significant-bug configuration. There is no copied game
asset dependency.

The fixture chassis has all six degrees of freedom. Its four suspension rays
rotate with it, and tire forces act at their actual ray contact positions.
Spring impulses precede dampers; contact speeds are resampled before tire
processing. Wheels run in FL/FR/RL/RR order. Body contacts use six spheres,
native collision detection/response and the native averaging convention for
multiple response velocities. Matrix repair uses the production implementation.

Prescribed values: mass 1,200 kg, scalar turn mass 2,500 engine units, zero
local center-of-mass offset, half track 0.75 m, half wheelbase 1.2 m, suspension
line length 0.70 m, travel 0.45 m, spring constant 1.5, damping 0.15, symmetric
0.5 spring bias, adhesion 0.002 and traction loss 0.8. Six body spheres have
radius 0.28 m. Road and obstacles are generated boxes.

Eight scenarios cover stationary equilibrium, braking from 20 m/s, coasting
cornering from 15 m/s with 0.10-radian steer after 0.5 game seconds, an 18 cm
right-side curb approached at 8 m/s, level and tilted landings from 1.5 m with
4 m/s downward velocity, a hard landing from 2 m with 15 m/s downward velocity,
and a one-meter obstacle approached at 8 m/s. Tilt starts at 0.12 radians.

Each runs for six **game seconds**, recording wall time separately. Render
rates are 15, 18, 20, 30, 36, 60, 72, 80, 90, 120 and 144 FPS. Pacing is steady,
alternating 0.85/1.15 frame duration, or one 120 ms stall per nominal second.
The native 60 ms clip is applied before advancing the experimental accumulator.
The first-low-speed timestamp samples total 3D speed below 0.05 m/s after
0.1 seconds; it is not purely a longitudinal stopping-time measurement.

## Fixed-step experiments

The matrix includes the variable native step and experimental fixed 30, 60,
120 and 240 Hz physics steps: **1,320 cases** in total. Each fixed-step group
produces identical recorded simulation metrics across all render rates and
pacing patterns. Wall time differs when clipping discards time. Fixed 30 Hz
also reproduces the native steady 30 FPS fixture row exactly in every scenario.
This is enforced by the runner, not inferred from a few selected rows.

| Fixed physics rate | Brake path | Curb peak pitch | Hard-landing overlap |
|---:|---:|---:|---:|
| 30 Hz | 12.3053 m | 1.7405 degrees | 162.36 mm |
| 60 Hz | 12.4002 m | 2.0727 degrees | 60.59 mm |
| 120 Hz | 12.4583 m | 2.1739 degrees | 49.16 mm |
| 240 Hz | 12.4891 m | 2.2329 degrees | 2.98 mm |

Render independence does not establish accuracy or convergence for every
observable. Contact timing, impulse limits, input sampling and integration
remain sensitive to the chosen physics rate. At 72 render FPS, the stall case
takes about 6.37 wall seconds to reach six game seconds; fixed stepping retains
the native clipping slowdown rather than pretending the missing time elapsed.

These are host numerical experiments, not Quest performance measurements.
At 120 Hz a clipped 60 ms frame may require eight catch-up steps; at 240 Hz it
may require fifteen. The real collision/world workload must be profiled before
choosing such a budget. A 30 Hz solver would require suitable visual
interpolation and latency assessment for VR.

## What this validates, and what remains

The smoke checks verify ray hits, sphere overlap, static impulse direction,
four-spring equilibrium, symmetric stability, and asymmetric free-roll/load
response. The matrix adds 2,108,354 numerical checks; the smoke suite adds
2,886. Fixed-rate consistency, the native 30 FPS reference and actual body
contacts in hard-landing/wall scenarios have explicit assertions. A rerun with
the production version branch produces byte-identical CSV measurements.

This is a **coupled synthetic vehicle rig**, not a full `CAutomobile` gameplay
execution. It does not run retail model collision, world-sector broad phase,
the native collision retry/shift loop, body-contact friction, transmission/engine
control, collision damage, moving surfaces, sleep, rendering or gameplay callbacks. Deepest-contact
projection is fixture code. Its collision metrics particularly need confirmation
in the full engine. Inputs and contacts are sampled at physics updates; no
historical geometry is reconstructed during catch-up.

The appropriate next implementation is an opt-in full-engine capture/replay
harness before changing tire corrections: log native timestep, model/handling,
inputs, contacts, spring ratios, chassis pose/velocities, collision retries and
sleep decisions for named-car braking, skidpad, curb, wall and landing runs.
Compare 30 FPS reference runs with 72/90/120 FPS and injected stalls, then profile
a bounded fixed/substep prototype with damage, AI and mission callbacks checked
for repeated execution. Only then select a runtime strategy. This task does
not justify uncommenting the historical wheel timestep corrections.

## Artifacts

All 39 supported host suites pass. The final expanded/configured fixture output
is `.cache/coupled-vehicle-production-config/measurements.csv`, with source
hashes beside it. The earlier full-suite run, logs and APK build are under
`.cache/coupled-vehicle-verified/`. No original game data was transferred or
required. Headset scenario execution and actual Quest solver-cost profiling
were not performed by this host characterization.
