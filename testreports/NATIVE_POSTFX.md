# Native-resolution postprocess shader

## Target and implementation

The corrected RenderDoc capture identifies the final fullscreen draw as the
strongest individual candidate among six tested draws. Two selective replays
measure about 1.20–1.24 ms when skipping that draw. That is an upper bound, not
an expected saving while preserving its output.

`rw_postfx.frag` now also compiles with `RW_POSTFX_NATIVE=1`. This removes motion
visualization, temporal reconstruction and SGSR branches from the fragment
entry point, allowing the compiler to remove their resources and helper code.
The common scene sample, FXAA, transition mask, screen fade and output functions
are shared with the general shader. The original native implementation also
shared the five-step colour treatment; the guarded gain refinement below now
factors that native calculation. The default/general SPIR-V is byte-identical
to its pre-change build.

The backend prebuilds an optional native pipeline with the existing layout and
one-attachment render pass. `PostFxPolicy.h` selects it only with:

- nonzero, equal scene and output dimensions;
- matching dimensions in the actual pushed draw constants;
- configured and pushed SGSR mode both OFF;
- known colour modes 0–2 and spatial-AA flag 0–1;
- a successfully created native pipeline.

Temporal, reconstruction, diagnostic, scaled-size and unknown modes use the
general shader. Native pipeline creation failure also retains the general
path. No compilation or allocation occurs when selecting a pipeline per frame.
The extra pipeline is destroyed with the existing postprocess resources.

## Verification

- Both shader modules compile and pass SPIR-V validation for Vulkan 1.1.
- The native SPIR-V has only scene-colour binding 0, retaining the centre and
  eight possible FXAA neighbor sample instructions, with no history/depth/
  motion bindings or reconstruction fetch/gather/query operations.
- Production selection-policy tests cover both AA values, colour and mask
  modes, mismatched dimensions/constants, zero size, missing pipeline and
  temporal/diagnostic/unknown-mode fallback.
- A temporary RenderDoc shader replacement on the corrected capture produces
  byte-identical RGBA render targets for both 1512 x 1584 eyes (zero differing
  components). Reference images contain nonzero scene variation. This checks
  that captured frame, not every game state or graphics-mode combination.
- Replay reports zero high-severity validation violations after replacement.
  The original shader was restored and the replay session closed afterward.
- Native driver statistics report 389 compiled instructions, register
  footprint 5 and ALU fiber occupancy 100%, compared with approximately 1,800
  instructions, footprint 10 and occupancy 50% for the general shader. These
  are static compiler statistics, not dynamic instructions per pixel or proof
  of a particular frame-time saving. The driver labels the fragment executable
  "Mesh Shader"; the stage query and pipeline remain fragment/VS-PS.
- Native SPIR-V size is 6,900 bytes versus 25,480 for the general module.
- Full arm64 debug APK compilation passes. The task APK was installed with
  `install -r` on the personal Quest 3 device, preserving data and saves; the
  read-only diagnostics provider responds after installation. Unhooked demo
  performance verification remains pending the user's two benchmark runs.

Reproduction:

```powershell
./tools/tests/test-postfx-native.ps1 -ShaderTools <NDK-shader-tools> -OutputDirectory <new-output-directory>
```

## Benchmark

Replay the same demo twice after reboot/cooldown and an unmeasured warm-up.
Keep 1512 x 1584 at 120 Hz, native scene size, temporal/SGSR OFF, the existing
FXAA and colour-filter settings, Forward View ON and minimal profiling. Launch
normally with RenderDoc injection and automatic capture OFF. Compare the
postprocess GPU bucket, whole-scene GPU bucket, CPU time and frame intervals;
optimizing the final pass does not by itself remove the separate scene spike.

## Guarded uniform colour gain (2026-10-31)

The 23-second busy-section capture confirmed that the final native postprocess
pass still has a real cost: about 0.99 ms when the entire pass is removed in
selective replay. That is not an attainable saving while retaining its output.
The inspected native executable had 389 instructions and nine texture reads.

The native shader now factors the five-step RGB recurrence:

`P[0] = S; P[n+1] = clamp(S*(1-a) + P[n]*C*a, 0, 1)`

into `P[5] = S*gain(C,a)`, where `gain[0]=1` and
`gain[n+1]=(1-a)+gain[n]*(C*a)`. In the native normalized scene path,
`S`, `C` and `a` are all in [0,1], so each intermediate stays in [0,1] and
the clamps do not change the mathematical result. The coefficient recurrence
is uniform; the Adreno compiler moves work into its preamble, reducing main
per-pixel arithmetic. No extra CPU coefficient allocation, push-constant data,
descriptor, pipeline or texture fetch was introduced.

`PostFxPolicy.h` now checks finite, normalized RGBA coefficients before selecting
the native colour-filter path. Unknown/nonfinite/out-of-range coefficients,
including alpha above one through intensity, retain the original general shader
and its clamps. Copy and mask modes do not use the colour filter, so coefficient
checks do not unnecessarily exclude those modes. Existing size, mode, temporal,
AA and pipeline-availability guards remain. Scene storage is the existing
normalized RGBA8 image; HDR/unknown scene formats must not opt into this policy
without extending its source-range proof.

Temporal/upscale processing, all nine FXAA samples, history RGB, colour mode,
screen fade, output alpha and resolution are preserved. The general SPIR-V is
byte-identical to the previous version (SHA256
`1542aee3d1e36e4b9d6087e4cd5f363338d7597a2a59e9ed0b9ce2253792ee28`).
Session CSV metadata labels the feature as
`postfx_native_colour_policy,uniform_gain_v1_normalized_coefficients`.

### Validation

- Both shader variants compile and pass SPIR-V validation; the native variant
  retains nine image-sample instructions and no temporal/upscale resources.
- Host tests execute the gain function extracted from the production shader
  against the original clamped recurrence: 624,297 cases including all byte
  coefficient/alpha combinations, representative source values and random
  fractional input. Maximum normalized error is `3.57627869e-7`; quantized
  differences are at most one byte level. Floating-point reassociation means
  this is not claimed to be bit-identical for every possible input.
- Policy tests cover missing pipelines, invalid dimensions/constants, temporal
  modes, invalid coefficients (negative, >1, infinity, NaN) and copy/mask bypass.
- A temporary local replay replacement on the recovered 23-second capture
  succeeds. For both 1512 × 1584 eye images, only two and three RGBA components
  respectively change, all by one 8-bit level. The reference images have
  nontrivial scene content. Image exports stay local in ignored cache.
- Driver main instruction count falls **389 → 321** (−17.5%). Preamble count
  increases 56 → 102; nine texture reads, footprint 5 and reported compiler
  occupancy 100% remain. These compiler counts are not a frame-time speedup.
- Replay message check reports no HIGH-severity violations. The replacement
  was restored and its managed replay closed.
- Full arm64 debug APK builds successfully. A final incremental package build
  includes the session policy label.
- Installed the final task APK with `install -r` on the verified **personal
  Quest 3 device**. Existing game data and saves preserved; read-only performance
  diagnostics accessible.

Normal unhooked performance remains to be measured. Use the latest 30-second
busy-section demo twice, normal launch, RenderDoc detached/auto-capture OFF,
Opaque Order Experiment OFF, Opaque World Fast Path ON, Forward View ON,
1512 × 1584 at 120 Hz and usual fixed settings. The preceding same-route OFF
runs form the initial baseline. Do not attribute the whole pass's 0.99 ms to
this smaller arithmetic optimization.
