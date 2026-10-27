# Automobile simulation replay, first version

The VR overlay now has **Settings → Simulation Replay**. This is a driving
benchmark tool: it records the controls applied to the player's automobile and
a reference route, then runs those controls through the live simulation.

## Using it

1. Enter an automobile and stop upright. Select a recording limit of 30, 60 or
   120 simulation seconds. **Record Route repairs the car** and normalizes its
   starting suspension/contact history. The menu closes while recording.
2. Drive the route. Open the same page and select **Stop** to save early, or let
   the limit finish it. The new recording becomes the selected demo. Record at
   least one second for playback; avoid traffic impacts in benchmark routes.
3. Return within 100 metres of the start in the same car model, using the same
   active asset selections and handling. Stop upright and select **Play**.
   Playback repairs and repositions this car; it does not spawn another car or
   load a saved game. The nearby-start requirement avoids moving into an
   unloaded island/collision scene.
4. Playback prepares for two simulation seconds, restores the starting car
   state again, then applies the recorded accelerator, brake, steering and
   handbrake values. Head tracking stays live. Use **Stop** to interrupt it.

**Playback Ghost** defaults on. The replay car, player and occupants ignore
pedestrian/vehicle collisions in both processing directions. Ambient entities
still collide with each other; roads, curbs, buildings and objects retain
collision physics. Ghost playback suppresses incoming damage, explosions,
fire, tyre damage and carjacking, and blocks outgoing weapons/melee and vehicle
impacts against pedestrians. It also suppresses collision damage to the replay
car, including damage from scenery. No persistent collision/proof flags are
changed; these filters expire when the run ends. Ghost mode applies during
preparation and playback, not during normal gameplay or recording.

**Auto Profile Playback** defaults on. Starting playback closes any existing
manual performance capture. Preparation is excluded. After the first rendered
playback frame, a new capture starts with the minimal FPS/frame-time display.
The final playback frame is submitted before the capture closes and the
profiler switches off. Automated changes do not rewrite the user's saved
profiler display preference. With this option off, manual profiling is left
alone. Opening the menu does not itself end a run; select Stop explicitly.

## What is saved

Files are local under the application's external files directory:

- `simulation-replays/<UTC>-<monotonic-ms>.sim.csv`: a versioned input tape,
  model/handling identity, five active model categories, initial transform and
  velocity/gear state, and reference position/forward/speed samples.
- `simulation-replays/<run-id>.run.csv`: completion or abort reason, demo name,
  ghost/auto-profile settings, linked profiler stem, game duration, comparison
  sample count, RMS/maximum position error, maximum heading/speed errors,
  build timestamp and profiler timing alignment.
- Existing `perf-captures/*.frames.csv` and `*.session.csv`: normal full
  profiling data. Session metadata now links the replay demo, run ID and ghost
  setting. The run report's `profile_start_simulation_us` identifies the initial
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
snapping. It aborts when position error exceeds 50 metres, vehicle ownership or
handling changes, a cutscene/native replay begins, a required input hook is
lost, an automatic capture fails/is interrupted, or five wall-clock minutes
elapse. User stops are reported as aborted playback. Completed runs can still
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
  model/asset/handling/start-distance rejection, stale vehicle handles, ghost
  symmetry/scope/cleanup, file persistence and menu geometry.
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
