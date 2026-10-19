# Host regression tests

These tests compile production helpers or extract production functions into
small native adapters. They check code behavior; they do not launch the game,
replace game data or establish headset appearance/FPS.

Windows tests require Visual Studio 2022 or Build Tools with the Desktop
development with C++ workload. Run from a disposable source-kit copy because
some extraction runners write temporary `.inc` files beside the test source.
Those files are ignored and must not be included in a release archive.

`test-autosave.ps1 -StageRoot <assembled-tree>` tests the production autosave
transaction and extracts the engine filename/checksum/recovery functions. It
injects failures at each transaction boundary, checks that manual slots remain
unchanged, tests first save and backup recovery, rejects truncated/checksum-bad
files and invalid block lengths, and checks both load-menu layouts. All save
bytes are synthetic; no game assets or player saves are accessed. These checks
do not replace mission-completion and load testing on a headset.

The header-only ragdoll suite works directly against the kit:

```powershell
.\tools\tests\test-ragdoll.ps1 -KitRoot .
```

Tests that extract patched engine functions require a freshly assembled re3
tree via `-StageRoot`, `-KitRoot` or `-EngineRoot`, as declared by that runner.
Some historical comparison fixtures additionally require a local
`baseline-qbuild` subdirectory containing the comparison revision; that tree is
not distributed. `test-heli-bullets.ps1 -OriginalRoot` needs a clean git checkout
of the documented upstream commit. Read each runner's parameter block before
invoking it.

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

Useful groups include ragdoll anatomy/contact/support, tracked aim and melee,
vehicle entry/seat transitions, wanted behavior, menu layout and vehicle
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
