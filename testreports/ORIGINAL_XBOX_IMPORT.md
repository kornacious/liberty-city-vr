# Original Xbox local asset import

Local Windows validation, 2026-11-06. Original disc data stayed local and is not
part of this change. The source image was opened read-only; temporary extracted
copies were cleaned by the importer. Only asset sizes/hashes identify the supported
US baseline in source control.

## Changes

- Replaced the Manager's pinned HD-mod/download workflow with a required local
  `.iso`/`.xiso.iso` input and strict retail file fingerprints.
- Added bounded XDVDFS reading for full DVDs and trimmed XISOs, without an external
  extractor. Paths, directory recursion, extents and duplicate files are checked.
- Added local LZO decompression and librw native conversion with a headless CMake
  target, preserving original texture dimensions/mips and using the compressed
  Xbox-to-D3D texture path where supported.
- Select the Xbox source pipelines when decoding native atomics, including those
  without explicit pipeline rights. Reopen every converted asset and validate
  portable geometry, finite vertex positions and texture rasters.
- Export the retail `playa5` bitmap for re3's default player skin. Route the retail
  default model definitions and matched world assets as a complete profile.
- Gate runtime availability on the retail identity; retain the existing setting
  and folder names. Old HD files remain untouched until successful replacement.

## Results

| Check | Result |
|---|---|
| .NET Release build | Passed, zero warnings/errors |
| Manager regression suite | 79/79 passed |
| Production profile routing/calibration/startup fixture | 85 checks passed |
| Native multi-block decoder and malformed-block CTest | Passed |
| Supplied retail ISO preparation | Passed, all output models/dictionaries reopened |
| Generated archive | 3,135 DFFs + 916 TXDs; approximately 242.39 MiB |
| Vehicle/vegetation manifests | 61 vehicle definitions; 101 vegetation names |
| Full ARM64 debug APK | Built successfully |
| APK and verified asset installation | Updated personal Quest 3 device |
| Windows standalone Manager | Published to ignored local output |
| Startup smoke | App launched and process survived the observation period; no recent fatal reports in filtered logs |

The smoke check did not observe the retail profile activation log and does not
establish gameplay or visual acceptance. Select Original Xbox and fully restart
for headset acceptance. Test all districts/LODs, shared materials, skinning,
cutscenes, wheel/glass behavior, weapon/seat calibration and sustained performance.

## Limits

This is an asset importer for the current Vulkan renderer. Desktop Neo lightmaps,
gloss/rim/reflection behavior are not enabled by this work. Xbox ANM cutscene-hand
extensions and console frontend are not imported. PC gameplay scripts, cutscene
animation archives, audio, saves and vehicle collisions remain on the existing
foundation. Existing Xbox VR calibration is reused provisionally.

Initial local checks exposed a compiler debug-PDB problem and CMake's handling of
the sandbox temporary path. Validation succeeded with Release compile probes in
the established local build workspace. Initial native conversion failures were
resolved before the successful complete preparation and installation.
