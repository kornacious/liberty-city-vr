# Recording and comparing performance

In **Graphics → Show FPS and Rendering Stats** select **MINIMAL** for FPS/frame
time only, **ON** for the full live diagnostic display, **DETAILED** for rendering
diagnostics, or **OFF** to hide it. PCVR also has an independent F12 ImGui window.
These display modes never start or stop recording.

DETAILED and the PCVR F12 window show two rows of sampled GPU milliseconds:
`GPU MS SCENE / POST` and `WRIST / SCOPE / OTHER`. `GPU SAMPLE` identifies the
source frame and its total GPU interval. These five exclusive intervals use
the same delayed sample (one frame per eight-frame block); they are not the
latest CPU frame or the independently updated Vulkan GPU headline. `--` means
unavailable, not zero. POST excludes its nested SCOPE interval. OTHER is GPU
time outside the named brackets, including gaps, not the other draw buckets.

SCENE includes all logical scene phases and the native pass's resolve/store.
Roads, world, fading and the other top draw buckets show submissions, not
individual GPU costs: they share a tiled native render pass. In-pass timestamps
would not isolate those costs reliably on Quest. The instrumentation does not
split the scene pass or add query waits. Existing `gpu_pass_*_ms` capture columns
contain these timings; use their valid mask/status and deduplicate by GPU source
frame ID when comparing samples. Logical scene buckets retain `-1` GPU times.

The `explicit_early_opaque_v1` fragment policy requests early depth tests for
eligible unblended world and instanced triangles with depth testing and
writes enabled. The fragment module omits alpha discard and is selected
for the world path with alpha test, world effects, Neo and mesh debug
disabled. Pipeline creation falls back to the ordinary module if needed.
Use the sampled scene GPU interval and the same route, resolution and
texture format to compare the policy across builds.

Masked Alpha Pass Skip also checks the alpha range of fully uploaded RGBA8
native texture assets, including their mip levels. It can omit either PS2 alpha pass when
every sampled alpha is on the same side of the split threshold. Incomplete
uploads, unsupported formats, or a fallback texture descriptor retain the
baseline decision. The replay counts additional savings in
`masked_alpha_uploaded_solid_passes_skipped` and
`masked_alpha_uploaded_faint_passes_skipped`.

**Graphics → Optimizations → Classic Foliage Cutout** defaults to OFF and
applies immediately. In first-person stereo with PS2 Alpha enabled, it keeps
the depth-writing half of eligible masks on `veg_tree3`, `veg_treenew05`, and
`veg_bush14` in the Classic asset set. Removing the faint half can make leaf
edges sharper. Use `classic_foliage_cutout_enabled` and
`classic_foliage_faint_passes_skipped` to compare same-session replays with
the toggle off and on, and check canopies in motion in both eyes.

Use **Graphics → Active Performance Profiling → ON** to record the existing
performance metrics. It defaults to **OFF**. Select **OFF** after the route to
drain accepted samples and close the files. Display changes preserve a running
capture session, frame IDs, events and vehicle-speed peaks.

Replay Simulation always enables profiling during demo playback. Preparation is
excluded; completion, manual stop and abort restore the prior profiling preference.
The temporary override is never saved and does not change the stats display.

Simulation Replay also records each playback's lowest instantaneous app FPS and
its exact simulation timestamp in `.run.csv`. A completed playback without a
RenderDoc capture establishes the demo's default automatic capture point. Per-demo
`.capture.csv` settings persist the target and auto toggle; new demos use zero and
OFF until a baseline completes. Manual **RenderDoc Capture Current Frame** captures
the next complete rendered XR frame independently of the toggle and target. See
[the replay capture workflow](../../testreports/SIMULATION_REPLAY.md#replay-timed-renderdoc-capture).
The former Auto Profile Playback switch has been removed.

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
- The world, vegetation, vehicle, pedestrian and weapon columns all reflect the
  same active base (`0=PC`, `1=Original Xbox`). Older captures can contain
  legacy model-set values. A requested change takes effect after restart.
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
- `profiler_display_mode` records `0=OFF` / `1=ON` / `2=MINIMAL` / `3=DETAILED` per frame, so display
  changes can be identified without splitting the session.
- Entity-render call counts for vehicles, pedestrians and static objects.
  These are **not** Vulkan draw-call counts or final visible-object counts.

Session metadata includes readable asset names/source labels at capture start,
game version, profiler compilation time, UTC/monotonic clock anchors and selected
saved graphics values. Early startup metadata can precede model initialization;
per-frame active selection is authoritative. Asset names describe configured
routing, not content hashes or proof that base files are unmodified. `saved_*`
values can be `-1` when absent and can differ from active settings. Toggle
Active Performance Profiling OFF and back to ON after changing settings for fresh
metadata. The analyzer supports schema 1, 2 and 3 recordings.

## Vulkan begin-frame investigation (schema 3)

Active Performance Profiling additionally captures synchronous CPU timings for pipeline
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

### Native-resolution FXAA stencil sampling

Open **Graphics -> Optimizations -> FXAA Integer Sampling**. The persisted
`VR/FxaaIntegerStencil` setting defaults OFF and switches immediately without
restarting. OFF selects the original filtered-stencil native pipeline; ON
selects the integer-stencil native pipeline. Both share the same native
eligibility checks, colour-filter factoring, FXAA thresholds and directional
samples. General temporal/upscale processing is not changed by this toggle.
If the integer pipeline is unavailable, ON falls back to the filtered native
pipeline when available, otherwise the general pipeline. OFF never selects
the integer variant. Both variants are prepared during device initialization,
so toggling does not compile a pipeline during measured playback.

The native 1:1 postprocess shader uses integer mip-zero fetches for the center
and four diagonal FXAA contrast samples. Integer coordinates come from the
current fragment and clamp to the source image bounds, preserving replicated
borders and eye-layer selection. Four fractional directional FXAA reads remain
linearly filtered; contrast thresholds, edge direction and colour filtering
are unchanged. The existing native pipeline eligibility requires matching
scene/output dimensions and draw constants with temporal/upscaling disabled.
General temporal/upscale processing retains normalized filtered sampling.

Session metadata identifies this as `postfx_native_sampling_policy` =
`filtered_or_integer_stencil_toggle_v2` and records `fxaa_integer_sampling_at_start`.
Frame columns `fxaa_integer_sampling_enabled` and `fxaa_integer_sampling_active`
separate the requested switch from actual integer-pipeline selection. Active
means integer center sampling even when spatial AA is OFF; with spatial AA ON
the four diagonal reads use it too. Active resets at frame start and remains
false on fallback. Host tests
check ideal texel-center equivalence; device filtering precision can differ
slightly from normalized-coordinate sampling, so visual comparison remains
necessary. The shader retains five contrast-stencil reads and four directional
reads: this changes sample addressing/filter work, not the number of texels
needed by FXAA or its intended quality.

Compare four normal-launch playbacks with FXAA Integer Sampling
**OFF -> ON -> OFF -> ON** using
the same 1512 x 1584 / 120 Hz settings, Forward View ON, AGGRESSIVE LOD, LOD Fade
Transition OFF and World Occlusion OFF. RenderDoc and auto capture stay OFF.
Inspect fine edges and image borders, and compare source-matched
resolve/postprocess timing before attributing any FPS change to this pass.

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
occluders, 1024 sampled source triangles (at most 48 per entity), 8192 raster
cell visits, 8192 raster rows and 16384 query cell visits per app frame. Incomplete coverage and
exhausted budgets reduce rejection rather than hiding uncertain geometry.
The projected-sources v4 policy projects each candidate's complete render
vertex bounds into the current eye views before checking opaque materials,
building source caches or assigning triangle samples. Only a viewport-clipped
extent of at least 2 cells in each dimension and 16 cells in area qualifies in
an eye. Scores favor area shared by both eyes, with a smaller weight for one-eye
contributors. Unprojectable near/far crossings contribute no source score in
that eye. The best 32 qualifying opaque sources receive the existing sample
budget, with deterministic ordering for equal scores. Bounds are scheduling
estimates only: rejected candidates render normally, and bounds never write
depth or prove object occlusion. No source/raster/query budgets were increased.
The policy caches up to 16 broad triangles for
each dominant normal axis in the geometry's instance header. A bounded scan
examines at most 2048 evenly distributed real faces per geometry, retaining
large, non-degenerate, reasonably compact faces and interleaving the axes.
No faces are merged, simplified or synthesized. Cache building examines at
most 4096 faces/frame; deferred objects do not act as occluders yet. The cache
resets on instancing/reinstancing and is freed with the geometry header.
Material/alpha/pipeline eligibility is rechecked every frame. Only face indices
are cached; projections and depth coverage are always from the current frame.
Source samples are distributed one per eligible object per round before any
object gets its next sample, sharing the same 1024-face projection cap.
The policy keeps the best 256 projected triangles from those
samples in a fixed-capacity heap, prioritizing viewport-intersecting projected
area in both eyes. The heuristic changes scheduling only, never the coverage
proof. Per-row edge bounds skip empty portions of triangle bounding rectangles;
every visited cell still passes the original three full-cell edge tests and
conservative far-depth calculation. No partial triangles are merged. Entity
selection partially sorts only the leading 128 candidates. This version still
deliberately samples a subset of real opaque triangles;
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
`world_occlusion_raster_rows` and `world_occlusion_selected_triangles` expose
the additional row budget and bounded shortlist. Source triangles count sampled
input faces; selected triangles count shortlisted faces before raster budgets.
New `world_occlusion_cache_*` counters track builds, hits, scanned faces and
deferred objects. `world_occlusion_source_*` tracks prepared objects, useful
projected samples, samples invalid in both eyes, samples too small/offscreen,
cached faces omitted by the frame budget, and the highest per-object sample
count with its model ID. The top model is a single most-sampled object per
frame (ties retain the first), not aggregate model work or GPU attribution.
The analyzer reports that model's frame-frequency histogram separately.
`world_occlusion_footprint_*` reports candidates tested, zero-score rejections,
candidates with at least one uncertain eye, selected sources and candidates
left unprepared after reaching the 32-source cap. Uncertain is an overlapping
diagnostic, not an exclusive rejection category; capacity-skipped candidates
have not necessarily passed material/cache eligibility. These counters and
total build/query times expose whether the added bounds work is worthwhile.

Clipped opaque-mesh occlusion (`world_occlusion_policy` ending in `v5`) adds
`world_occlusion_source_geometry_rejected`, `world_occlusion_source_no_opaque`,
`world_occlusion_mesh_opaque`, `world_occlusion_mesh_alpha`,
`world_occlusion_mesh_unsupported`, `world_occlusion_source_clipped`,
`world_occlusion_clip_triangles`, `world_occlusion_query_invalid`, and
`world_occlusion_cells_per_eye` (2304). Coverage percentages are
`100 * left_cells / cells_per_eye` and the equivalent right-eye expression.
Selected triangles now count clipped triangle parts; sampled source triangles
still count original input faces. The analyzer includes the new columns when
present and accepts older captures. See the
[diagnostic field guide](QUEST_VISUALIZATION_DEBUG.md) for counter meanings.

The convex-polygon policy (`v6`) adds `world_occlusion_polygon_candidates`,
`world_occlusion_selected_polygons`, `world_occlusion_selection_dropped`,
`world_occlusion_raster_budget_skipped`, `world_occlusion_no_depth_polygons` and
`world_occlusion_edge_tests`. These distinguish shortlist loss, raster work
limits and zero-depth contribution. Under this policy, the legacy
`world_occlusion_clip_triangles` and `world_occlusion_selected_triangles`
columns describe triangle-fan equivalents, not emitted raster primitives.
Actual selection is capped at 256 convex source polygons. Depth-map previews
are displayed live and are not appended to each CSV row.

The depth-reuse policy (`v7`) adds `world_occlusion_redundancy_tests`,
`world_occlusion_redundancy_budget_stops` (0/1 fallback flag),
`world_occlusion_redundant_polygons`, `world_occlusion_redundant_eyes`,
`world_occlusion_tile_skipped_cells` and `world_occlusion_depth_skipped_cells`.
These measure skipped occluder-map work, independently of object rejection and
GPU draw counts. The analyzer includes these fields when present. Cheap tile
checks share an 8,192-check budget across both eyes; exhaustion falls back to
detailed raster checks rather than skipping potentially useful depth.

The certified-row policy (`v8`) appends `world_occlusion_span_rows`,
`world_occlusion_span_cells`, `world_occlusion_span_fallback_rows` and
`world_occlusion_span_edge_tests`. Certified rows and fallback rows can overlap:
a row can use endpoint coverage proof for its interior and per-cell checks at
its boundaries. Endpoint evaluations are already included in
`world_occlusion_edge_tests`; do not add the two counters together. The live
WORK C/R/E row exposes raster cell/row/edge usage to identify which unchanged
budget is reached after reducing repeated edge work.

Instrumentation tag `exclusive_phases_top3_zero_depth_v1` adds
`world_occlusion_build_phases_valid`, `world_occlusion_phase_clock_reads` and
`world_occlusion_build_{setup,gather,bounds,materials,cache,projection,selection,raster,finalize}_ms`.
The exclusive phase sum reconciles with the existing build bracket within CSV
rounding. Untimed rows have a false validity flag; the analyzer excludes them
from phase distributions. Extra phase clocks are enabled with profiler
collection and their cost is included in the timings.

For ranks 1–3, `world_occlusion_zero_source_<rank>_` columns contain `slot`,
`model`, `selected`, `attempted`, `no_depth`, `improved`, `budget`, `redundant`
and `updates`. An unused rank has slot/model -1 and zero counters. Rank is by
zero-depth attempt count, then selected work, with stable slot ties. Slots
identify prepared source instances only in the current frame; model IDs can
repeat across instances. These rows represent the top three observations, not
all model work. The analyzer labels this scope when aggregating by model.
See the [diagnostic field guide](QUEST_VISUALIZATION_DEBUG.md) for phase scopes
and overlapping contribution counters. The instrumentation tag is separate from
the rendering-policy version and does not itself change culling rules.

The clip-code policy (`v9`) appends `world_occlusion_clip_inside`,
`world_occlusion_clip_outside`, `world_occlusion_clip_crossing`,
`world_occlusion_clip_boundary`, `world_occlusion_clip_invalid` and
`world_occlusion_clip_plane_visits`. Route counters classify each eye's clipper
invocation; boundary fallbacks retain the full reference implementation. Plane
visits count polygon traversals, excluding initial classification arithmetic.
Compare the projection phase time alongside these counters; fewer traversals
alone do not establish a frame-time improvement. The analyzer includes these
columns when present and continues to accept older captures.

Replay **OFF -> ON -> OFF -> ON**, keeping World Model LOD AGGRESSIVE, World
Backface Cull OFF, Static Instancing and all remaining settings fixed. Use a
normal launch without RenderDoc/auto-capture. First run one unmeasured playback
with World Occlusion ON to populate source caches, then perform the four
measured playbacks. Keep LOD Fade Transition OFF. Streaming/reinstancing can
still rebuild caches; the new counters expose that work. Depth grids are
rebuilt every frame. Watch both eyes for missing
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

### LOD fade transition toggle

**Graphics -> Optimizations -> LOD Fade Transition** is persisted as
`VR/LodFadeTransitions` and defaults **OFF**, including existing installs that
have no saved value. It applies immediately and leaves World Model LOD mode,
authored switch distances and traffic settings unchanged.

- OFF: switch internal world meshes directly at their selected threshold;
  AGGRESSIVE switches between paired high/LOD models directly and hides
  unpaired detail at its cutoff. Legacy world distance fade bands are disabled.
- ON: restore legacy world distance fades and AGGRESSIVE's reversible 20 m
  crossfades/fade-to-hidden behavior. Internal mesh crossfades are specific to
  AGGRESSIVE; this does not add them to the legacy ON/PERFORMANCE selectors.

Loading/residency opacity is separate and remains active. Retain the related
LOD fallback while detailed geometry is loading; intact/damaged model families
remain separate. The toggle affects simple/time world geometry, not vehicle or
pedestrian callback LOD. Distance fading extends rendering beyond the cutoff
while blending, so OFF/ON is a quality/performance tradeoff even though the
activation distances themselves are unchanged.

Frame CSVs append `lod_fade_transition_enabled`, and session metadata records
`lod_fade_transition_at_start`. The analyzer includes the new toggle in the
recorded configuration. For a controlled comparison, keep World Model LOD
AGGRESSIVE, World Occlusion OFF and World Backface Cull OFF; compare fade
**OFF -> ON -> OFF -> ON** under the usual fixed settings. Old captures made
before this toggle had the distance fades enabled implicitly.

### World model LOD and optimization menu

Open **Graphics → Optimizations → World Model LOD**. This persisted four-state
control (`VR/WorldLodMode`) affects simple/time world models; vehicle and ped
callback LOD and traffic generation keep their existing behavior.

| Mode | Value | Selection policy |
|---|---:|---|
| OFF | 0 | Highest-detail atomic in the correct intact/damaged family; LOD entities suppressed. |
| ON (default) | 1 | Existing authored selection with the current VR minimum 2.2× distance multiplier. |
| PERFORMANCE | 2 | Approximately 1× authored switching distances, with related high-detail/LOD handoffs capped at the normal 300 m sector range. |
| AGGRESSIVE | 3 | Earlier switching and hiding of unpaired detail; fades/crossfades are optional. |

AGGRESSIVE (value 3) uses the PERFORMANCE handoff distances but also limits
unpaired high-detail world models to their authored final distance (at most
300 m). With LOD Fade Transition ON, those models fade out over the following
20 m, and fade back in when approaching. With it OFF, they hide/show directly
at that cutoff. Existing low-detail world entities retain their far coverage.
With LOD Fade Transition ON, internal mesh changes and related high/LOD
handoffs crossfade over 20 m;
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

Two default-OFF controls benchmark the existing static-world renderer independently.
**Static Submission Cache** requires Static Instancing. It indexes candidate
instance matches by geometry and texture and skips repeated Vulkan bindings
within each collector flush. When enabled, the `vk_static_atomic_*` counter
totals also include the collector, where one call denotes one measured flush.
Compare pipeline and texture requests against actual binds and compare CPU
flush time with the control enabled and disabled.

**Static Mesh Frustum Cull** uses bounds built from each indexed mesh of a static
building. It tests those bounds against both current eye frusta before
pipeline and descriptor resolution and keeps any mesh visible in either eye. The replay records
the starting settings as `static_submission_cache_at_start` and
`static_mesh_cull_at_start`. Measure culled work with actual road, world and
fading draw/triangle counts. Check fast head turns and geometry close to the
edge of either eye before enabling it for normal play.

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

### Static texture array batching

**Graphics → Optimizations → Static Texture Array Batching** is a default-off
control for the main stereo world view. It uses the no-discard opaque world
path and works with or without Static Instancing. Keep **Opaque World Fast Path**
on for the Classic comparison. The batcher joins consecutive compatible
opaque triangle ranges within a static road or building atomic. Each triangle
uses the original vertex values and its own material colour, lighting
coefficients, and texture layer. Array layers keep the original texels and
sampler and use a full layer for each texture, so UVs continue to wrap normally.

The path handles single-mip opaque RGBA8 textures up to 256 by 256.
Physical pages group textures by dimensions. Each sampler gets an immutable
descriptor to the shared array view, and each batch still requires one
effective sampler. A raster on the page reuses its layer across sampler variants. Other textures and
order-sensitive surfaces use ordinary draws. Texture pages and the geometry
cache are bounded, and uploads are amortized across frames. The original
draw path also handles cold shaders, exhausted budgets, and failed allocations.

For a comparison, warm the same Classic route in each configuration, then run
three alternating off/on pairs with matching graphics settings, 120 Hz, and
RenderDoc detached. Record both the full route and seconds 20–22. The session
records `static_array_batch_at_start`. Frame columns report emitted batches,
saved draws, submitted indices, cache builds, fallbacks, image allocation bytes,
geometry payload bytes, and uploads. Check the companion draw counts and
frame-time tails on device, and compare both eyes at road seams and coplanar
markings.

For fallback diagnosis, one completed Classic replay with batching on after a
warm-up is sufficient. The `static_array_fallback_*` columns add up to
`static_array_fallbacks` on each frame. They distinguish a full preferred page,
page-count and memory limits, upload and build budgets, and resource failures.
`static_array_shortened_batches` counts runs that emitted a batch for a prefix
after a later texture could not join its page. Those batches still saved draws.
Initial checks that found fewer than two compatible ranges do not count as
fallbacks. The capture analyzer totals the reasons for enabled frames and
leaves the breakdown unavailable for older recordings.

New texture sizes start with up to eight layers. When a size fills its
existing pages, its next page targets twice the largest allocated page for
that group, subject to the existing page, device, and pool limits. Compare
the live and reusable layers with the prior build's occupancy to measure the
change in reserved memory. Page creation retries with fewer layers when the driver's allocation exceeds
the remaining memory budget. A shape whose two-layer page cannot fit is cached
until capacity is available. Compare `static_array_page_size_retries` and
`static_array_page_memory_cache_hits` with actual saved draws and CPU frame
times. To compare against an earlier build's page packing, start both runs in
a fresh game process and warm up on the same route before measuring.

The recorder samples page occupancy every eighth captured frame.
`static_array_occupancy_sampled` distinguishes an unsampled row from a sampled
empty pool. `static_array_page_occupancy` contains one pipe-separated record
per page, with colon-separated width, height, sampler key, total layers,
live layers, layers held for the current recording, and allocated bytes.
Reusable layers are total minus live and held. The capture analyzer aggregates
by dimensions and sampler key. A sampler key of 512 marks a physical page
shared across samplers. The analyzer checks the pool byte total and reports bytes in
completely empty pages separately from reusable layers in occupied pages.

The 24 MiB, 32-page pool protects capacity for textures up to 64×64. Pages
with either dimension over 64 may occupy up to 10 MiB and 15 page slots.
Small textures can use the remaining pool, including capacity the larger
textures leave unused. Limits use the Vulkan allocation size, including
padding, and apply only to new pages. Existing eligible layers can still be
used or filled. The `static_array_fallback_small_texture_reserve` column
counts page admissions blocked by the reservation, while
`static_array_fallback_page_memory_limit` tracks the full-pool limit.
Compare 64×64 resident layers and actual draw counts after warming the same
route from a fresh game process.

Residency diagnostics share the one-in-eight occupancy sampling interval.
`static_array_residency_sampled` marks the source frame.
`static_array_residency_misses` contains pipe-separated records with
colon-separated width, height, initial residency, outcome, and request count.
`absent` means no compatible resident layer. `other_page` means the raster was
resident on another physical page when a specific page was requested.
`uploaded` reports a successful upload after a miss, `resident_only` reports a
lookup that could not allocate, and other outcomes use the existing fallback
reason names. These are lookup counts, so repeated requests for one raster
count separately. The bounded histogram reports uncounted requests in
`static_array_residency_overflow`.

`static_array_page_copies` follows the page order in
`static_array_page_occupancy`. Each entry contains the number of live primary
copies and additional copies. The sum of primary copies across pages counts
unique resident rasters. Each sampler descriptor shares its page's texels.
`static_array_duplicate_merged_layers` counts uses of additional copies in
emitted draws that merged multiple ranges or objects, once per source range.
These fields contain dimensions and counts only.

When an eligible 64×64 or 128×128 texture misses an allocating lookup on
three distinct frames with at most 24 frames between consecutive misses, the batcher
can reuse a cold layer of the same size. The replacement must be on the
page already chosen for the run or on a page containing the next compatible
texture. A slot must have gone at least 240 frames without a reference.
Resident-only probes and textures that already have a copy do not trigger
ordinary replacement. Each page caches its two oldest candidates so it can defer
a search until a layer can age out and still exclude a companion texture.
A request that finds no victim waits eight frames before trying again.
The renderer allows up to eight full page scans and two successful
replacements per frame, within the existing upload limit.
`static_array_demand_probes`, `static_array_demand_replacements`, and
`static_array_demand_upload_bytes` report this activity on every frame.
With `cached_page_candidates_v2`, probes count full page scans and
`static_array_demand_page_deferrals` counts page searches avoided because
the cached candidates are too recent or unavailable for the requested
pair. Demand upload bytes are included in the normal page upload total.

A missing 64×64 or 128×128 texture from a single-range object can qualify
when the immediately preceding queued draw belongs to another object and
the pipeline, sampler, page dimensions and batch limits agree. After three
recent qualifying misses, the renderer may admit one such texture per
frame on that exact page, within the existing upload and replacement
budgets. The initial geometry upload can drain the queue. Subsequent
draws can reuse the admitted layer with cached geometry.
`static_array_adjacent_misses` counts qualifying requests,
`static_array_adjacent_admissions` counts uploads, and
`static_array_adjacent_merged` counts admitted single ranges that actually
participate in an emitted cross-object batch.

### Cross-object array batching

**Cross-Object Array Batching** is a default-off control in the same menu.
Enable **Static Instancing**, **Opaque World Fast Path**, and **Static Texture
Array Batching** for the comparison. This path combines consecutive compatible
submissions from static road and building objects with the same array descriptor
and Vulkan pipeline. It retains primitive order and each object's transform,
lighting, and fog state. Other draws and render-pass boundaries drain pending
work. Each draw groups at most eight source submissions and 4,096 indices.
Merged vertices and indices are cached in a 16 MiB device-local buffer keyed
by the ordered source geometry. The cache holds up to 512 entries and stores
each entry in contiguous 2 KiB blocks. Reinstancing or retiring a source
invalidates its merged entries. Retired blocks stay reserved until the frame
fence completes. Cold builds pack directly into 512 KiB of persistently mapped
staging space per frame slot and record the copy before the draw submission.
The cache attempts at most four builds and 512 KiB of geometry per frame. Each in-flight
frame slot has a 144 KiB uniform stream for current transforms and lighting,
with 128 object records per page. Each merged draw uses its first instance
index to select its records inside the page. Consecutive merged draws reuse
unchanged pipeline, texture and uniform-page bindings. Runs that exceed a
budget or fail allocation use the original draws in submission order.

Keep the previous Classic route settings, warm up for each configuration, then
run three alternating pairs with only Cross-Object Array Batching toggled.
Compare `cross_array_saved_draws`, `cross_array_fallbacks`, and
`cross_array_flush_ms` alongside the normal route and busy-window frame times.
`cross_array_upload_bytes` counts transforms and lighting in this version.
Add `cross_array_cache_upload_bytes` to compare with the combined upload total
reported by the previous stream. The cache also reports hits, misses, successful
builds, and resident geometry bytes.
`cross_array_arena_bytes` is the device buffer size.
`cross_array_arena_reserved_bytes` includes alignment padding and blocks
waiting for fences. `cross_array_arena_misses` counts attempts blocked by
cache capacity or lack of a contiguous writable range.
Compare `cross_array_pipeline_binds`, `cross_array_descriptor_binds`, and
`cross_array_transform_binds` with `cross_array_batches` to measure binding
reuse. Transform page binds are included in the descriptor bind count.
Merged draws contribute their complete index count to the render phase. The
combined model attribution is unknown because a draw can contain multiple
owners. The per-frame timings still include eligibility and collection costs
outside the flush timer.

The flush breakdown is sampled on the same one-in-eight source frames as
texture-array residency. Use rows with
`cross_array_flush_breakdown_sampled=1`. The CPU timing buckets are
`cross_array_lookup_ms` for run eligibility, key construction and cache
lookup, `cross_array_build_ms` for attempted cold geometry builds,
`cross_array_transform_ms` for uniform allocation and writes,
`cross_array_record_ms` for merged command recording and draw attribution,
and `cross_array_fallback_ms` for ordinary draws emitted by the cross-object
queue. The buckets add to `cross_array_flush_ms` on sampled rows within CSV
rounding. Compare frame pacing on unsampled rows if the extra clock reads
affect measurements.

Ordinary array draws drained from the cross-object queue share its
pipeline and texture binding cache. Ordinary and merged draws use the same
layout, so the transform page stays valid across ordinary draws. Renderer
boundaries invalidate the cache. Compare pipeline and texture binds in
`cross_array_ordinary_pipeline_binds` and
`cross_array_ordinary_texture_binds` against
`cross_array_ordinary_draws`. These counters exclude immediate array draws
outside the cross-object queue.

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
dotnet run --project tools/manager -c Release -- --tool tools/analyze-perf-capture.py -- "before.frames.csv" "after.frames.csv"
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
Measure total overhead with matched Active Performance Profiling OFF versus ON OVR runs.
MINIMAL reduces display clutter; recording cost is controlled separately by Active Performance Profiling.
Files grow with session length and are not automatically deleted. Disable full
profiling when finished and remove unwanted recordings locally.
