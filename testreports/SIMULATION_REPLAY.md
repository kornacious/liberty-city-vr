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
   handbrake values. **Benchmark Forward View** defaults ON for a fixed cockpit
   forward view; turn it OFF for live head tracking. Use **Stop** to interrupt it.
   With minimal profiling active, a separate line immediately below FPS/frame
   time shows `PLAYING 37.2S OUT OF 120.0S`. It uses elapsed simulation playback
   time and the selected demo's actual recorded duration, including recordings
   stopped early. It excludes preparation time, does not advance while the
   simulation is paused, and disappears on stop, completion or abort. The timer
   is hidden by the full menu and is not added to the full profiler display.
   Its compact panel reuses the recording HUD strip and binocular-safe placement;
   the independent recording timer keeps its existing behavior.

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

## Fixed forward benchmark view

Simulation Replay -> BENCHMARK FORWARD VIEW defaults ON. The preference is
latched when playback starts and cannot be changed during an active run.
It affects playback preparation and playback only; route recording and normal
play remain head tracked. Existing demo files need no conversion.

The synthetic stereo camera uses the cockpit's neutral tracking position and
yaw, so its view follows the vehicle's forward direction rather than the user's
physical head direction. Physical yaw, pitch, roll and room-scale displacement
cannot change that camera during the run. Eye spacing, relative eye orientation
and per-eye FOV are retained from the first eligible frame. The same synthetic
eye/head poses feed projection, culling and in-world HUD placement. No extra
scene pass or render target is introduced. Invalid initial stereo poses or an
unavailable cockpit neutral pose fall back to normal tracking.

The OpenXR projection submission and overlay retain the real runtime poses.
This is an intentional head-independent game camera, not a tracking replacement:
the menu remains head tracked and live game views resume on the next frame
after completion, abort or stop. The standalone bring-up target is excluded.

Run reports include the requested `view_policy` and automatic profiling context
includes `view=fixed_forward` or `view=live_head_tracking`. Session metadata adds
`replay_forward_view_at_start`; the additive `replay_forward_view_active` frame
column records the effective 0/1 mode, including fallback. Schema 6 and existing
column order are preserved. The analyzer exposes every observed state in
`recorded_configuration`; verify a stable 1 for fixed-view benchmark captures.

For the opaque-world A/B comparison, leave BENCHMARK FORWARD VIEW ON for all
four runs and change only OPAQUE WORLD FAST PATH OFF -> ON -> OFF -> ON. Use the
same demo, 1512 x 1584 at 120 Hz, and fixed graphics/SideQuest settings. This new
camera establishes a fresh comparison set rather than making older manual-head
captures equivalent to it. Other live-world and controller variation remains.

Host checks cover 625 initial yaw/pitch/roll combinations and translations,
stereo spacing/cant, unchanged tracked inputs and FOV, run changes, OFF/reset and
invalid inputs. Replay runtime tests cover default-on, recording exclusion,
active-run policy immutability, OFF, completion and capture provenance. Writer
and analyzer tests cover both effective states and legacy CSV compatibility.

## Replay-timed RenderDoc capture

Launch the debug APK through RenderDoc Meta Fork, then open Simulation Replay.
Use RENDERDOC CAPTURE CURRENT FRAME for a one-shot capture of the next complete
rendered XR frame. It closes the overlay and works in ordinary gameplay or during
recording, preparation and playback, independently of RENDERDOC AUTO CAPTURE and
CAPTURE AT. It leaves the scheduled auto trigger intact. An unavailable API or
busy external capture reports failure without taking ownership of that capture.

Each new demo starts with RENDERDOC AUTO CAPTURE OFF and CAPTURE AT zero. Complete
at least one playback before enabling auto capture or editing the target. A
completed playback with valid non-capture samples stores its lowest instantaneous
app FPS (1000 divided by the largest raw frame-start interval), with the preceding
frame's exact simulation timestamp in microseconds. Preparation, pauses and the
first resumed interval are excluded. Capture runs and manually captured runs do
not update the baseline; aborted runs do not unlock auto capture. The minimum FPS
and time are visible below the menu and recorded in each `.run.csv` report.

The default CAPTURE AT becomes the worst measured timestamp across completed
baseline playbacks, displayed to six decimal places. Auto capture remains OFF
until explicitly enabled. Target edits use 0.5-second steps; a custom target is
preserved when a later baseline finds a slower frame. A per-demo `.capture.csv`
sidecar stores auto mode, target, custom/default selection, completed baseline
count, worst frame interval and timestamp. Selection and application restart
reload it; old demos without a sidecar start with the same safe defaults. Deleting
a demo removes its settings but retains performance reports. Writes use a
temporary file and backup replacement; invalid settings restore the defaults.

Set RENDERDOC AUTO CAPTURE ON and start the selected demo normally. Enabling
it disables the menu's Matched workload option, since captured runs are diagnostic;
exit an active imported matched profile first. Auto options are latched per run.
One capture is attempted on the first rendered, unpaused playback frame whose
pre-step simulation clock reaches that time. Preparation time is excluded.
The target must precede the loaded demo's actual end, including short demos.

The integration passively obtains API 1.4+ from an already injected RenderDoc
module; it does not load or enable a capture layer in normal gameplay. If that
API is unavailable, starting a timed-capture replay reports a visible failure
before teleporting the car. Existing GUI-owned captures are left untouched;
a busy capture at the target is recorded as busy, without automatic retries.

Explicit capture boundaries run after xrBeginFrame and before any frame Vulkan
commands, through xrEndFrame after app submission. They use the Vulkan instance
dispatch pointer required by RenderDoc, with no window/present dependency.
Non-rendered/failed frames and teardown discard only a capture owned by this
integration. Replay completion/stop cleanup waits for capture finalization.
The standalone bring-up renderer has no replay capture hooks.

Files remain in the app-local `files/renderdoc-captures/` directory, prefixed by
the run identifier (or `manual-` plus a unique identifier). RenderDoc's previous capture filename template is restored
after success, failure or discard. A saved capture appears in the connected
RenderDoc capture list; use its normal save/copy workflow to retain it on this
computer. Captures contain game resources and must remain local.

Run reports include requested/actual trigger times, start/end monotonic clocks,
result and local filename. The capture context includes `renderdoc=1` and the
requested time. The additive frame column `renderdoc_capture_active` identifies
the captured source CPU frame. Session metadata also records `renderdoc_attached_at_start`, so an injected
session is marked diagnostic even with the timed capture option OFF.
The analyzer marks these as diagnostic runs;
capture interception and file writing can affect neighboring frames too. Use
RENDERDOC AUTO CAPTURE OFF in an unhooked game session for ordinary performance
comparisons. The capture flag does not attempt to label individual delayed GPU
results as originating from the delivery row. Schema 6 remains supported.

API-table tests cover passive/unavailable API behavior, exclusive capture
ownership, instance dispatch selection contract, capture path reporting,
template restoration, start/end failures and discard. Replay tests cover clock
thresholds, warmup/pause exclusions, one attempt per run, frozen options, invalid
times, visible attachment errors, stop during capture, report finalization and
discard. CSV/analyzer checks cover diagnostic frame marking and legacy data.
Actual Meta-fork capture/replay validation requires the injected first demo run.

Local validation on 30 October 2026: the RenderDoc API-table tests, 13,563 replay
runtime/menu checks, CSV writer and six analyzer tests passed. The full debug
APK built successfully in 75 seconds and its merged manifest is debuggable.
All eleven modified production sources match the assembled build. Deployment
was skipped because no authorized, responsive Quest 3 was connected. A real
injected capture and headset visual verification remain pending.

## Recovering a remote capture after the app closes

A RenderDoc remote capture entry/thumbnail can remain visible after its target
connection closes. A failed transfer may leave a zero-byte PC file and opening
that file reports an I/O error reading the magic number. Save/copy the remote
capture before closing the game when using the GUI's target connection.

The original capture may still be valid in the app-owned external capture folder,
which Android can deny to both the shell and run-as. Do not erase app data or
change game-data storage permissions. The shell/app-only performance provider
now exposes read-only `renderdocs` listing and `renderdoc/<name>` streams for
strictly generated .rdc filenames inside `renderdoc-captures`. It rejects path
traversal, other file types, redirected files/directories and all writes. Other
applications remain excluded by the existing UID check. CSV/replay exports are
unchanged. These captures include game resources and must remain local.

Use the local helper, with ADB on PATH or an explicit --adb path:

```powershell
python tools/export-renderdoc-capture.py --output .cache/renderdoc-recovered --latest 2
```

It selects exactly one authorized, responsive personal Quest 3 device privately,
checks the provider's full byte count and RDOC signature before creating a file,
computes SHA256 and refuses to overwrite different existing contents. Open the
recovered local file through File -> Open Capture in a compatible headset
profiling replay context; do not reopen an empty placeholder or stale remote
entry. Header/thumbnail parsing is distinct from full GPU replay validation.

Validated locally on 30 October 2026: Java filename/path boundary tests and
three export tests passed; the full APK built in 57 seconds and installed with
app data preserved. The exporter recovered two non-empty originals with matching
source sizes and valid signatures, and RenderDoc parsed both embedded thumbnails.
GPU profiling replay is not included in that validation.
