# Conservative opaque ordering experiment

Implemented locally on 2026-10-30. This is an A/B experiment, default OFF;
no performance improvement or runtime visual equivalence is established yet.

## Scope and safety policy

VR overlay → Graphics → **Opaque Order Experiment** toggles immediately and
persists in `VR/OpaqueOrderExperiment`. Keep **Opaque World Fast Path** ON.

Road and non-road submissions each use a local copy of the visible pointer
list. Within fixed groups of at most 32 slots, adjacent buildings may exchange
positions only when their entire geometry bounds are separated in depth in
both eyes, with a 0.0001 normalized-depth margin. Overlapping, near-equal,
near-plane-crossing and invalid bounds retain their order. The global visible
list is unchanged. This is an insertion partial order, not a `std::sort`
comparator.

Eligibility requires a non-fading, non-draw-last, non-damaged, non-scorched
building atomic with default callbacks/pipeline, previously instanced static
single-morph geometry, LIGHT unset, no pending reinstance, and at most 64
meshes. Every mesh must have opaque vertices/material/texture, ordinary texture
addressing and no MatFX. Custom callbacks, mixed-alpha atomics, vehicles,
pedestrians, props and entries from the other render phase are barriers.
Road-group visualization disables ordering. Bounds are collected during
existing instancing, not by rescanning vertices each frame; unknown/custom
instances start with invalid bounds. First-use geometry stays in original order.

The experiment does not disable alpha testing/blending/depth writes or move
masked geometry into a separate global pass. It does not promise to restore
LRZ writes after the first masked draw. Strict barriers can limit swaps; the
recorded counters reveal a no-op experiment and the CPU overhead must be
included when judging results.

## Capture data

Session metadata records the initial toggle and policy version. Optional
appended frame columns preserve compatibility with older captures:

- `opaque_order_experiment_enabled`
- `opaque_order_candidates` (backend classifier calls)
- `opaque_order_eligible` (successful proofs)
- `opaque_order_swaps` (adjacent exchanges, not saved draws)

Counters reset each frame with existing atomic-submission counters. The analyzer
reports configuration, per-frame distributions and frames with swaps; older
captures report no opaque-order summary. Existing render-phase CPU timings
include remapping/classification/sorting work.

## Validation

- Host C++ ordering tests passed under MSVC `/W4 /WX`, including 500 randomized
  sequences checking permutation and pairwise order preservation, barriers,
  overlapping intervals, disagreeing eyes, tolerance and invalid depths.
- Recorder tests passed: numeric serialization, queue accounting, shutdown and
  exclusive capture handling, including all new fields.
- Six Python analysis tests passed, including new configuration/counter handling
  and absent-column compatibility. Initial execution hit sandbox temporary-file
  permissions; rerunning with TEMP/TMP inside the ignored workspace cache passed.
- Full arm64 Quest debug APK build passed after fixing the default pipeline
  lookup to use `Atomic::getPipeline()`. Existing shader precision and Gradle
  deprecation warnings remain.
- APK installed with `adb install -r` on the verified **personal Quest 3 device**.
  Existing game data and saves preserved; read-only diagnostics accessible.
- Runtime image correctness and performance benefit await user replay runs.

## Controlled benchmark

Run the same demo **OFF → ON → OFF → ON**, normal game launch, no RenderDoc
attachment or automatic capture. Keep 1512 × 1584 base resolution, 120 Hz,
Forward View ON, opaque world fast path ON, and other fixed settings unchanged.
Use minimal profiling. Reboot/cool before the sequence and avoid radio changes.
Check for missing geometry, changed transparency or flicker in both eyes.

Compare scene GPU distributions and the busy 35–40 second window, CPU render
timings, frame intervals above budget and eligible/swap counters. Retain only
if repeatable gains outweigh CPU cost and visuals remain correct. The two clean
native-postprocess runs from the preceding analysis remain the historical
baseline; the interleaved OFF runs control for conditions in this build.
