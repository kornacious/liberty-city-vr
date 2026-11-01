# Host regression tests

`test-boat-window-depth.ps1 -StageRoot <assembled-tree>` executes the boat
body submission block with the production Vulkan vehicle-alpha policy. It
checks water visibility through all transparent material alpha values, opaque
hull depth, the wheels-only cheat, and state restoration before the hull water
mask and subsequent world draws. In-game appearance still needs headset QA.

`python3 tools/tests/test-device-diagnostics.py --powershell powershell.exe`
checks the production Windows installer selection and ADB boundaries using a
fake native process. `--powershell pwsh` selects PowerShell 7; `--bash bash`
checks the Unix installer functions and tee logs. Both flags may be combined.
It verifies console/log redaction, numbered selection, unauthorized/offline
devices, native stdout/stderr, failure exit codes and explicit target routing.
It uses no Git commands, network, game data or real device.

`python3 tools/tests/test-patch-series.py <local-re3-clone>` checks that every
patch uses LF and applies the complete series to the pinned upstream revision.
It exports tracked source with `core.autocrlf=false` so Windows also exercises
the clean Linux source bytes. It needs Python 3.12+ and Git, uses a temporary
directory, and does not read game data or modify the upstream checkout.

`test-android-audio.ps1` covers the production Android callback and controls,
headset frame composition, and measured binaural filters. It checks front/rear
separation, world-coordinate ear ordering, elevation, independent head/body
rotation, tilted cockpit and room translation, HRIR tails, channel reuse,
callback partitioning, smoothing and spherical seams, alongside the existing
attenuation, resampling, looping and frontend stereo checks. Perceived
localization and real-time performance still require the headset.

These tests compile production helpers or extract production functions into
small native adapters. They check code behavior; they do not launch the game,
replace game data or establish headset appearance/FPS.

`test-holster-inventory.ps1 -StageRoot <assembled-tree>` checks independent
inventory slots, holster assignment cycling, centre EMPTY persistence and
settings reload normalization. `test-throwable-preview.ps1` uses the same
argument and compiles the production trajectory renderer to check grenade and
Molotov arcs in both hands, collision termination and visibility gates.
`test-laser-cache.ps1` checks cached per-weapon profiles and verifies that the
Weapons page OFF switch suppresses saved ON overrides in both model sets.

`test-plane-visibility.ps1 -StageRoot <assembled-tree>` compiles the actual
big-building vehicle branch and shared visibility helper in VR and non-VR
configurations. It checks planes outside the mono view remain eligible during
the shared VR scan, while unloaded/hidden planes and mono/theater clipping
retain their previous behavior. Headset appearance requires an in-game check.

`python3 tools/tests/test-loading-artwork.py <assembled-tree>` uses a C++17
compiler (`--cxx` selects it; `--cxx msvc` discovers the Windows toolchain) to
exercise the production splash loader, artwork fit, `DoFade` and Vulkan
fullscreen transform. It checks cyan texture selection/cache reuse, native
aspect ratio, maximal fit without cropping, centered black bars, all 256
opacities, solid camera fades, scope reset, and twelve framebuffer sizes.
Pixel compositing checks verify that the artwork and black surround reveal the
world at the same rate, without applying the fade twice beneath the image,
and that inherited blend factors and vertex-alpha state are restored.
It also runs 2,430 projections using the production vertex shader's projection
logic and shared-plane builder, checking binocular convergence under asymmetric
eye frustums, head yaw/roll and translation, constant physical size, no clipping
and foreground overlay depth. These checks do not establish headset appearance.
GPU rendering and compositor presentation still require headset verification.

`test-mission-counter-layout.ps1 -StageRoot <assembled-tree>` executes the
production mission counter drawing block with recorded font and rectangle
calls. It covers numeric and bar counters, every fill percentage from 0 to 100,
appearance flashing and inherited alignment/background/font settings when the
clock and timer are absent. It verifies right-anchored labels and fixed meter
bounds; actual glyph rendering still requires headset verification.

Windows tests require Visual Studio 2022 or Build Tools with the Desktop
development with C++ workload. Run from a disposable source-kit copy because
some extraction runners write temporary `.inc` files beside the test source.
Those files are ignored and must not be included in a release archive.

`test-radio-station.ps1 -StageRoot <assembled-tree>` exercises the production
radio board selection, native retune dispatch and stream switch with a fake
audio backend. It covers both directions, custom music availability, Radio Off,
station wrapping, pending X-button taps, rapid selections and police vehicles.
Listening to playback still requires headset verification.

`test-radio-touch.ps1` exercises production input and menu functions for the
saved laser/touch setting, fingertip contact, release hysteresis, both hands,
tracking/menu/weapon transitions, driving capture and the original laser input.
The fake poses do not establish physical reach or headset appearance.

`test-cd-directory.ps1 -StageRoot <assembled-tree>` compiles the production
archive directory with assertions disabled, as in the APK. It adds 4,096 entries
from initial capacities 0, 1 and 128, verifies all sector ranges/image selectors,
and covers duplicate handling and aliased insertion across reallocation. Both
FIX_BUGS configurations run. The former fixed table overwrote the heap at 129.

`test-asan-apk.ps1 -Apk <apk>` checks that a player APK excludes the sanitizer
runtime and launch wrapper. Add `-ExpectAsan` to check a diagnostic APK's arm64
runtime, wrapper, preload command and LF line endings. This artifact check runs
separately after building an APK.

`test-cd-stream-sync.ps1` runs the production Android streaming queue with a
controlled reader. It exercises 20,000 completion-before/during-wait schedules,
queued channels, duplicate/replacement reads, buffer ownership, read errors,
flush and 100 shutdown/restart cycles. A deadline fails stalled runs. Actual
archive I/O and rare end-to-end save-load reproduction require the headset.

`test-mission-selector.ps1 -StageRoot <assembled-tree>` extracts the actual
mission safety check and switch routine. It checks free-roam launches with
main-script mission threads, preservation of those threads during cleanup,
rejection of loaded missions without a death/arrest return frame, safe loaded
mission cleanup, invalid indices and file read failures. The previous source
fails the background-thread launch case. File I/O and script execution are
adapters; live mission playback still requires headset verification.

Run the supported Liberty City suites together from a normal PowerShell session:

```powershell
.\tools\tests\test-current-sources.ps1 -Re3Root C:\LCVRBuild\re3-public-b050e1c
```

Use the clean GTA III/re3 upstream described in BUILDING.md. The runner rejects
reVC input, assembles the current kit and all patches in a new directory, and
runs 52 suites against that tree or the kit's production helpers. Each suite
runs in a separate process. `results.json` records exit codes, log paths, totals
and exclusions; any failed suite makes the command fail after writing the
report. Excluded scripts never count as passes. No game assets, headset or
private historical checkout is needed. Use `-OutDir` with a new directory to
choose where the assembled sources, binaries and logs are kept.

`test-traffic-visibility.ps1 -StageRoot <assembled-tree>` checks the production
VR traffic spawn/retention policy across 360 directions and generation distance
multipliers, including near-spawn rejection and camera separation boundaries.
It also verifies integration into random spawning and both cleanup paths.
The policy uses the distant spawn ring in every direction and retains nearby
traffic independently of the mono camera frustum. These checks do not establish
headset appearance, traffic density or performance.

`test-held-weapon-frame.ps1 -StageRoot <assembled-tree>` extracts the body IK
entry point, floating-hand pose conversion, both skeleton carrier blocks and
rigid weapon reconstruction. Synthetic moving anchors cover 15/30/45/60/72/80/
90/120/144 FPS with alternating frame durations, running, stopping, turning,
teleports and tracking loss. Both grip modes and skeleton paths must preserve
camera-relative gun position without applying locomotion twice. Skeleton bone
storage is an adapter boundary; this test does not evaluate rendered appearance
or the full two-hand constraint. The previous source fails the anchor-order test.

`test-weapon-cadence.ps1 -StageRoot <assembled-tree>` extracts the tracked
on-foot and vehicle firing blocks plus the native ammo transaction, reload and
weapon-state update. It compiles those with the production cadence clock. Checks
cover ten rates from 15 to 180 FPS, alternating frame durations, fractional loop
intervals, magazine depletion, semi-auto trigger edges, independent guns, hand
transfers, tracking loss, long stalls, timer wrap, automatic/manual reloads and
the native 70 ms vehicle Uzi interval. Synthetic loop widths avoid distributing
original game data. Ballistics, sound and effects are adapter boundaries; live
headset cadence and desktop comparison still require playing the game.
The same suite tests both pistol trigger modes on foot/in vehicles, in either
hand, plus the production Weapons page rows, setting mapping, default, toggle
and save/reload behavior. Both mode labels and all nine rows must fit the panel.

The repaired adapters use Liberty City's actual weapon inventory, animation
names, wanted header and crime definitions. They do not add Vice City weapon
aliases or substitute passing implementations for missing game methods. The
reaction/braking calibration fixtures explicitly select a 70 kg body; the
separate weight suite covers current tunable/default weight behavior. The
explosion pose check compares captured solver coordinates before and after
the impulse rather than assuming solver indices equal skeleton bone indices.

`test-android-audio.ps1 -KitRoot .` extracts the production Android channel
state, spatial/volume setters, channel initialization and AAudio mixer callback.
Synthetic PCM checks near/mid/far gain, all three position axes, moving sources,
range updates and setter ordering, volume/fade/cutscene scaling, channel reuse,
2D audio and invalid ranges/coordinates. It needs no game files or Android
audio device. Listening tests on the headset are still needed to judge balance.

`test-autosave.ps1 -StageRoot <assembled-tree>` tests the production autosave
transaction and extracts the engine filename/checksum/recovery functions. It
injects failures at each transaction boundary, checks that manual slots remain
unchanged, tests first save and backup recovery, rejects truncated/checksum-bad
files and invalid block lengths, and checks both load-menu layouts. All save
bytes are synthetic; no game assets or player saves are accessed. These checks
do not replace mission-completion and load testing on a headset.

`test-frame-timers.ps1 -StageRoot <assembled-tree>` compiles the production
timer against synthetic display timestamps and extracts the damaged-engine
drain and script timer consumer. It checks 20–144 FPS, uneven pacing, slow
motion, pauses, stall clipping, rollover, clock replacement and agreement
between timer consumers. No game data or headset is needed.

`test-angular-drag-boat-lift.ps1 -StageRoot <assembled-tree>` extracts the
production air-resistance function, buoyancy impulse calculation and extra
boat-lift block. It checks the 30 FPS reference and equal-time angular decay,
uncapped/capped lift and pitch torque at 20–144 FPS, including uneven pacing,
slow motion, zero timestep and lift eligibility gates. These isolate forces at
fixed speed/immersion; they do not establish full boat handling on a headset.

`test-wheel-suspension-fps.ps1 -StageRoot <assembled-tree>` characterizes the
production wheel and suspension functions in constrained synthetic
rigs. It runs 297 fresh-process combinations of nine scenarios, 15–144 FPS and
steady/uneven/stalled pacing, writing `measurements.csv` and source hashes to
its output directory. Passing means smoke checks and finite bounded states;
it does not mean handling is FPS-independent. Its regression checks also verify
that skid history stays within each wheel call, that a recovered wheel regains
normal grip, and that incoming non-normal wheels retain their traction loss at
15–144 FPS. See
[the detailed findings](../../WHEEL_SUSPENSION_FPS_REPORT.md) for rig assumptions,
measured differences and limits. No original game data is used.

`test-vehicle-coupled-fps.ps1 -StageRoot <assembled-tree>` characterizes a free
six-degree chassis with rotating suspension rays, unequal spring loads, native
wheel/spring/damper forces, native box narrow-phase contacts and static contact
response. Generated geometry and an explicit fixture depenetration replace
retail vehicle models, world sectors and the engine collision retry/shift loop.
It runs 1,320 cases: eight scenarios, 15–144 render FPS, three pacing patterns,
and native/experimental 30/60/120/240 Hz stepping, each over six game seconds.
It checks geometry/response wiring, numerical bounds, native matrix repair,
and render-pacing independence of fixed-step outcomes. It writes measurements
and source hashes; fixed stepping is experimental and changes no runtime code.
See [the coupled-vehicle report](../../VEHICLE_COUPLED_FPS_REPORT.md) for results,
assumptions, exclusions and the required full-game validation follow-up.

`test-vehicle-capture.ps1 -StageRoot <assembled-tree>` compiles the capture runtime
against engine adapters and checks completed/invalid input tapes, recording,
replay timing, pose restoration, command/session lifecycle, handle generations,
model/handling mismatches, pause, duration bounds and duplicate control calls.
A separate executable links disabled hooks without the runtime implementation.
Native integration hooks are checked in the assembled world/car/collision code.
See [capture/replay instructions](../../VEHICLE_CAPTURE_REPLAY.md).

`test-boat-input-timing.ps1 -StageRoot <assembled-tree>` extracts the production
boat input function and gameplay clock helper. It checks the original 30 FPS
response, throttle/brake deadzone and reverse transitions on crossed 30 Hz
ticks, and smooth exponential steering at 15–144 FPS. Coverage includes pad
and immersive helm input, uneven pacing, slow motion, pause, clipping/catch-up,
steering reversals, output bounds and the signed squared steering lock. These
are control-filter tests, not full hull dynamics or headset handling tests.
See [repair notes and timing limits](../../BOAT_INPUT_TIMING_REPORT.md).

`test-physics-sleep-timing.ps1 -StageRoot <assembled-tree>` compiles the production
object sleep control, extracted vehicle sleep block and shared timing helper.
It checks 30 FPS reference behavior, equal-time averaging, quiet-tick counting,
speed/angular thresholds and previous-frame displacement normalization across
15–144 FPS, uneven pacing, slow motion, pause and catch-up. Wake resets,
moving-map gates, counter saturation and the stationary-contact fast path are
covered. Source checks verify every displacement producer/reset updates the
sleep cache in existing storage. See
[repair notes and limits](../../PHYSICS_SLEEP_TIMING_REPORT.md).

`test-heli-control-timing.ps1 -StageRoot <assembled-tree>` extracts helicopter
avoidance/recovery and Catalina landing blending. It checks 15–144 FPS,
uneven pacing, slow motion, pause, crossed 30 Hz decision windows, blocked-side
responses, held lateral decisions and shot-down contact timers against an
independent 30 FPS reference. Landing convergence is checked against an analytic
elapsed-time response. Catch-up and separate helicopter state are included.
See [repair notes and limits](../../HELI_CONTROL_TIMING_REPORT.md).

`test-gameplay-timing.ps1 -StageRoot <assembled-tree>` compiles the production
timer, gameplay schedule helper, swinging-door simulation and aircraft
destruction sequences. It checks 15–144 FPS, door axes/directions, 30 FPS
reference motion, pause/slow motion, uneven pacing, bounded door stops,
crossed schedule phases, partition coverage and deferred destruction with
exactly-once explosions and mission callbacks. Source checks verify the
helper is wired into the selected engine schedules; full gameplay scenarios
still require headset testing.

The header-only ragdoll suite works directly against the kit:

```powershell
.\tools\tests\test-ragdoll.ps1 -KitRoot .
```

Tests that extract patched engine functions require a freshly assembled re3
tree via `-StageRoot`, `-KitRoot` or `-EngineRoot`, as declared by that runner.
Optional `-UseBaseline` modes require a private historical snapshot and are
outside the current-source run. Read each runner's parameter block before
invoking it.

The following retained reference fixtures are **not supported Liberty City
regressions** and are excluded explicitly. Their presence is not validation
evidence for this port:

| Runner | Incompatible assumption |
| --- | --- |
| `test-devtools-culling.ps1` | `render/VrCullViz` module absent from this port |
| `test-heli-bullets.ps1` | Private `baseline-qbuild`, reVC revision `026cd10`, sniper and M60 paths |
| `test-modelset-wheels.ps1` | Wheel texture-transfer behavior differs from Liberty City's MODELFILE loader |
| `test-ragdoll-vehicle-hook.ps1` | Movable-pair hook and `KillPedWithCar` signature differ |
| `test-ragdoll-visibility.ps1` | Expects two population-cleanup integration sites absent in current sources |
| `test-vehicle-deformation-collision.ps1` | Snapshot-context/manifold hooks differ from current Physical.cpp |
| `test-vehicle-driveby.ps1` | Requires `Bike.cpp`, `CBike`, and bike driving paths |

These integration gaps need separate porting/audit work before those fixtures
can be promoted into the current-source suite. The suite does not claim
coverage of those behaviors. Installer/updater fixtures, shader-tool checks
and actual-mesh diagnostics below are also separate from the 41 synthetic
host suites; they are not silently treated as passing.

Actual-mesh diagnostics require the player's local, legally owned game data:

```powershell
.\tools\tests\test-vehicle-deformation-body.ps1 `
  -StageRoot C:\LCVRBuild\liberty-city-vr-build `
  -GameDataRoot 'C:\Games\Grand Theft Auto III'
```

The vehicle readers access `models/gta3.img`, `models/gta3.dir` and
`models/coll/vehicles.col`; optional Modern tests also need the locally generated
overlay. No asset is embedded in these tests or exported for redistribution.
`-UseBaseline`, where available, intentionally runs the old implementation and
can fail the fixed-behavior assertion.

The supported groups include ragdoll anatomy/contact/support, tracked aim and
melee, vehicle entry/seat transitions, wanted behavior, menu layout and vehicle
deformation geometry/lifetime. Benchmarks report host time only. Exact pass
counts are local release evidence rather than a substitute for player testing.

`test-vulkan-descriptors.ps1 -KitRoot .` extracts the production texture cache,
raster destructor and retirement queues. Its Vulkan mocks reject descriptor
rewrites and premature frees, and exercise sampler switching, mip-view changes,
two frame slots, streaming eviction, pool failure and shutdown. Run the existing
`test-mip-worker.ps1` alongside it to check the actual mip-completion invalidation
hook. These host checks do not replace Vulkan validation on a headset.

`test-xr-focus-input.ps1 -KitRoot .` exercises production OpenXR input reads,
session transition invalidation, controller-pose gating and logic-only pad
publication with a mocked runtime. It checks held controls across focus loss and
regain, failed synchronization (including positive `XR_SESSION_NOT_FOCUSED`),
inactive actions, frame-start failures and raw crouch-edge clearing.
It also checks that unavailable hand poses cannot turn neutralized input into
a physical weapon drop or synthetic trigger-release action.

`test-quest-crouch.ps1` compiles the production crouch controller and skeleton
alignment helper. It checks repeated toggles, immediate re-entry, all movement
directions, jumping and airborne recovery, simultaneous input priority, and
whole-skeleton yaw alignment while retaining the authored crouch height/pitch.
The heading cases vary pelvis yaw independently of the entity and view across
64 combinations, including the authored quarter-turn, for skinned bone matrices
and non-skinned clump axes. Degenerate hip geometry is rejected.

`test-vr-crouch-view.ps1` checks the visible-hands crouch camera at 72, 90 and
120 Hz: animated-head convergence, smooth standing recovery, stable hidden-
hands anchors, option/context reset, missing poses and once-per-frame updates.
It verifies that the camera follows the skeleton only with Hide VR Hands off.
It reuses the production knockdown position easing without imposing rotation.

`test-minimal-profiler.cpp` checks the production head-space panel fit against
both eyes' four frustum planes, including asymmetric FOVs, eye offsets, canted
views, narrow FOVs that require scaling, and invalid views. Run with a C++17
compiler and the OpenXR headers, for example:

```sh
c++ -std=c++17 -I/path/to/openxr/include -Ioverlay/android/app/src/main/cpp \
  tools/tests/test-minimal-profiler.cpp -o /tmp/test-minimal-profiler
/tmp/test-minimal-profiler
```

The graphics menu layout suite covers OFF, ON and MINIMAL labels.

`test-wheel-hand-basis.ps1 -StageRoot <assembled-source>` extracts the actual
tracked-hand render basis after the IK override and the production controller
conversion. It checks that grabbing the left car grip preserves the free hand's
finger, palm and thumb directions across independent wrist and wheel rotations,
with both floating and IK-owned hands. The wheel-locked wrist, right hand and
bike grip keep their existing behavior. The converted meshes share local
geometry and rely on the controller conversion's handedness signs; a positive
basis determinant alone cannot establish correct anatomy. These checks require
no game assets; the final visual fit still needs a headset check.

### Xbox Version HD preparation and routing

`python tools/tests/test-xbox-modelset.py` runs seven synthetic production-builder
cases without network or ADB: the Modern profile, archive precedence, full output
hashes, source pinning, allowlisted extraction, conversion failure rollback,
verified-cache reuse and official mirror/HTML-error handling. Windows uses the
bundled compressor; build `tools/modelsets/txdcompress` first on Linux.

`test-modelset-profiles.ps1` now tests Liberty City's production ModelSets and
streaming category filter, and is included in `test-current-sources.ps1`.
Its 77 checks cover complete-pack routing, ignored old category mixes, map/loose
paths, Classic fallback, restart behavior and Xbox HD calibration identity. Actual-pack
preparation and headset appearance/performance checks remain separate; see
[the asset guide](../../overlay/docs/QUEST_MODELSETS.md).

`python tools/tests/test-modelset-progress.py --powershell powershell.exe`
launches the real Windows preparation script with synthetic game sentinels.
It waits for each download/local-archive question to become visible before
sending any answer, detecting prompts hidden by the PowerShell output pipeline.
No download or device command is issued. The same test accepts PowerShell 7.

`test-installer-native-tools.ps1` exercises the production build command wrapper
under strict PowerShell error handling: successful stderr warnings, nonzero
exit codes, missing executables and caller preference restoration. Its isolated
ADB discovery checks cover the default `LCVRBuild` cache, explicit SDK precedence
and missing-SDK failures, without discovering a device or transferring data.

`test-hd-texture-loading.ps1` covers the production shared generic lookup
(model-local precedence, missing names, repeated installation and shutdown),
and the production vehicle alpha depth policy for opaque bodywork and glass.
It is included in the current-source gate. `test-modelset-profiles.ps1` now
checks two selectable profiles, legacy Xbox migration, complete-pack routing,
ignored old category mixes and the internal Xbox calibration identity.

`test-perf-capture.ps1` validates the production per-frame writer: raw timing
precision, CSV schema, event transitions, queue overflow accounting, shutdown
draining and exclusive session creation. It is included in the current-source
gate. `python tools/tests/test-perf-analysis.py` separately tests the comparison
tool's percentiles, validity masks, activity selection and incomplete sessions.
Capture tests also cover mode switching policy, vehicle speed conversion and
per-occupancy peak resets. Analysis tests cover schema 1 compatibility and schema
2 speed summaries with filtered gameplay selection.

`TestPerfCaptureFiles.java` tests the Android export provider's shared filename
and canonical-path boundary without Android dependencies. Compile it alongside
`overlay/android/app/src/main/java/com/libertyvr/quest/PerfCaptureFiles.java`,
then run `com.libertyvr.quest.TestPerfCaptureFiles` with a local temporary root.
Provider listing/read is separately verified on the personal Quest 3 device.

`test-vulkan-pipelines.ps1` executes production pipeline-key, standard warm-up
and deferred-compilation code with a mocked driver. It checks ignored opaque
blend-state reuse, independent depth-test/write coverage, strict HUD/effects
isolation, compile counts/failure reporting, state restoration and cleared
begin-frame snapshots. It is included in the current-source gate. The existing
descriptor suite continues to check fence-based resource lifetime safety.
Recorder/analysis tests cover schema 3 begin-frame columns, validity and stall
attribution while retaining schema 1/2 compatibility. Host tests do not measure
on-headset compilation latency, startup time or the resulting frame pacing.


`test-renderdoc-export.py` checks capture listing validation, rejection of empty
or truncated/error transfers before creating placeholders, traversal rejection,
verified writes and protection of existing local captures. `TestPerfCaptureFiles`
also covers the .rdc provider boundary and rejection of arbitrary game files.

`test-player-traversal.ps1` compiles the production traversal controller with
host engine adapters. It checks normalized movement and boost across frame
rates, collision mode combinations, ground preservation, terrain model
classification, pause/cutscene/vehicle/death guards, collision restoration and
Town Hall recovery. World collision and streaming are adapters; map traversal
and headset comfort still require in-headset verification.
