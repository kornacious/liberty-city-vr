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

**Overdraw - depth tested** uses the same accumulation colors but retains each
mesh draw's normal depth test/write rules, including transparent materials.
It shows fragments that pass depth in submission order. A far surface drawn
before a nearer one can still contribute; this is not a final-visible-surface
count or a GPU hardware fragment-invocation counter. Both overdraw variants
exclude sky/effects and override the geometry selector.

For live statistics, select **Graphics > Performance Profiler (FPS) > ON**,
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
