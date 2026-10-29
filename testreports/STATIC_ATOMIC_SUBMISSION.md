# Static-world atomic submission optimization

## Problem and change

World/fading submission rebound a graphics pipeline and the material descriptor
set for every successful mesh pass, even when the previous pass in the same
atomic already established identical state. Split alpha halves also repeated
the same raster/sampler descriptor lookup.

The Vulkan atomic path now keeps a small local binding cache for SHADER_WORLD
with no bone offset, dynamic-object flag or vehicle-alpha flag. The first
successful mesh pass always binds both states. Later passes skip only an exact
pipeline handle match or an exact descriptor-set AND pipeline-layout match.
Different textures or samplers still resolve their own descriptor handles.
The descriptor lookup is reused only across the surviving split halves of one
mesh; it remains lazy so failed/missing pipelines do not allocate descriptors.

The cache ends when the atomic returns. It never spans objects, command buffers,
subpasses, frames, UI, scope replay or other renderers. Dynamic/skinned/vehicle
paths retain unconditional binds and per-pass lookups. Existing draw order,
geometry, push constants, blending, depth, alpha coverage, motion output and
physical-scope draw capture are preserved. This changes neither shaders nor
pixel count and does not merge or remove any draw commands.

## Profiling

The new session metadata identifies
`static_atomic_submission=local_bind_cache_and_split_descriptor_reuse_v1`.
Six additive frame columns capture synchronous CPU submission bookkeeping:

| Column | Meaning |
|---|---|
| vk_static_atomic_calls | Eligible atomic calls, including ones with no successful draws |
| vk_static_atomic_pipeline_requests | Successful draw preparations requesting a pipeline binding |
| vk_static_atomic_pipeline_binds | Pipeline binding commands emitted |
| vk_static_atomic_texture_requests | Successful draw preparations requesting a texture binding |
| vk_static_atomic_texture_binds | Texture descriptor binding commands emitted |
| vk_static_atomic_texture_lookups | Material descriptor lookups performed |

Counters reset at backend frame begin and are collected only with the native
profiler enabled. Use them when `vk_begin_detail_valid` is 1. The analyzer's
`atomic_submission` section reports totals, per-frame means, binding skip
percentages, avoided descriptor lookups and invalid counter rows. Request minus
emitted gives commands saved; texture request minus lookup gives lookups saved.
Zero requests have no skip percentage. Legacy captures report the feature as
unknown, not as zero savings. Existing CSV columns stay in order and schema 6
remains supported. These are eligible static-atomic counters, not exclusive
world/fading GPU timings or total Vulkan command counts.

## Verification and benchmark

The host test compiles the actual production binding block and compares 2,401
mesh/pass sequences against an unconditional reference. It checks per-draw
state and order, split passes, missing pipelines, texture/sampler/layout changes,
white fallback, empty geometry, first-bind behavior after unrelated commands,
independent atomic boundaries, disabled caching and profiling-off behavior.
CSV writer and analyzer tests verify new counters and legacy compatibility;
existing alpha coverage, pipeline warmup, GPU attribution, texture and vehicle
regressions remain applicable.

Run the same demo twice with BENCHMARK FORWARD VIEW ON, OPAQUE WORLD FAST PATH
ON, 1512 x 1584 at 120 Hz and unchanged graphics/SideQuest settings. Minimal
profiling is sufficient. Compare with the previous ON runs and check both CPU
world/fading time and binding skip counts. Device performance and visual results
must be measured; host equivalence and successful compilation are not evidence
of an FPS gain. A wider cache across object boundaries would need separate
invalidation work and is intentionally outside this change.

## Local validation, 29 October 2026

All equivalence, coverage, pipeline, GPU-profile, texture/vehicle, CSV writer and
six Python analyzer tests passed. A freshly assembled full Quest APK built
successfully in 47 seconds and was installed with `install -r` on the personal
Quest 3 device. Game data, saves and demos were preserved; the read-only replay
diagnostics provider remained accessible. Device visual/performance comparison
awaits the two new demo replays.
