# Conservative uniform-alpha split-pass rejection

## Change

The PS2 alpha policy normally submits two halves of a blended, depth-writing
mesh: alpha at or above the reference writes depth, and alpha below it blends
without depth writes. Some distance-faded opaque materials have constant
fragment alpha, making one half discard every fragment.

The atomic world path now omits that empty half only when all of these hold:

- The PS2 split is active and fragment alpha testing is enabled.
- This is the unskinned world shader, outside dynamic-object and vehicle paths.
- Vertex alpha is uniform opaque and the texture is untextured/opaque white or
  an opaque Vulkan root raster. librw root rasters self-parent; subrasters and
  unsupported platforms and render-target/camera rasters retain both halves.
- Actual sampler U/V state is wrap, mirror or clamp-to-edge. Border/unknown
  addressing retains both halves even for an untextured white fallback,
  because border sampling can introduce transparent pixels.
- Effective alpha matches the shader's actual material multiplier. Material
  colour is ignored when geometry lacks `MODULATE`.
- Alpha/reference are finite and in range, the reference is away from zero/one,
  and their separation exceeds **1/1024**. Equality, near-threshold values,
  reference endpoints and uncertain inputs retain both halves.

The 1/1024 guard is smaller than one authored 8-bit alpha step but much wider
than ordinary float roundoff. It also avoids the negative-zero reference case,
which cannot encode the shader's second comparison correctly.

Rejection occurs before depth/alpha state changes, pipeline lookup, descriptor
binding, scope capture and draw submission. The surviving half keeps its
original pass index, signed shader reference and depth/blend behavior. Shader
code and draw ordering are unchanged. Cutout foliage, glass, skinning and
unsupported cases keep the existing path; the PS2 alpha fix stays enabled.

## Profiling

The sole surviving half is counted as the primary `mesh_passes` command,
including when it is the faint half. `extra_split_draws` counts only additional
submitted halves. PS2 policy flags still identify the retained command, so do
not infer extra draws by simply halving `ps2_split_draws`. Triangle and draw
counts describe issued commands and exclude skipped work.

Schema 6 files and dynamic model attribution are retained. Session metadata
adds `ps2_uniform_alpha_fast_path=world_constant_alpha_margin_1_over_1024`.
No resolution, refresh rate, graphics preference, foveation, replay, model
geometry, texture contents or simulation changes are included.

## Validation

- The host fixture compiles the production atomic selector. Differential
  coverage checks use the current fragment shader's complementary comparisons.
  All 65,536 authored alpha/reference pairs are tested with three small alpha
  perturbations, checking each half: **393,216 coverage checks**.
- Explicit cases cover equality, guard boundaries and next-representable
  floats, zero/one and nonfinite references, invalid alpha, `MODULATE`, material
  absence, opaque root/self-parented rasters, subrasters, texture/vertex alpha,
  disabled testing, border/inherited/unknown addressing, vehicle/dynamic paths,
  unsupported shaders and bone data.
- Existing vehicle-alpha, Vulkan pipeline, GPU capture, writer and analyzer
  regressions pass.
- Fresh pinned-base assembly and Android arm64 debug APK build pass. The task
  APK is installed with `install -r` on the personal Quest 3 device; saves,
  original game data and recorded demos are preserved.

Host checks establish selector behavior against the shader comparisons; they
do not validate original assets visually on the headset or establish an FPS
gain. Those are checked with the next replay pair.

```powershell
./tools/tests/test-uniform-alpha.ps1 -OutDir <new-output-directory>
```

## Next benchmark

Replay the same 120-second demo twice at **1512 x 1584, 120 Hz**, with the same
fixed game/SideQuest settings and consistent head direction. Minimal profiling
is sufficient. Let playback finish and retain all three capture CSV files.

Compare actual road/world/fading draws, extra split draws, scene CPU/GPU time
and frame-time tails, especially 35-40 seconds. The previous pair averaged
100.99/99.59 app FPS, 7.328/7.473 ms scene GPU, and 13.654/13.362 ms scene GPU
in that slow window. Check distance-faded buildings/props and masked edges for
visual changes. The earlier ~126 candidate passes per sampled frame was an
aggregate-data estimate, not a promised saved-draw count or FPS increase.
