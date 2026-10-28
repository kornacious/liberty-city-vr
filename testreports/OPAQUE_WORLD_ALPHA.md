# Opaque world shader selection

## Change

World draws frequently retained an enabled alpha test even when texture,
vertex and material data requested no alpha variation. This change selects
the existing `RW_ALPHA_TEST=0` world specialization for proven opaque draws,
removing an always-passing discard path. It changes no shader source.

The selector requires no requested blending, no enabled vertex blending and
no PS2/vehicle split. It reuses the conservative uniform-alpha proof for the
world shader: no dynamic-object/vehicle/skin path, opaque vertex data, an
opaque Vulkan root texture or opaque white fallback, supported wrap/mirror/
clamp sampler state, and no subraster/render-target/unsupported texture.

The proof uses the actual unsplit positive **`alphaTestRef`**, not the PS2
split reference. It respects the shader's `MODULATE` behavior and requires
alpha to exceed the reference by more than **1/1024**. Endpoint, nonfinite,
out-of-range and near-threshold inputs retain the existing behavior. Draws
that already disable alpha testing keep their existing pipeline selection.

`alphaTestFunction` is overridden only while looking up the eligible pipeline,
then immediately restored alongside depth-write state, including null-pipeline
returns. Vehicle split handling retains priority. Texture, geometry, draw
ordering, depth/blend state and material/shader outputs remain otherwise the
same. The preceding uniform-alpha split-pass optimization remains active.

## Profiling and comparison

The existing `alpha_test_draws` counter now describes the selected pipeline:
it should fall for eligible road/world meshes while total draws/triangles and
the genuine cutout/split work remain comparable. No new timing markers or
schema changes are introduced. Session metadata adds
`opaque_world_alpha_fast_path=guarded_opaque_positive_alpha_test_ref`.

Replay the same 120-second demo twice with **1512 x 1584, 120 Hz**, the same
fixed game/SideQuest settings and consistent head direction. Minimal profiling
records all data. Let playback finish; retain frames/session/draws CSV files.

The preceding pair recorded 106.73/105.69 app FPS, 6.764/6.942 ms average scene
GPU time and 12.253/12.492 ms scene GPU time in the 35-40s window. In that window,
1,484/1,511 world draws enabled alpha testing, but only 90/92 requested alpha.
About 1,400 draws are candidates, not a guaranteed eligible count: additional
texture/sampler/state guards still apply.

Compare actual alpha-tested commands, total draws, CPU/GPU timings, slow-route
windows and frame-time tails. Less fragment-discard work may help, but the
GPU benefit depends on the driver and workload and is not claimed by tests.
Check opaque surfaces, fading transitions, foliage and masked edges visually.

## Validation

- Production selectors compile in the host fixture. Opaque coverage is checked
  for all 256 authored references with three precision perturbations: 768 new
  differential checks, alongside the prior 393,216 split-pass checks.
- Explicit cases retain alpha testing for blend/split/cutout/dynamic/vehicle/
  skin paths, unsafe samplers/rasters, disabled tests and uncertain thresholds.
- The actual pipeline-selection block is compiled with a lookup stub to verify
  selected alpha/depth flags, vehicle precedence and restoration on success
  and failure. Profiling observes the temporary selected state before restore.
- Vulkan pipeline, vehicle-alpha, GPU recorder, writer and analyzer regressions
  pass. All four world shader specializations compile and validate; optimized
  alpha-test-disabled variants contain no discard operation.
- Fresh pinned-base assembly and Android arm64 debug build. The task APK is
  installed with `install -r` on the personal Quest 3 device, preserving saves,
  original game data and recorded demos. Actual visual and performance results
  require the next on-headset replay pair.

```powershell
./tools/tests/test-uniform-alpha.ps1 -OutDir <new-output-directory>
```
