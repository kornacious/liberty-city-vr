# Visualization debug

Open **VR Overlay > Graphics > Visualization debug** during gameplay. Changes
apply live, last only for the current process, and do not change saved graphics
preferences. **Reset all visualization debug** restores normal drawing, shows
all categories, and disables all overlays. Closing the menu keeps the selected
view active; returning to Graphics does not reset it.

- Geometry: meshes only (default), xray wireframe only, meshes plus green triangle
  edges. Edges follow skeletal animation and vehicle deformation. Triangle
  strips and degenerate connectors are handled; optional Vulkan polygon-line
  support is not required. Wireframe-only draws edges without a hidden-surface
  fill pass. **Wireframe - hidden edges removed** instead fills mesh depth without
  color and replays green edges after world/effects geometry, before the HUD.
  Opaque surfaces hide edges behind them; transparent surfaces retain their
  normal depth-write rules. It adds a depth pass and edge draws. Edge buffers are allocated lazily per streamed geometry and freed
  when that geometry is released. Wireframe is a diagnostic, not a performance
  benchmark.
- Shading: normal lit, unlit textured albedo with material tint, solid grey,
  geometric face normals in view/play space, vertex color, a 16-cell UV checker,
  native vertex lighting (including baked prelight), linear distance from the
  eye, and submitted mesh overdraw. Diagnostic shading bypasses material
  effects, fog, color grading and blur. Texture alpha coverage is preserved.
  The overlay and HUD remain readable. Vertex lighting excludes per-pixel
  dynamic lights and material tint; it is not a full lighting buffer.
- Overdraw: each submitted atomic/skinned mesh fragment adds an orange
  contribution on black; more layers become brighter toward white. Depth test
  and writes are disabled, and this view overrides the wireframe setting.
  CPU visibility/LOD/backface/occlusion decisions still apply. This is mesh
  layer accumulation, not an exact count of all GPU fragment invocations:
  immediate-mode particles, water, shadows and sky are excluded. It does not
  measure shader cost or invisible geometry removed before submission.
- Physics overlays: cyan transformed collision bounding boxes, green collision
  triangles/boxes/spheres/lines, and yellow velocity vectors showing one second
  of native linear motion for physical entities. These are collision bounds,
  not exact deformed render-mesh bounds or solver-contact/force visualization.
  Overlays cover submitted visible entities within 25, 50 (default) or 100 metres,
  with budgets of 256 entities and 16,384 line segments per frame. They are depth
  tested, do not write depth, and leave simulation/collision behavior unchanged.
- Visibility: sky/clouds, buildings, roads/terrain, pedestrians, vehicles,
  objects/props, water, particles/effects, and shadows. Hiding an entity only
  suppresses drawing; it remains in simulation and streaming. The road category
  uses the engine's treadable-building classification. Terrain embedded in a
  building mesh follows Buildings; this port has no separate terrain layer.

On Quest, check stereo alignment of animated wires, near/far alpha-tested
surfaces, each visibility toggle and reset, then compare the default view with
normal gameplay. Host topology, pipeline isolation, and menu-layout tests do
not replace headset visual QA.

Select **Graphics > Show FPS and Rendering Stats > DETAILED** for a rendering
focused view. On PCVR, **F12** independently toggles a resizable ImGui window
with the same telemetry; opening it releases mouse capture. The desktop preview
suppresses the mirrored DETAILED stats while F12 is open, leaving one readable
copy on the monitor. Closing F12 restores those stats. The headset stats and
interactive settings menus remain visible.
The preview panel has a **Reset preview options** button. Neither diagnostics control starts a
performance recording or changes optimization settings.

The view shows CPU render/list costs, Vulkan GPU time when available, entity
list counts, authored occlusion mode, CPU geometry-occlusion candidates and
rejections, conservative fail-open cases, stereo coverage cells, and occlusion
build/query cost. It also shows batching/instancing saved draws, backface state,
LOD selections, and completed GPU draw/triangle samples broken down by pass.
LOD modes are 0=Off, 1=On, 2=Performance, 3=Aggressive. Backend counters describe
the latest completed app frame; timing values are smoothed and entity-list
counts update at the profiler's one-second diagnostic cadence. GPU command
samples arrive separately every eight frames; their source frame ID is shown.
When rendering pauses, the last completed measurements remain on screen.

To assess occlusion, hold the viewpoint and settings fixed and compare
**Optimizations > World occlusion** OFF/ON separately from authored occlusion.
Behind a large opaque building, look for geometry rejections and fewer submitted
draws/triangles, then compare GPU time against the extra CPU build/query cost.
Check both eyes, doorways, near-plane crossings, transparent surfaces and moving
head positions for missing surfaces or popping. A zero rejection count can be
correct in an open scene. Invalid/budget cases keep geometry visible. Counters
show activity, not proof of correct visibility or a net speedup.

Use wireframe, overdraw and category toggles above to inspect the remaining
workload. For per-draw pipelines, textures, depth/blend state and mesh inspection,
use the existing RenderDoc capture workflow. Debug views change the workload;
return to normal shading before timing comparisons. PCVR can inspect shared
rendering decisions, but PC timings do not measure Quest GPU performance.

Geometry occluders now clip real triangles against the eye's clip volume.
Nearby buildings crossing the near plane remain scheduling candidates; their
bounding boxes never write occlusion depth. Opaque material meshes can contribute
even when the same atomic contains transparent meshes. Alpha textures, vertex or
material transparency, material effects and unsupported mesh states remain
excluded. Material eligibility is checked each frame and cached face selection
is rebuilt when the eligible ranges change. Per-source scans, triangle selection,
raster and query work retain their existing limits.

The DETAILED view exposes the pipeline in order:

- **COVERAGE L/R**: percentage of the 48x48 cells with conservative depth in each
  eye. This is map coverage, not percentage of scene geometry hidden.
- **SOURCES IN / USED / NO OPAQUE**: submitted candidates, sources with sampled
  eligible faces, and candidates with no eligible opaque mesh. USED does not mean
  every source contributed coverage; OCCLUDERS counts contributors.
- **MESH OPAQUE / ALPHA / UNSUP**: material meshes examined in the selected source
  candidates, classified by eligibility. These are not scene-wide mesh totals.
- **SRC BAD GEO / SMALL/OFF**: unsupported source geometry versus source bounds
  rejected by projected footprint scheduling.
- **CLIPPED / PROJECTED / DROP**: original sampled triangles requiring clipping,
  surviving in at least one eye, or yielding no usable triangle in either eye.
  DROP includes offscreen, clipped-away and numerically invalid surfaces.
- **WORK C/R/E**: raster cell tests, raster rows and edge evaluations, with limits
  of 8,192 / 8,192 / 24,576 per frame across both eyes. Legacy fan-equivalent
  triangle counts and query-validity failures remain in performance captures.
- **DEFER CACHE / SOURCE CAP**: sources deferred by cold-cache scan budget or the
  projected-source capacity. INVALID and BUDGET remain aggregate event counts.

The four largest GPU pass draw counts are shown to keep these diagnostics readable
in the headset; remaining passes are summed as OTHER PASSES. Full per-pass data
and the new eligibility/clipping counters remain in performance captures.
The view-cull mode label explicitly identifies stereo frustum culling:
0=Off, 1=Stereo safe, 2=Authored experimental, 3=Aggressive experimental.
It is separate from the geometry occlusion map.

The DETAILED headset display and PCVR F12 window also show **LEFT/RIGHT MAP**
previews from the same completed frame as the CPU counters. Black cells have no
proven depth; blue/green cells have conservative depth, with brighter colors
indicating nearer surfaces. Brightness uses a nonlinear device-depth curve for
legibility, not metres or a shaded scene image. N/A means no map was built.
The previews are diagnostic copies; display quantization never affects culling.
Uncovered regions can be real openings, conservative edge margins, omitted
sources, shortlist losses or work-budget limits.

Clipped triangles remain convex polygons through selection and rasterization.
Each source surface occupies one of the 256 shortlist slots, independently of
how many triangles a fan would have required. Coverage checks use only its outer
edges and its original surface depth plane, removing artificial fan-diagonal
gaps. Separate mesh triangles are not joined, and no convex hull is formed over
windows or other openings. Both eyes must still prove an object hidden.

- **POLY IN / KEEP / DROP** counts useful polygon candidates, shortlisted
  polygons and selection losses.
- **RASTER SKIP** counts selected polygons skipped or interrupted by raster
  work/contributor limits.
- **NO DEPTH** counts attempted polygons that added no closer depth. This can
  indicate redundant depth, insufficient full-cell coverage, invalid shape/depth,
  or an interrupted raster; it is not proof of redundant geometry by itself.
- **EDGE** counts row-span and cell-coverage edge evaluations. A new 24,576-edge
  limit bounds the extra cost of polygons with more than three edges; existing
  cell/row/query limits also apply. Limits stop work conservatively.

Depth reuse avoids raster work that cannot improve the map. Each eye maintains
4x4-cell depth summaries. A tile becomes eligible only when all 16 cells have
proven coverage; its stored farthest depth is an upper bound. Subsequent depth
improvements may leave that bound conservatively farther, causing fewer skips
rather than incorrectly discarding useful depth. Summaries reset with the map.
Whole polygons or tile spans are skipped only when existing depth is already
nearer than their conservative minimum depth. Remaining cells compare the exact
proposed depth before evaluating polygon edges. These checks never write depth
or combine partial coverage across an opening.

- **DEPTH SKIP POLY / EYES** reports selected surfaces skipped by the tile proof
  in every applicable eye, and individual eye skips. These overlap NO DEPTH.
- **TILE/CELL** reports cell visits skipped by tile proof and exact-depth
  comparison, respectively. Repeated visits to the same screen cell count again;
  these are avoided map-building operations, not pixels or GPU draw rejections.
- **CHECKS** reports cheap tile checks, bounded to 8,192 per frame across both
  eyes. At that limit, tile checks fall back to normal rasterization; exact cell
  checks remain bounded by the existing raster-cell limit. Capture field
  `world_occlusion_redundancy_budget_stops` is 1 when this fallback occurred.

Existing raster, edge, source and query limits remain unchanged. A/B comparisons
should examine build time, edge/raster usage, coverage and object rejection
together. The fast paths are enabled with geometry occlusion and require no
additional settings.

Certified row spans reduce repeated cell-edge tests. Each edge's minimum over a
whole cell varies linearly across a row. When both endpoints of an interior span
pass every polygon edge with an additional rounding allowance, the cells between
them use that coverage proof. Depth comparison and conservative far-depth writes
still occur per cell. Boundary cells, short rows and uncertain proofs retain the
original per-cell edge checks. Certification runs lazily after depth-reuse checks
find a cell that could improve depth, avoiding proofs for already rejected work.

**SPAN R/C** counts certified rows and cells using those row proofs. **FALL**
counts rows that used at least one per-cell fallback, including ordinary boundary
cells. A row may therefore appear in both certified and fallback counts; FALL
is not a correctness-error count. Performance captures also contain
`world_occlusion_span_edge_tests`, the endpoint-proof evaluations included in
the existing EDGE total. No raster, source, selection or query limits increase.

Map-build profiling now uses exclusive phases. The DETAILED/F12 rows show:

- **GATHER**: engine candidate filtering and initial priority selection.
- **BOUNDS**: backend source validation, transforms and projected footprints.
- **MATERIAL**: prepared-source validation and opaque mesh eligibility checks.
- **CACHE**: cached face selection, cold scans/rebuilds and source preparation.
- **CLIP**: source-face traversal, projection/clipping and priority scoring.
- **SEL**: backend source sorting, polygon shortlist insertion and final ordering.
- **RAST**: selected-surface visits, rasterization, depth reuse and contribution
  accounting. It excludes final map-coverage scans and the later object queries.

All values are milliseconds. Capture-only SETUP (map/shortlist reset) and FINALIZE
(validity/coverage scans) complete the partition of OCC BUILD. The existing total
still begins immediately before map reset; earlier view-matrix setup is outside
that bracket. The phases sum to the total within output rounding, and QUERY is
separate. Extra phase clocks run only when profiling is collecting; timings
include instrumentation overhead. No clocks are added inside raster cell loops.
`world_occlusion_phase_clock_reads` counts the extra phase-boundary samples;
the existing start/end clock reads are reused.

Up to three **S… M… ZERO …/… ADD …** rows identify sources with the most
zero-depth attempts. S is a slot in this frame's prepared-source list; M is the
game model ID. ZERO is zero-depth polygons / attempted polygons, and ADD is the
number of polygons that improved depth. Slots are not persistent object IDs.
Multiple instances may have the same model ID. Ties prefer more selected work,
then the lower slot. A source may be useful overall while many of its later
polygons add nothing; this report does not justify removing or hiding its model.

Captures retain selected/attempted/improved/zero-depth/budget/redundant counts
and depth-update events for those three sources. Fully budget-skipped polygons
are selected but not attempted; interrupted attempts can also be budget-limited.
Redundant attempts are a subset of ZERO. Depth updates include replacing an
existing value with a nearer one, so they are not unique covered cells. Sources
without zero-depth attempts are omitted. The analyzer aggregates reported model
observations, explicitly limited to the top three source slots per frame.

The compact display combines batching/instancing savings, removes some secondary
eligibility rows, and shows four draw passes. The full counters remain in captures.

The clip-code fast paths preserve the same clip thresholds, plane order and
conservative raster rules. Fully inside triangles project directly. A triangle
is rejected early only if all three vertices are confidently outside one common
plane; vertices outside different planes may still enclose visible geometry.
Crossing triangles traverse only the planes crossed by their original vertices.
Inputs within a floating-point uncertainty band use the original full clipper.

The GPU timing rows in DETAILED and the F12 window show sampled milliseconds for
SCENE, POST, WRIST, SCOPE and OTHER. The sample ID and total identify their common
source frame. POST excludes SCOPE; SCENE includes resolve/store and all logical
world draw buckets. `--` denotes unavailable data. Top draw buckets remain sorted
by submissions and are not individual GPU timings. These rows use the existing
asynchronous native-pass timestamp queries without splitting the tiled scene.

**CLIP IN/OUT/CROSS** reports direct projections, common-plane rejects and
selective clipping paths. **CLIP REF/BAD/PLANES** reports full-reference fallbacks,
invalid optimized-path inputs/results, and actual polygon-plane traversals.
REF is a normal boundary-safety path, not an error count; its outcomes remain
classified as REF. CROSS can yield an empty polygon. PLANES excludes the initial
lightweight classification checks, so it is not a count of all plane arithmetic.
These route counters count each eye's triangle invocation separately; the older
CLIPPED/PROJECTED/DROP counters count original sampled faces across both eyes.
Their totals therefore should not be compared directly. Source eligibility,
triangle-selection limits, raster budgets and the two-eye hiding requirement
remain unchanged. Use the measured CLIP phase time to assess the device benefit.

**Overdraw - depth tested** uses the same accumulation colors but retains each
mesh draw's normal depth test/write rules, including transparent materials.
It shows fragments that pass depth in submission order. A far surface drawn
before a nearer one can still contribute; this is not a final-visible-surface
count or a GPU hardware fragment-invocation counter. Both overdraw variants
exclude sky/effects and override the geometry selector.

For live statistics, select **Graphics > Show FPS and Rendering Stats > ON**,
then close the settings menu. MINIMAL only shows FPS and frame time. The full
profiler shows FPS, frame budget, CPU workload, GPU timing (META/runtime and/or
VK/Vulkan), CPU rendering phase costs and visible-entity counts. Its final rows
now show **GPU DRAWS**, **TRI** (submitted triangles in thousands), and
**GPU ALPHA** (draws requesting alpha blending). These totals include all
instrumented passes, including HUD, shadows and diagnostic depth/edge passes;
they are sampled once every eight frames from completed GPU readbacks, not
instantaneous hardware utilization. N/A means no complete sample is available.
They do not measure shaded pixels, overdraw ratio or VRAM bandwidth. Multiview
stereo counts a submitted command once; triangle totals are not doubled per eye.

Compare performance with **Meshes only + Normal lit** at a fixed viewpoint,
then toggle one scene category at a time. Debug modes change the render workload,
so their timings should not be treated as normal-gameplay performance.
