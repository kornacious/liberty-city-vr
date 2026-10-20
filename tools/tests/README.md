# Host regression tests

These tests compile production helpers or extract production functions into
small native adapters. They check code behavior; they do not launch the game,
replace game data or establish headset appearance/FPS.

Windows tests require Visual Studio 2022 or Build Tools with the Desktop
development with C++ workload. Run from a disposable source-kit copy because
some extraction runners write temporary `.inc` files beside the test source.
Those files are ignored and must not be included in a release archive.

Run the supported Liberty City suites together from a normal PowerShell session:

```powershell
.\tools\tests\test-current-sources.ps1 -Re3Root C:\LCVRBuild\re3-public-b050e1c
```

Use the clean GTA III/re3 upstream described in BUILDING.md. The runner rejects
reVC input, assembles the current kit and all patches in a new directory, and
runs 35 suites against that tree or the kit's production helpers. Each suite
runs in a separate process. `results.json` records exit codes, log paths, totals
and exclusions; any failed suite makes the command fail after writing the
report. Excluded scripts never count as passes. No game assets, headset or
private historical checkout is needed. Use `-OutDir` with a new directory to
choose where the assembled sources, binaries and logs are kept.

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
unchanged production wheel and suspension functions in constrained synthetic
rigs. It runs 297 fresh-process combinations of nine scenarios, 15–144 FPS and
steady/uneven/stalled pacing, writing `measurements.csv` and source hashes to
its output directory. Passing means smoke checks and finite bounded states;
it does not mean handling is FPS-independent. See
[the detailed findings](../../WHEEL_SUSPENSION_FPS_REPORT.md) for rig assumptions,
measured differences and limits. No original game data is used.

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
| `test-modelset-profiles.ps1` | reVC streaming region and `CdStreamPosix.cpp` registration |
| `test-modelset-wheels.ps1` | Wheel texture-transfer behavior differs from Liberty City's MODELFILE loader |
| `test-ragdoll-vehicle-hook.ps1` | Movable-pair hook and `KillPedWithCar` signature differ |
| `test-ragdoll-visibility.ps1` | Expects two population-cleanup integration sites absent in current sources |
| `test-vehicle-deformation-collision.ps1` | Snapshot-context/manifold hooks differ from current Physical.cpp |
| `test-vehicle-driveby.ps1` | Requires `Bike.cpp`, `CBike`, and bike driving paths |

These integration gaps need separate porting/audit work before those fixtures
can be promoted into the current-source suite. The suite does not claim
coverage of those behaviors. Installer/updater fixtures, shader-tool checks
and actual-mesh diagnostics below are also separate from the 35 synthetic
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
