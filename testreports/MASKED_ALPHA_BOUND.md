# Masked-material alpha upper-bound rejection

## Problem and change

The existing uniform-alpha optimization could skip an empty half of a PS2
alpha split only for opaque/untextured samples. Faded alpha-textured world
geometry kept both halves even when its material alpha was too low for any
fragment to pass the solid threshold.

For the existing unskinned world shader, fragment alpha is constant opaque
vertex alpha × effective material alpha × sampled texture alpha. A known
normalized texture sample is in [0,1], including its ordinary filtered mip
samples. Consequently material alpha is an upper bound. If that upper bound
is below the positive solid reference by more than 1/1024, the solid shader
would discard every fragment. The backend now omits that pass and retains the
original faint pass, including its signed reference, discard, blending, depth
and motion behavior. Zero-alpha material does not cause the entire mesh to be
removed; the faint pass remains.

The new branch returns only `Faint` or `Both`. An upper bound cannot justify
discarding the faint half or selecting an opaque-only pipeline.

## Conservative proof

- Active split, enabled alpha testing, unskinned world shader.
- Outside vehicle splits, vehicle/dynamic passes and bone-data paths.
- No vertex alpha; retain the existing conservative uniform-opaque vertex proof.
- Supported sampler U/V wrap, mirror or clamp-to-edge. Border/unknown modes
  retain both halves.
- Alpha-textured Vulkan root/self-parented texture raster, actual native view
  present, explicit `VK_FORMAT_R8G8B8A8_UNORM`. Unknown, compressed/float formats,
  subrasters, other platforms and camera/render-target rasters retain both.
- Material alpha applies only with `Geometry::MODULATE`, exactly as in the
  current vertex shader. Otherwise its effective upper bound is one.
- Reuse the existing finite/range/reference endpoint guards and 1/1024 margin.
  Equality and near-threshold values retain both halves.

The selector runs after sampler state setup but before changing split-pass
depth/alpha state or issuing pipeline/descriptor work. The surviving faint half
continues to be counted as the primary mesh pass, not as an extra pass. Shader
code, texture contents, foliage softness, visibility, draw order, draw distance,
resolution, lighting and simulation are unchanged. Existing uniform-alpha and
opaque-world fast paths retain their behavior.

## Capture instrumentation

New session metadata:
`masked_alpha_bound_policy,world_unorm_material_upper_bound_margin_1_over_1024_v1`.

Appended optional per-frame CSV columns:

- `masked_alpha_bound_candidates`: textured split decisions reaching the new
  proof after the existing shader/entity/vertex/sampler guards.
- `masked_alpha_bound_solid_passes_skipped`: proven empty solid-pass selections.
- `masked_alpha_bound_indices_skipped`: their index counts, 64-bit and not
  multiplied by stereo views. This is submitted-element work, not pixels or a
  measured time saving; strip degenerates are not subtracted.

Counters reset with the existing per-frame backend submission counters. The
game thread copies values independently of frame-begin timing availability.
Formatting and I/O remain on the background writer. Schema 6 and prior columns
retain their order; the analyzer reports totals, per-frame distributions,
frames with skips and invalid-count diagnostics. Older captures report the
new summary unavailable rather than synthetic zeros.

## Validation

- Host regression extracts the actual normalized-raster proof and production
  alpha selector. It compares both original shader half-pass tests against
  retained commands across all byte material alpha × reference × texture alpha
  combinations and three bounded interpolation perturbations: **100,663,296
  coverage checks**, preserving fragment/pass membership.
- That fixture identifies 32,385 empty-solid decisions among 65,536 synthetic
  material/reference pairs, with exact candidate/skip/index counter accounting.
  This is not an estimate of the real route's opportunities.
- Unsupported raster formats/views, root/subraster cases, ignored material
  colour, variable vertices, vehicle/dynamic/skinned cases, bone data, sampler
  borders and disabled alpha testing retain both halves.
- Existing 393,216 uniform-alpha coverage checks and 768 opaque-world checks
  pass, including pipeline state restoration and failure behavior.
- CSV writer regression passes, including all new columns and >32-bit index
  counts. Six analyzer tests pass, including backward compatibility and invalid
  skipped > candidate detection.
- Fresh pinned-base assembly and full arm64 Quest APK build pass. Final package
  verification passes; installed the task APK with `install -r` on the verified
  **personal Quest 3 device**. Game data, saves and recorded demos are preserved;
  read-only diagnostics remain accessible.

Host checks establish coverage under the proof's assumptions. Visual checks
with real assets and actual CPU/GPU benefit await the user's next replay pair.

## Benchmark

Use the latest 30-second busy-section demo twice, same 1512 × 1584 / 120 Hz
settings, Forward View ON, Opaque Order Experiment OFF and Opaque World Fast
Path ON. Launch normally with no RenderDoc injection/automatic capture. Minimal
profiling is sufficient. Check masked/faded edges and distant foliage in both
eyes. Retain all capture CSVs.

The preceding two postprocess-gain runs are the baseline. First verify actual
nonzero skipped work, then compare scene GPU/CPU time, total/extra split draws
and the busy final 10 seconds. There is no promised FPS improvement; the
available gain depends on real material fade values and eligible masks.
