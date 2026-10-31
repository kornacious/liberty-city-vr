# Recording and comparing performance

In **Graphics → Performance Profiler (FPS)** select **MINIMAL** for FPS/frame time
only, or **ON** for the full diagnostic display. **Both modes record the same
performance metrics.** Switching ON ↔ MINIMAL preserves one continuous session,
frame IDs, events and vehicle-speed peaks. Select **OFF** after the route to
drain accepted samples and close the files; only OFF stops recording.

Recording keeps the existing one-second `quest_perf.csv` and also writes
a unique pair of files for each session under:

```text
/sdcard/Android/data/com.libertyvr.quest/files/perf-captures/
    <UTC timestamp>-<monotonic timestamp>.frames.csv
    <UTC timestamp>-<monotonic timestamp>.session.csv
```

Copy both files locally using the headset file manager or local ADB workflow.
Android scoped storage can deny direct `adb pull` even for a debug APK. The app
therefore exposes a read-only ADB bridge: query
`content://com.libertyvr.quest.performance/captures` to list names/sizes, then
read `content://com.libertyvr.quest.performance/capture/<filename>` using
`adb shell content query` / `adb exec-out content read`. Select the verified
device locally and keep its identifier out of logs. Only the app, root and ADB
shell can use this provider; only generated frame/session CSV names are accepted.
The provider cannot modify captures or expose saves and game assets. Installing
the export update preserves recordings made by the earlier capture build.
New sessions never overwrite old pairs. `quest_perf.csv` retains its existing
overwrite-on-enable behavior. Captures are excluded from Git. They include
gameplay coordinates, but no device identifiers or original game assets.

## Recorded data

- Raw timings for every completed profiled app frame: monotonic start, interval
  between frame starts, local CPU time and pre/begin/step/post/end timings.
  The first interval is `-1`. Canceled frames leave frame-ID gaps.
- Raw engine phases and `phase_valid_mask`: bits follow phase columns in header
  order, from `game_ms` at bit 0 to `render_effects_ms` at bit 23. An unset bit
  means unobserved, not free work. Raw phases retain stalls over one second,
  unlike the legacy overlay's smoothing filter.
- Separate latest runtime CPU/GPU and Vulkan GPU values; `-1` means unavailable.
  GPU observations are asynchronous, can repeat and are **not attributed to that
  row's CPU frame**. Their measurement scopes differ; do not add them together.
- Actual render dimensions, effective scale and reported refresh rate.
- Per-frame active world/vegetation/vehicle/pedestrian/weapon selections:
  `0=Classic`, `1=Modern`, `2=legacy Xbox identity`. Requested selection is separate
  because model changes require a restart.
- Player presence, ped/movement activity, health/armour, wanted level, weapon/ammo,
  vehicle model, position, engine speed, and pause/cutscene/on-mission flags.
  `speed_game_units` is the engine speed magnitude, not m/s. Activity reflects
  engine state, not a headset-motion classifier.
- Schema 2 adds actual vehicle velocity: `vehicle_speed_kmh` is horizontal speed,
  while `vehicle_speed_3d_kmh` includes vertical motion. Both use the vehicle's
  move-speed vector, not the seated player's. Conversion is engine magnitude ×
  180, the inverse of re3's km/h-to-50-Hz handling conversion. These are nominal
  simulation speeds, not wall-clock travel speeds during slow motion.
- `vehicle_peak_kmh` is the highest observed horizontal speed for the current
  vehicle occupancy during this capture. It resets on exiting/changing vehicles
  or starting a new capture, but survives switching ON ↔ MINIMAL. It is not a
  previously saved lifetime record or a guarantee of maximum achievable speed.
- `vehicle_handling_cruise_kmh` and `vehicle_transmission_max_kmh` are the
  converted runtime handling cruise estimate and transmission maximum. The
  engine derives them from handling/drag; they are not the original unconverted
  handling-file value or guaranteed road top speed. Slopes, collisions, tuning
  and cheats can change achieved speed. Vehicle values are `-1` when unavailable.
- `profiler_display_mode` records `1=ON` / `2=MINIMAL` per frame, so display
  changes can be identified without splitting the session.
- Entity-render call counts for vehicles, pedestrians and static objects.
  These are **not** Vulkan draw-call counts or final visible-object counts.

Session metadata includes readable asset names/source labels at capture start,
game version, profiler compilation time, UTC/monotonic clock anchors and selected
saved graphics values. Early startup metadata can precede model initialization;
per-frame active selection is authoritative. Asset names describe configured
routing, not content hashes or proof that base files are unmodified. `saved_*`
values can be `-1` when absent and can differ from active settings. Toggle
profiling OFF and back to ON or MINIMAL after changing settings for fresh
metadata. The analyzer supports schema 1, 2 and 3 recordings.

## Vulkan begin-frame investigation (schema 3)

MINIMAL and ON additionally capture synchronous CPU timings for pipeline
compilation, frame-slot fence waiting, timestamp readback, retired-resource
destruction, wrist/scope-target preparation and remaining backend setup.
`vk_begin_detail_valid=0` means unavailable; canceled/failed/skipped begins must
not reuse the previous frame's measurements. Timing is collected only while
profiling is enabled. These sections measure CPU wall time, including waiting
and scheduling interruptions, rather than GPU pass durations.

`vk_begin_backend_total_ms` covers librw's beginFrame. The older `vk_begin_ms`
also covers its app wrapper, so they can differ. The analyzer exposes the
nonnegative difference as `vk_begin_outside_backend_ms` in stall reports.
Setup is the residual backend time after the separately measured sections.

Counts identify pending pipeline requests, newly created pipelines, creation
failures, retired buffers/images/descriptors, upload command buffers released
and first-use framebuffer creation. `vk_last_pipeline_key` is the last pending
pipeline state key processed, not a device identifier or a unique frame ID.
The comparison tool reports the ten largest selected begin-frame intervals
over 5 ms, with section timings and counts. Older schemas report these fields
as unavailable, not zero.

The first begin-frame optimization pass warms standard depth-test/depth-write
combinations independently and reuses opaque pipeline keys when only ignored
blend factors change. An in-memory Vulkan pipeline cache shares driver results
within the device lifetime, with an uncached fallback if creation fails. No
cache binaries are persisted. More variants are prepared at startup, trading
startup work and pipeline memory for fewer standard first-use compilations.
The existing frame fences, retirement safety and render-pass compile deferral
remain in place. New captures must establish which section explains the measured
37–41 ms stalls and whether their frequency/duration improves.

Coarse streaming/scripts/population/car/world-process/particle timers now have
producer hooks. Detailed physics channels without producer hooks remain
unsupported; the legacy summary still provides other existing diagnostics.

## Player event flags

| Value | Meaning |
|---:|---|
| 1 | Observed health or armour decrease |
| 2 | Entered dying/dead state |
| 4 | Alive after an observed death, including across temporary player absence |
| 8 / 16 | Entered / left a vehicle |
| 32 | Successful weapon activation in instrumented player Fire/drive-by paths |
| 64 / 128 | Player appeared / disappeared |
| 256 | Moved over 50 game units between adjacent valid player samples |
| 512 / 1024 / 2048 / 4096 | Pause / cutscene / mission / wanted state changed |

`event_mask` can contain several flags. `weapon_uses` counts successful
activations, not pellets, damage or hits. Direct physical-melee queries are not
counted; melee/attack ped states are still recorded. State changes occurring and
being undone within one frame can be missed. Recovery is not proof of hospital
respawn, and relocation can also be loading, teleportation or scripted movement.

## Before/after comparison

### Stereo world occlusion experiment

Open **Graphics -> Optimizations -> World Occlusion**. `VR/WorldOcclusion`
defaults OFF and applies immediately. The menu makes this experiment mutually
exclusive with World Backface Cull: enabling either turns the other OFF.

This CPU software occlusion pass rebuilds two 48 x 48 depth grids from the
current frame's loaded, visible, solid static-building triangles. Collision
boxes are used only for prioritization, never as opaque occluders. Each cell
must be entirely covered by an actual triangle with a silhouette margin;
partial triangle coverage cannot fill a window or fence gap. An affine depth
plane stores a conservative far depth for that whole cell. An occludee uses
its selected render geometry's complete vertex AABB, expanded on screen and
compared at its nearest depth. It is skipped before default world mesh draw
submission only if all relevant cells are nearer in **both** eye grids.

There is no GPU depth readback and no prior-frame visibility reuse. Projection
or camera-fold changes invalidate queries; near/far-plane crossings, missing
instance data, fades, dynamic/unknown owners, unsupported callbacks/topologies
and uncertain cases keep their original rendering. Occluders exclude alpha
textures, vertex/material alpha, additive/no-Z-write/draw-last/scorched/damaged
entities and non-default pipelines. The first loaded frame may lack eligible
instance data and simply draws normally. Main world view only: alternate
renderers, scope/wrist passes, temporal rendering, reflections/modern water,
active dynamic-light effects, debug replacement views and backface-cull mode
fall back to original rendering. The implementation targets the normal fixed
benchmark settings; it does not claim support for every render configuration.

Work is bounded to 128 prioritized entity candidates, up to 24 contributing
occluders, 1024 sampled source triangles (at most 64 per entity), 8192 raster
cell visits and 16384 query cell visits per app frame. Incomplete coverage and
exhausted budgets reduce rejection rather than hiding uncertain geometry.
This first version deliberately samples a subset of real opaque triangles;
selected triangles remain exact and are not replaced by filled bounding boxes.

Frame CSVs append `world_occlusion_enabled`, builds, occluder candidates,
contributing occluders, source triangles, cell updates, queried atomic calls,
rejected/visible calls, invalid/budget counts, coverage per eye, raster/query
cell visits, avoided input indices/vertices, and build/query CPU milliseconds.
These are input-work estimates, not measured GPU vertex invocations, unique
object counts or compositor statistics. Indexed geometry shared by multiple
passes can appear repeatedly; skipped alpha split passes are not multiplied
into the input estimates. Metadata records the starting state and policy.
The analyzer summarizes the new counters and includes the toggle in settings.

Replay **OFF -> ON -> OFF -> ON**, keeping World Model LOD AGGRESSIVE, World
Backface Cull OFF, Static Instancing and all remaining settings fixed. Use a
normal launch without RenderDoc/auto-capture. No special cache warm-up is
needed: the depth grids are rebuilt every frame. Watch both eyes for missing
walls, holes, side views and fast-turn disocclusion. The experiment must save
more submission/GPU work than its measured CPU cost to be worth enabling.
Host tests and compilation do not establish device visual correctness or a
performance gain; those remain to be checked with the controlled recordings.

### Guarded world backface culling experiment

Open **Graphics -> Optimizations -> World Backface Cull**. The persisted
`VR/WorldBackface` toggle defaults OFF and applies immediately. It selectively
uses backface culling on opaque static-building geometry with closed,
consistently outward-wound components. This is rasterization culling, not
whole-object occlusion: input draw calls and vertex shading are not removed.

The guard rejects open/inverted/non-manifold/degenerate meshes, alpha textures,
vertex/material transparency, fading objects, MatFX, non-default pipelines,
vehicles/peds/unknown owners, reflected or near-singular transforms, custom
scope/wrist/effect rendering and uncertain state. Exact-position seam welding
does not bridge real gaps. Certification is bounded to 8192 vertices and
24576 triangle-list indices; oversized and allocation-failed cases keep the
original rendering. Geometry reinstancing invalidates the cached result.
The head must be outside a conservatively transformed vertex bound plus a
0.25-unit eye margin. Cull state is restored immediately after choosing each
pipeline, including instanced pipelines.

Capture metadata records `world_backface_at_start` and the policy version.
Frame CSVs append `world_backface_enabled`, `world_backface_candidates`,
`world_backface_draws`, rejection reasons (`state`, `geometry`, `view`,
`transform`), `world_backface_certifications`, `world_backface_indices` and
`world_backface_certify_ms`. Candidates/draws count considered mesh-pass draws;
indices count input indices of accepted draws, not culled triangles. Rejection
reasons include atomic-level guards, so they do not partition the mesh-pass
candidate count. Certification time measures first-use CPU work independently.
The analyzer includes the toggle in configuration and summarizes these fields.

Use one ON warm-up playback to populate geometry certificates, then compare
**OFF -> ON -> OFF -> ON**, holding World Model LOD AGGRESSIVE, resolution,
refresh rate, Forward View and every other experiment fixed. Keep Static
Instancing at the same setting as the preceding LOD comparison (ON there).
Use ordinary launches without RenderDoc or auto-capture. Inspect both eyes for
missing walls or interiors; certification is a conservative filter, not device
visual validation. Retain the warm-up capture separately and exclude it from
the four comparison runs. No performance gain is assumed before measurement.

### World model LOD and optimization menu

Open **Graphics → Optimizations → World Model LOD**. This persisted four-state
control (`VR/WorldLodMode`) affects simple/time world models; vehicle and ped
callback LOD and traffic generation keep their existing behavior.

| Mode | Value | Selection policy |
|---|---:|---|
| OFF | 0 | Highest-detail atomic in the correct intact/damaged family; LOD entities suppressed. |
| ON (default) | 1 | Existing authored selection with the current VR minimum 2.2× distance multiplier. |
| PERFORMANCE | 2 | Approximately 1× authored switching distances, with related high-detail/LOD handoffs capped at the normal 300 m sector range. |
| AGGRESSIVE | 3 | Earlier switching, smooth mesh handoffs, and earlier fading/hiding of unpaired detail. |

AGGRESSIVE (value 3) uses the PERFORMANCE handoff distances but also limits
unpaired high-detail world models to their authored final distance (at most
300 m). Those models fade out over the following 20 m, and fade back in when
approaching. Existing low-detail world entities retain their far coverage.
Internal mesh changes and related high/LOD handoffs crossfade over 20 m;
closely spaced internal thresholds shorten that band to avoid overlaps.
Distance weights work in either travel direction. Missing streamed variants
cannot be crossfaded; during an internal transition the resident variant is
kept solid, and related LOD coverage is retained while detail is loading.
This mode deliberately removes distant unpaired scenery and may expose gaps
behind it; it is an experimental quality/performance tradeoff. Transparent
crossfades temporarily submit both meshes and can increase blending cost.
Compare PERFORMANCE -> AGGRESSIVE -> PERFORMANCE -> AGGRESSIVE under otherwise
identical benchmark settings. Device visual/performance validation is required.

PERFORMANCE retains the far visibility range for LOD entities and for models
without replacement partners. It changes world geometry transitions rather than
reducing traffic generation distance. It is PS2-like in using earlier authored
distances; it does not claim exact PS2 camera or asset parity. Time-of-day checks
remain in the renderer, and paired time variants share a handoff distance.
Damaged atomics are not treated as low-detail alternatives.

OFF extends paired high-detail visibility/scanning to the camera's far range
and requests detailed assets through normal streaming. LOD-only scenery with no
high-detail counterpart is hidden: OFF does not fabricate replacement geometry.
Its larger scan/streaming workload is intentionally a quality/control mode.
Missing high-detail residency can leave temporary gaps. The submenu displays
the LOD-only limitation. ON remains the initial/default behavior.

The five renderer controls (Opaque World Fast Path, Masked Alpha Pass Skip,
Opaque Mesh Batch, Static Instancing and Opaque Order Experiment) now live in
the same Optimizations submenu. Their saved values and backend behavior remain
independent. Back/B from this submenu returns to Graphics.

Sessions record `world_lod_mode_at_start` and a policy string. Frames record
`world_lod_mode`, high/lower atomic selections, LOD-entity selections, suppressed
LOD calls, missing selections and linked/unpaired model counts. Selection counts
include visibility/fading queries and offscreen/streaming candidates; they are
NOT actual draw counts or GPU costs. Use companion draw/model histograms and
triangle counts to verify submitted workload. Model links are invalidated when
definitions/big-building relationships are rebuilt.

For a controlled performance comparison, replay **ON → PERFORMANCE → ON →
PERFORMANCE**, with the usual fixed resolution, refresh rate and Forward View,
and Static Instancing, Opaque Mesh Batch, Masked Alpha Pass Skip and Opaque Order
Experiment OFF. Keep Opaque World Fast Path ON. No restart is required between
mode changes; set the mode before starting playback and keep it fixed per run.

### Static instancing experiment

Graphics → Optimizations → **Static Instancing** is a persisted, default-OFF benchmark switch
(`VR/StaticInstances`); changes apply immediately. Compare four normal demo
playbacks in **OFF → ON → OFF → ON** order. Keep Opaque Mesh Batch, Opaque Order
Experiment and Masked Alpha Pass Skip OFF, Opaque World Fast Path and Forward
View ON, and the usual fixed resolution, refresh rate and minimal profiling.
Keep RenderDoc detached and auto-capture OFF.

The native, effects-free world path collects a window of at most 64 static
building draws. Up to 16 matching opaque ranges can use one indexed instanced
draw, with each object's own transform and original visibility decision. Matches
require identical buffers/ranges, pipeline, texture descriptor, non-transform
material/lighting push data, model and profiling flags. Members must lie within
32 metres of the first member. No persistent merged geometry is created.

Before moving a matching draw earlier, both eyes must prove independence from
every crossed draw. Opaque depth-writing barriers allow separated depth
intervals or screen rectangles; alpha/blending barriers require screen
separation. Bounds cover the entire geometry, include a raster-pixel expansion
and numerical margins, and reject near-plane/invalid projections. Instance order
inside a group is preserved. Masked/blended meshes retain ordinary draws and
act as order barriers; they are not instanced.

Phase, scene/viewport, immediate-mode and resource-retirement boundaries flush
the collector. Skinned/dynamic/custom callbacks, material effects, temporal SGSR,
physical scope and active world effects use the normal path. A cold/missing
instanced pipeline or failed transform allocation retains ordinary submissions.
Transform uploads use a private 256 KiB buffer per frame slot, reset after that
slot's fence, and do not consume the immediate-mode/bone stream.

The session records `static_instances_at_start` and the policy version. Frame
columns `static_instances_*` record enabled state, queued draws, successful
groups/instances, **actual saved submissions**, state/bounds/order/spatial
rejections, pipeline misses, allocation failures and source/emitted index totals.
The two index totals must agree; GPU/model triangle counts include every
instance, while draw counts reflect actual Vulkan draw calls. `flush_ms` covers
planning/upload/emission, excluding front-end collection and bounds preparation.
Existing per-atomic binding-cache counters are disabled while the collector is
active; use the new counters and normal CPU/GPU timings for acceptance.

The previous reuse feasibility audit is disabled on both OFF and ON runs of
this build. Its session policy says `disabled_for_static_instance_benchmark`,
and its rows have `static_reuse_sampled=0`. Successful compilation and host
ordering/fallback tests do not establish a device performance gain or absence
of all visual artifacts; the controlled replay recordings provide that check.

### Cross-object static reuse audit (earlier diagnostic build)

The earlier diagnostic build collected `static_reuse_*` on the native
GPU profiler's rotating one-in-eight source frames. Two matching normal demo
playbacks are sufficient for this opportunity audit; keep Opaque Mesh Batch,
Opaque Order Experiment and Masked Alpha Pass Skip OFF, Forward View ON, and
RenderDoc/auto-capture OFF. That build measured opportunities rather than
implementing instanced rendering or draw savings. The following field reference
applies to its historical recordings; the audit is disabled in the subsequent
instancing benchmark build described above.

The audit accepts default-pipeline opaque static buildings in roads/world/fading
phases, with depth testing/writes and no alpha test. Dynamic/skinned/custom
callbacks, material effects, translucent/split-alpha meshes, unknown owners,
offscreen wrist targets and invalid transforms are excluded. It groups exact
shared geometry ranges, pipeline and texture descriptor handles, model ID and
all non-transform push data, including material state and per-object lighting.
Render-phase boundaries separate groups. Equal geometry content in separate
allocations is not detected. Untracked global state still needs checking before
any grouping is used to change rendering.

- `source_frame_id` refers to the current CPU-side `frame_id`, unlike delayed
  GPU result IDs. `sampled=0` rows carry no audit; do not average them as zeros.
- `candidates` counts eligible draw observations; `groups` counts exact-state
  groups. `geometry_repeats` counts draw observations sharing a geometry range
  with an earlier different owner, even if state differs; it is not unique
  instance count.
- `compatible_instances` deduplicates object identities within each exact-state
  group and counts owners after the first. `nearby_instances` is its subset
  within 32 metres of the first owner. Neither implies safe reordering or
  predicted draw savings. An object with multiple meshes can count per range.
- `adjacent_instances` counts consecutive compatible different-owner draw
  transitions. Intervening unsupported draws and phase boundaries break it;
  transitions can revisit owners and are not a unique-instance count.
- `max_instances` and `top_model` identify the largest observed group, with
  first-seen tie breaking. They are not GPU cost rankings.
- `cpu_ms` measures the in-draw audit, including matching and clock overhead.
  Per-atomic eligibility setup and phase/reset hooks are outside this timer.
  Sampled frames can be slower; treat this build as diagnostic and do not
  subtract this partial timer to manufacture an uninstrumented benchmark.
- Storage is bounded to 2048 geometry and state slots, 64 probes per lookup and
  32 owners per state group, with no per-draw allocation. `table_overflow` and
  `owner_overflow` disclose truncated observations; resulting counts are lower
  bounds. `reject_state`, `reject_owner` and `reject_transform` explain exclusions.

Geometry, object and Vulkan handles remain in process-local tables; only numeric
counts and model IDs enter the CSV. The analysis tool verifies source-frame
identity and summarizes valid sampled rows separately from unsampled rows.

### Opaque mesh batching experiment

Graphics → Optimizations → **Opaque Mesh Batch** is a persisted, default-OFF benchmark switch
(`VR/OpaqueMeshBatch`). It combines at most 32 adjacent opaque triangle-list
mesh ranges within the same default-pipeline object. Ranges must be contiguous
in the same index buffer, with identical texture/sampler identity, material
colour and surface properties. Transform, lighting and stereo state remain
per-object. It preserves index order and falls back at incompatible boundaries;
it does not batch across objects, change culling, or merge transparent/PS2 split
passes, material effects, skinned objects or dynamic-object passes.

Use four normal demo playbacks in **OFF → ON → OFF → ON** order with Forward
View ON, fixed resolution/refresh rate and all other settings held constant.
Leave Opaque Order Experiment and Masked Alpha Pass Skip OFF. No restart is
needed between toggle changes. Keep RenderDoc detached and auto-capture OFF.

The session records `opaque_mesh_batch_at_start`; frames record the current
`opaque_mesh_batch_enabled` and `opaque_mesh_batch_*` counters. `eligible`
counts eligible group starts, `checks` counts attempted adjacent joins,
`merged_meshes` counts source meshes in successful groups, and `saved_draws`
counts eliminated Vulkan draw submissions. Rejections distinguish unsupported
state, incompatible material/vertex alpha, and invalid/noncontiguous ranges.
Counters reset each frame and are collected with performance profiling enabled.
OFF performs no compatibility scans. GPU draw/model histograms count the actual
combined submissions and all their triangles, not the original source meshes.

Compare normal CPU/GPU times and tails, saved submissions, unchanged triangle
counts and visual behavior. A zero or small saved-draw count means this narrow
path has little opportunity in that scene; it is not evidence for a GPU gain.

Use the same save, route, assets, settings, refresh rate and warm-up. Record at
least three repeats per build and retain matching OVR Metrics reports. Use a
controlled route without deaths for the main comparison; test combat and
death/respawn separately for transition stalls.

```text
python tools/analyze-perf-capture.py "before.frames.csv" "after.frames.csv"
```

The command prints one JSON summary per run: metadata, activity counts, events,
missing/dropped records and mean/median/p95/p99 timings. Default timing selection
requires an alive, present player outside pause/cutscenes; events cover all rows.
`--include-nongameplay` includes all states. Phase distributions require their
validity bit. GPU percentiles describe asynchronous observations, not uniquely
measured GPU frames. Frame intervals include pacing, not compositor missed counts.
Schema 2 results also group speed distributions, highest observed speeds and
handling limits by vehicle model. Older recordings have no vehicle-speed data.

Clean close adds `closed`, `written_rows`, `dropped_rows` and `write_error` to
session metadata. Incomplete/error captures are normally rejected; after a crash,
`--allow-incomplete` salvages complete rows with explicit limitations. Missing
IDs, drops and selection transitions must be checked before tail comparisons.

## Overhead and storage

A bounded 1024-frame queue feeds a background writer. The game thread uses a
nonblocking enqueue and drops samples instead of waiting on a contended/full
queue. Formatting/disk I/O run on the worker, with periodic flushing. Disabling
profiling drains accepted samples and closes the pair; force-stop can lose
the last buffered portion. Disk errors are recorded in session metadata.

`recorder_prepare_ms` excludes enqueue, legacy logging and worker contention.
Measure total overhead with matched recording OFF versus MINIMAL/ON OVR runs.
MINIMAL reduces display clutter; it still incurs the metric collection cost.
Files grow with session length and are not automatically deleted. Disable full
profiling when finished and remove unwanted recordings locally.
