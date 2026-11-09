> Tool migration: standalone `.ps1`, `.sh` and `.py` entrypoints are now embedded
> manager modules. Open **Tools and tests**, or run `dotnet run --project
> tools/manager -c Release -- --tool MODULE -- ARGUMENTS` from the repository root.
> Legacy filenames below identify resource modules; old direct shell invocations
> are historical. Use the manager pages for build/install, editor build, music,
> models, repair and update. See [the manager guide](../manager/README.md) for runtime requirements.
# Host regression tests

`test-scope-overlay-depth.cpp` compiles as C++17 with three extracted production
includes from `librw/src/vulkan/vkim.cpp`: `scope-overlay-depth-production.inc`
contains `const bool32 scopeOverlay` through `gstate.zWriteEnabled = savedZWrite;`;
`scope-overlay-strict-production.inc` contains the complete
`getImmediate2DStrictDepth` function; `scope-overlay-surface-production.inc`
contains the `push.surfaceProps[0]` assignment. It exercises 64 combinations of
scope/Im2D/Im3D, inherited depth states, radar masking and pipeline failure,
checking overlay depth selection and state restoration. No game data is used.

`test-scope-overlay-stereo.cpp` compiles directly as C++17 and exercises the
production scope plane math. It checks 216 per-eye projections over target
distances, moving reticles, head rotations, asymmetric frustums and zoom levels,
plus angular size and targets behind the viewer. No game data is used.

`test-scope-overlay-mask.cpp` executes the sniper mask rectangle submissions.
Extract the block from `const float maskLeft` through the last `DrawRect` before
`rect.left = rLeft` in the assembled `src/renderer/Hud.cpp` into
`scope-overlay-mask-production.inc` on the include path and compile as C++17.
It checks opaque black coverage beyond all four HUD edges, a clear moving
aperture, square/wide displays and desktop coverage across 72 layouts. No game
data is used; final stereo appearance still requires headset QA.

`test-particle-frame.cpp` compiles as C++17 with `overlay/src/renderer` on the
include path. It executes the production particle frame resolver over Xbox
fireball/flame ranges, six-texture explosions, native Classic sequences,
explicit out-of-range frames, reversed ranges and integer limits. Xbox ranges
that exceed the available raster array are mapped across that array; every
lookup is bounded, including single-raster effects. The renderer derives array
capacities from the actual raster arrays. Existing Xbox asset installations
need no reconversion. This adapts animation to re3's available textures; it
does not implement additional console animation textures. No game data is used
by this test. Repeated vehicle explosions still require headset QA.

`test-jump-heading.ps1 -StageRoot <assembled-tree>` compiles the production Quest
jump-heading helper, walk-direction calculation and launch-impulse branch. It
checks standing and moving jumps after physical quarter/half turns, independent
camera/entity headings, steep downward gaze, both native camera paths and
NPC/vehicle/tracking gates. Source integration checks require facing updates at
the jump request and before takeoff collision probes. No game data is read;
actual jump direction and obstacle clearance still require headset QA.

`test-physical-turn-input.ps1` compiles the production on-foot input gate and
head-relative stick transform. It covers tracked sniper held/unheld, physical
quarter/half turns, body/head movement modes and gameplay/tracking exclusions.
`test-head-attachment.cpp` also covers physical and thumbstick facing during
idle/jump: stabilized horizontal neck position, authored vertical jump motion,
eye calibration, unchanged native animation and a single application of HMD yaw.
These checks read no game data; body/hand alignment and sniper locomotion still
require headset QA.

`test-quest-surrender.cpp` compiles as C++17 with `overlay/src/vr` on the
include path. Extract `static void UpdateSurrender(bool blocked)` from
`QuestWeaponVR.cpp`, ending before the following `} // namespace`, into
`quest-surrender-production.inc` on the include path. The fixture executes
the production gesture and officer scan: three-second holds, shooting and
punching officers, wanted/ignore flags, officer target/state/range, obstructed
sight, unavailable officers, control/vehicle/menu/tracking gates, interrupted
holds, suspended frames, timer rollover and duplicate arrest prevention.
No game data is read. In-game gesture comfort, native Busted presentation and
police-station restart still require headset QA.

`test-shop-transition.cpp` compiles with the assembled `src/control` include
path, with and without `__ANDROID__`. Extract the `shopTransitionParams`
block from `Script.cpp` (ending before `USE_ADVANCED_SCRIPT_DEBUG_OUTPUT`)
into `shop-transition.inc` and add that directory to the include path. It
executes the production dispatch guard for both shop scripts: teleports,
heading changes, control freezes, camera resets and staged doorway clearing
consume their arguments without performing the action on Quest. Two crossings
exercise entry and exit, while clerk, purchase-state and other gameplay
commands still dispatch. Mission and desktop behavior remain covered. No game
data is read. Seamless traversal and purchases still require headset QA.

`test-ambient-camera.cpp` compiles with and without `__ANDROID__` and checks
the production bathroom script policy, normalized/new/restored thread names,
all mission flags, unrelated scripts and bounded non-terminated names. The
Quest dispatcher yields before reading an opcode from the ambient `camera`
thread; this suppresses its fixed views, fades, control freezes and staged
repositioning together. Retail saved ambient threads have the mission-launch
bit set, so that bit must not exclude the bathroom thread. An explicit
regression covers that saved-state combination through repeated dispatches.
Desktop and actual mission-flagged scripts retain their behavior.
The test reads no game data; headset traversal still requires in-game QA.
It also checks both Ammu-Nation thread names, mission isolation and exact name
matching: fixed camera setup/takeover and fades are bypassed while the shop scripts
continue to run. Shop entry, turning, purchases and exit need headset QA.
`test-shop-camera-dispatch.cpp` executes the production `Script2.cpp` cases
for `POINT_CAMERA_AT_PLAYER`, `SET_FIXED_CAMERA_POSITION`, `DO_FADE` and
`GET_FADING_STATUS`, extracted into `shop-camera-dispatch.inc`. Compile with
that include directory, with and without `__ANDROID__`. It verifies camera
takeover and both fade directions are suppressed only for the shops on Quest,
and all opcode parameters are still consumed. Other callers retain fade timing
and direction; genuine fades from other callers still report as active.

`test-stair-camera.cpp` compiles with the assembled `src/core` include path,
with and without `__ANDROID__`. It exercises the production zone queries over
768 combinations and repeated zone entry/exit: Quest ignores the fixed stair
camera while desktop retains it, and rain, collision and other zone attributes
remain intact. No game data is read. Safehouse stairs and physical/stick
turning still need headset QA. The bathroom also has a separate ambient script,
covered by `test-ambient-camera.cpp` above.

`python tools/tests/test-car-windows.py <assembled-tree>` compiles the production
immersive car-window gates, toggle persistence, Vulkan mesh filter and renderer
state restoration. It checks third-person/default/motion isolation, other cars,
opaque bodywork, windscreen atomics, and both Vehicles pages across 108 vehicle,
view and seat contexts (at most 12 visible rows). Without an assembled tree it
runs the source-only gates, mesh filter and menu cases. Requires a C++17 compiler;
no game data or device is used. Window appearance still requires headset QA.

`test-siren-input.cpp` compiles directly with a C++17 compiler and exercises
the production Quest siren input helper. It checks double taps at 30–120 FPS,
the 500 ms gap boundary, held horns, repeated taps, control/vehicle changes,
missed player updates and clock wrap. Both L3 and virtual wheel horns use this
handler; physical horn interaction still requires headset validation.

`test-garage-exit.ps1 -StageRoot <assembled-tree>` extracts and compiles the
production garage outside test in Android and desktop configurations. It checks
that a room-scale headset still inside the garage blocks delivery even when
Claude's collision body is outside, all four exit directions, caller clearance
margins, tracking fallback, other entities and collision sphere overlap. It reads
no game data. It also executes the production camera registration block and
mission closure interlock: Quest garages retain the player camera, side/back
rectangle exits cannot accept a delivery, both body and eye must clear an actual
doorway, and returning inside during closure reopens the door and restores
controls. Desktop camera/closing behavior remains covered. Replay Kenji's Grand
Theft Auto deliveries on the headset to verify door behavior in the mission.

`python tools/tests/test-cutscene-cameras.py` compiles the production camera
lookup and render selection with synthetic original and skinned actors. It
checks named head-frame detection, actor/director switching, unavailable heads,
streaming guards, and live-world vehicle occupants. It requires a C++17 compiler
(Visual Studio on Windows); it reads no game data. Headset appearance and physical
R3/calibration controls still require in-game validation.

`test-boat-window-depth.ps1 -StageRoot <assembled-tree>` executes the boat
body submission block with the production Vulkan vehicle-alpha policy. It
checks water visibility through all transparent material alpha values, opaque
hull depth, the wheels-only cheat, and state restoration before the hull water
mask and subsequent world draws. In-game appearance still needs headset QA.

Device discovery, explicit target routing, log redaction and save/music provider
boundaries are covered by the active C# manager checks using fake devices. The
old script installer diagnostics tests were removed with those installers.

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
It also checks left/right steering ownership in both modes, suppression with
both hands steering, and held-trigger/contact transitions after wheel release.
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
runs 67 suites against that tree or the kit's production helpers. Each suite
runs in a separate process. Child stderr cannot abort failure accounting on
Windows PowerShell; failed suites are retained in the final report. `results.json` records exit codes, log paths, totals
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

`test-gta3-ragdoll-rig.ps1 -StageRoot <assembled-tree>` compiles the production
GTA III rigid/skinned adapter and checks unique model-matrix ownership, virtual
neck/clavicle/missing-spine joints, invalid-pose rejection and world/local
conversion under a rotated entity root. It also checks the engine presentation,
bullet exclusion, bounds, deformation update and geometry-release hooks.
The same runner compiles `test-ragdoll-recovery.cpp` using the production
`SetFall`, `Fall`, evade-completion and getup-completion callbacks. It checks
that evasion/cover/getup animations are not taken over, their finite deadlines
reach native getup and restored AI, genuine later impacts still qualify, and
script-controlled indefinite falls retain native behavior. The explosion
fixture separately exercises live admission and first-getup-frame pose handoff.
`test-ragdoll-getup.cpp` in the same runner executes the full production
`SetGetUp` and finish callback with the production standing-clearance query.
It covers stale/side contact through all native modulo phases, rotated and
moving cars, low chassis and head-height obstruction, immediate retries,
bounded candidate overflow, unsupported geometry and the unchanged legacy gates.
Optional `-GameDataRoot <local-GTA-III-folder>` reads `male01`, `cop` and `b_man1`
from the user's local IMG/DIR into memory and exercises their actual frame
hierarchies. No game assets are embedded, uploaded or exported by the test.
Invoke this runner through the manager's `--tool` interface above.

`test-ragdoll-vehicle-hook.ps1` and `test-vehicle-deformation-collision.ps1`
are active GTA III regressions. They execute the native movable-pair contact
loop in both processing orders and all ten accepted collision-response sites,
including manifold-pose rollback, opposite inward normals and stationary cars.

Tests that extract patched engine functions require a freshly assembled re3
tree via `-StageRoot`, `-KitRoot` or `-EngineRoot`, as declared by that runner.
Optional `-UseBaseline` modes require a private historical snapshot and are
outside the current-source run. Read each runner's parameter block before
invoking it.

The five previously excluded reference-only suites (`devtools-culling`,
`heli-bullets`, `modelset-wheels`, `ragdoll-visibility`, `vehicle-driveby`) and
associated C++ fixtures have been removed. Their assumptions were incompatible
with this port (missing `render/VrCullViz`, private reVC baselines/M60/bikes,
different MODELFILE behavior or absent population-cleanup sites). No coverage is
claimed for those behaviors. Active GTA III regressions with adapter failures
remain available; see [manager validation](../manager/VALIDATION.md).

Superseded script installer/updater, old save converter, privacy script and
HD-pack preparation tests were removed with the implementations they exercised.
Use `dotnet run --project tools/manager/tests -c Release` for the active native
manager, save, privacy, music and retail ISO workflows. See
[the tool inventory](../README.md) and [notices](../../NOTICE.md) for prerequisites.
Asset diagnostics and optional baseline modes remain separate from synthetic
host suites and are never silently counted as passing.

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

### Model-set routing

`test-modelset-profiles.ps1` tests Liberty City's production ModelSets and
streaming category filter and remains in `test-current-sources.ps1`. Its checks
cover complete-pack routing, ignored old category mixes, map/loose paths, Classic
fallback, restart behavior and retained calibration identity. Native manager
Xbox checks and the converter CTest cover the current retail ISO workflow;
retired HD-pack downloader tests are not validation for that workflow.

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

`test-quest-melee.ps1 -StageRoot <assembled-tree>` compiles the production
motion detector for fists, bats and all held gun types. It checks speed/travel
strength limits, slow follow-throughs, both hands, multiple frame rates,
locomotion compensation, one-contact latching and pending-strike cancellation
on tracking loss, support grip, vehicle entry and gameplay restriction. Guns
retain native firing/ammo coverage in `test-weapon-cadence.ps1`. In-headset
checks are still needed for reach, comfort and physical impact feel on peds
and cars; host fixtures do not simulate the full collision world.

`test-culling-visualizer-api.ps1 -StageRoot <assembled-tree>` compiles the
production Liberty view predicate, developer classifier and snapshot accessors.
It checks OFF/per-view gates, stereo box and sphere queries, mode margins, small
map-building bounds, accessor bounds and camera snapshot copies. The visualizer
reports view rejection and unlisted entities; Vulkan geometry occlusion has no
per-entity classification in this tool. No game data is read. Frozen-frame colors
and head-turn behavior still require headset QA.

`test-visualization-debug.cpp` compiles directly as C++17 and executes the
production debug-view defaults/reset, all mesh/shading combinations, and line
index generation for lists, strips, degenerate connectors, material boundaries,
short inputs and the largest 16-bit vertex index. No game data is read.
`test-vulkan-pipelines.cpp` also verifies that overdraw gets a separate pipeline
key for meshes while preserving HUD and immediate-line keys. The graphics menu
layout adapter renders every visualization selection and shading view in both
shipping and developer configurations and checks labels/highlights/footer bounds.
See `overlay/docs/QUEST_VISUALIZATION_DEBUG.md` for semantics and headset QA.

The Vulkan pipeline adapter also runs `test-visualization-render.cpp` against
extracted production edge queue/replay and depth/blend policy code. It checks
that hidden edges are deferred until depth completion; skinned/material state
is captured by value; cold pipelines and aborted frames are safe; ordinary
wireframes draw immediately; and tested overdraw preserves depth while xray
overdraw disables it. Pipeline-key tests isolate depth-only fills, tested
accumulation, wire lines and UI. Menu layout covers all geometry/shading pairs.
