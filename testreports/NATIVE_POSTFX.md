# Native-resolution postprocess shader

## Target and implementation

The corrected RenderDoc capture identifies the final fullscreen draw as the
strongest individual candidate among six tested draws. Two selective replays
measure about 1.20–1.24 ms when skipping that draw. That is an upper bound, not
an expected saving while preserving its output.

`rw_postfx.frag` now also compiles with `RW_POSTFX_NATIVE=1`. This removes motion
visualization, temporal reconstruction and SGSR branches from the fragment
entry point, allowing the compiler to remove their resources and helper code.
The common scene sample, FXAA, five-step GTA III colour treatment, transition
mask, screen fade and output functions are shared unchanged with the general
shader. The default/general SPIR-V is byte-identical to its pre-change build.

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
