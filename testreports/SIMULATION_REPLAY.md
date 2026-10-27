# Automobile simulation replay, first version

The VR overlay now has **Settings → Simulation Replay**. This is a driving
benchmark tool: it records the controls applied to the player's automobile and
a reference route, then runs those controls through the live simulation.

## Using it

1. Enter an automobile and stop upright. Select a recording limit of 30, 60 or
   120 simulation seconds. **Record Route repairs the car** and normalizes its
   starting suspension/contact history. The menu closes while recording.
   An upper-right HUD panel shows `0.0S RECORDED / 120.0S` (elapsed time and
   selected limit). It updates while recording and disappears when recording
   ends, independently of the profiler display. With minimal FPS visible, the
   recording panel sits below it. Both panels stay within both eye frusta.
2. Drive the route. Open the same page and select **Stop** to save early, or let
   the limit finish it. The new recording becomes the selected demo. Record at
   least one second for playback; avoid traffic impacts in benchmark routes.
3. Use the same car model, active asset selections and handling. Stop upright
   anywhere and select **Play**.
   Playback repairs and repositions this car; it does not spawn another car or
   load a saved game. A distant start loads the destination island collision
   and scene before teleporting the occupied car; there is no 100-metre range
   restriction. Scene loading is excluded from automatic playback profiling.
4. Playback prepares for two simulation seconds, restores the starting car
   state again, then applies the recorded accelerator, brake, steering and
   handbrake values. Head tracking stays live. Use **Stop** to interrupt it.

**Recording Ghost** and **Playback Ghost** are independent options, both on by
default. During the corresponding operation, the car, player and occupants ignore
pedestrian/vehicle/prop collisions in both processing directions. Physical and
dummy props, known static street lights, trees and fences are ignored. Ambient
entities still collide with each other. Roads, curbs, ordinary buildings and the
game's moving bridge road sections retain collision physics. Ghost playback suppresses incoming damage, explosions,
fire, tyre damage and carjacking, and blocks outgoing weapons/melee and vehicle
impacts against pedestrians. It also suppresses collision damage to the replay
car, including damage from scenery. No persistent collision/proof flags are
changed; these filters expire when the run ends. Playback ghost also applies
during preparation. Neither option changes ordinary gameplay outside a run.

**Route Assistance** defaults on for ghost playback. It anchors the automobile
to the interpolated recorded route before each simulation step, restoring route
position/heading and estimating linear/angular velocity from adjacent path
samples. Native automobile controls, physics, road/building collisions, ambient
simulation and rendering still execute. Sector membership is updated without
removing/readding the moving entity or toggling its audio. The car is not repaired
on each correction. Large corrections clear stale suspension contacts.

This makes long benchmark routes usable despite input replay divergence. It is
an assisted route benchmark, not an unmodified vehicle-physics accuracy test.
Existing V1/V2 recordings work; their path supplies an estimated upright basis
and velocity, not a complete recorded physics snapshot or exact banking/roll.
Turning assistance off keeps the original input-only simulation and 50-metre
drift abort. Disabling playback ghost also disables assistance. Use the same
tracking mode in before/after comparisons, and use input-only mode when testing
the uncorrected dynamics themselves.

**Delete Selected Demo** asks for a second press on the same row, then removes
only the selected input tape. Run reports and performance captures are kept.
Deletion is blocked during recording or playback. Selecting another action or
reopening the page cancels the pending deletion.

Unavailable starts keep the menu open with an explanation. If deeper tape or
state validation fails after queuing, the replay page reopens with the reason,
such as an invalid tape, a different car/model set/handling, or a short recording.
A start queued while normal gameplay cannot advance also times out with a
resume-gameplay message.

**Auto Profile Playback** defaults on. Starting playback closes any existing
manual performance capture. Preparation is excluded. After the first rendered
playback frame, a new capture starts with the minimal FPS/frame-time display.
The final playback frame is submitted before the capture closes and the
profiler switches off. Automated changes do not rewrite the user's saved
profiler display preference. With this option off, manual profiling is left
alone. Opening the menu does not itself end a run; select Stop explicitly.

## What is saved

Sampled geometry and model-owner detail are also recorded in a `.draws.csv`
companion; copy it with the frame/session files. See
[draw profiling](DRAW_PROFILE_PROFILING.md).

Whole-render-pass GPU intervals and logical phase draw counts are included automatically when
profiling. See [GPU pass profiling](GPU_PASS_PROFILING.md) for sampling, source
frame matching and the next before/after benchmark procedure.

Files are local under the application's external files directory:

- `simulation-replays/<UTC>-<monotonic-ms>.sim.csv`: a versioned input tape,
  model/handling identity, five active model categories, initial transform and
  velocity/gear state, and reference position/forward/speed samples.
  V2 tapes also store the recording ghost setting. V1 tapes remain readable;
  their recording ghost setting is reported as unknown (`-1`).
- `simulation-replays/<run-id>.run.csv`: completion or abort reason, demo name,
  ghost/auto-profile settings, linked profiler stem, game duration, comparison
  sample count, RMS/maximum position error, maximum heading/speed errors,
  build timestamp and profiler timing alignment.
  `tracking` identifies `route_assisted` versus `input_only`; `route_corrections`
  counts applied route anchors and `route_correction_max_m` reports the largest
  position adjustment. Drift statistics are measured before correction, so
  assisted completion does not conceal divergence. `reference_state` labels the
  estimated reference state, and `ghost_policy` identifies prop filtering.
- Existing `perf-captures/*.frames.csv` and `*.session.csv`: normal full
  profiling data. Session metadata now links the replay demo, run ID and ghost
  settings for recording and playback. The run report's `profile_start_simulation_us` identifies the initial
  portion excluded before the automatic capture started.

Only completed, bounded, finite, monotonically timed tapes with a valid END
marker can play. There are at most 20,000 samples and 120 simulation seconds.
Files use exclusive creation; existing demos are not overwritten. An empty or
failed write is removed. A crash leaves an unclosed tape which playback rejects.
The overlay may list such a file, but selecting Play reports that it is invalid.

The existing shell/app/root-only read-only diagnostics provider also supports
`content://com.libertyvr.quest.performance/replays` for listing, and
`content://com.libertyvr.quest.performance/replay/<filename>` for reading a
demo or run report. Performance captures retain their existing endpoints.
Names, extensions and canonical directory boundaries are checked separately
for each endpoint. The provider cannot import files, alter saves or export
original game assets. Keep device selection and identifiers local when using
ADB; shared reports must say **personal Quest 3 device**.

## Comparison limits

This is not a deterministic saved-world replay. Traffic, pedestrians, weather,
time of day, scripts, RNG, streaming/cache state and HMD/controller poses are
not restored. Horns, weapons, entering/exiting vehicles and on-foot movement are
not recorded. The engine's real simulation timestep is used, so frame rate and
physics changes can produce route drift. The recording and replay both start
with a repaired car and normalized suspension history; this is not a complete
snapshot of every internal car field.

Reference positions/speeds/heading are interpolated for comparison, while
controls are held until their next sample. Playback never corrects its route by
snapping in input-only mode. In that mode it aborts when position error exceeds
50 metres. Both modes abort if vehicle ownership or handling changes, a cutscene/native replay begins, a required input hook is
lost, an automatic capture fails/is interrupted, or five wall-clock minutes
elapse. Assistance also rejects nonfinite state or implausible reference
velocities rather than applying corrupt path data. User stops are reported as aborted playback. Completed runs can still
have substantial error: inspect the run report before treating two captures as
comparable. Use the same settings, car, route and ghost option for before/after
tests, and repeat runs to account for the live world and cache differences.

## Validation

Local execution; implementation and automated validation require source code
and the Android toolchain, not original game assets.

- Native host tests compile the production tape/parser/runtime and replay menu
  functions. They cover malformed/truncated tapes, frame-rate-independent input
  selection and reference interpolation, paused/repeated hooks, duration bounds,
  preparation, profiler lifecycle/failure/interruption, drift without snapping,
  model/asset/handling rejection, distant-start scene/collision loading, stale
  vehicle handles, recording/playback ghost symmetry/scope/cleanup, V1/V2 tape
  compatibility, deletion scope/confirmation, failed-start feedback, recording
  HUD lifetime/text bounds, file persistence and menu geometry.
  A full 120-second assisted regression uses changing timesteps and injected
  80-metre disturbances, checks completion and profiler shutdown, and verifies
  that reports retain correction counts and pre-correction divergence. Prop
  tests cover both entity orders, static furniture, dummy objects, ambient pairs
  and preserved moving bridge roads. Input-only drift abort remains covered.
- The minimal-profiler placement suite checks recording panels both alone and
  below FPS across asymmetric/canted binocular views, including narrow FOVs.
- Integration checks verify the World/input/physics/damage/fire/weapon/menu/app
  frame hooks, including all three physical collision-sector paths.
- Existing performance-writer, legacy vehicle-capture and graphics-layout host
  suites pass. Java host tests verify both export filename/directory boundaries.
- Android arm64 debug APK compilation passes. The update is installed with
  `install -r` on the personal Quest 3 device, preserving game data and saves;
  the new read-only replay listing responds on device. Automated tests use engine stubs;
  actual route repeatability and Quest visual/gameplay behavior still need a
  headset driving session.

Reproduce the focused suite with:

```powershell
./tools/tests/test-simulation-replay.ps1 -StageRoot <assembled-source> -OutDir <new-output-directory>
```
