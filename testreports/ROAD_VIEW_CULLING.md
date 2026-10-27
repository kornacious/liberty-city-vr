# Tracked-view road rejection

## Change

`CRenderer::RenderOneRoad` now checks `ShouldRenderInCurrentView` before
visible-building profiling, collision debug drawing, material setup or render
submission. Roads previously bypassed the tracked-eye rejection already used
by non-road entities and fading submissions.

The existing visibility helper transforms building bounds into the same space
used for rendering and retains an entity if either tracked eye can see it.
Large bounds can intersect a view even when the entity origin is offscreen.
Unavailable tracking and nonfinite bounds fail open. The shared 360-degree
render list is preserved and each submission uses the current view, allowing
roads to return on a head turn without streaming/list changes.

This follows the selected culling mode, including its existing angular margin.
OFF retains the full set. Mono/theater behavior follows the existing helper
policy. Null entities and unloaded render objects are skipped before access.
No resolution, graphics preference, LOD, road geometry, collision simulation,
traffic, replay, timestamp or CSV schema changes are included.

## Validation

- Fresh assembly from the pinned re3 base and Android arm64 debug APK build.
- Host tests compile the actual patched road function, renderer visibility
  helper and Vulkan stereo box/sphere functions, with engine/device stubs.
  Cases cover left-eye-only and right-eye-only visibility, neither eye,
  large crossing bounds, head turns, all three enabled culling modes, OFF,
  inactive per-view culling, absent tracking, nonfinite bounds, sphere and
  conservative big-building fallback, unloaded/null entities, shipping and
  mono paths, debug collision drawing and profiler accounting.
- GPU profiling backend, capture writer and analyzer regressions pass.
- Task APK installed with `install -r` on the personal Quest 3 device;
  original game data, saves and recordings are preserved. Diagnostics export
  remains accessible. Visual correctness and performance benefit require the
  next on-headset replay pair; host stubs do not validate original road assets.

Focused test command, after assembly:

```powershell
./tools/tests/test-road-culling.ps1 -StageRoot <assembled-source-directory> -OutDir <new-output-directory>
```

## Comparison procedure

Use the same 120-second route-assisted ghost demo, Classic assets, graphics
settings, 120 Hz, minimal/full profiling and consistent head direction.
Verify actual output **1512 x 1584 at 100%** before playback, since 100% alone
does not establish the absolute resolution across game launches.

The preceding same-resolution pair is the baseline:

| Metric | Baseline 1 | Baseline 2 |
| --- | ---: | ---: |
| Average application FPS | 95.26 | 94.67 |
| Average road draw commands per sampled frame | 971.6 | 975.0 |
| Average scene GPU ms | 8.216 | 8.318 |
| Scene GPU p95 ms | 12.700 | 13.638 |
| Scene GPU mean ms during 35-40 seconds | 14.306 | 14.500 |

Let both new playbacks finish normally. Compare road draw counts first to
confirm rejected work, then road CPU, total scene CPU/GPU, frame-time tails
and the same slow route windows. Individual road GPU costs remain unavailable;
the whole-scene timing measures the resulting GPU change. Check visible road
edges and head turns for missing geometry. No gain is claimed before capture.
