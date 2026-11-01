> Tool migration: standalone `.ps1`, `.sh` and `.py` entrypoints are now embedded
> manager modules. Open **Tools and tests**, or run `dotnet run --project
> tools/manager -c Release -- --tool MODULE -- ARGUMENTS` from the repository root.
> Legacy filenames below identify resource modules; old direct shell invocations
> are historical. Use the manager pages for build/install, editor build, music,
> models, repair and update. See [the manager guide](../../tools/manager/README.md) for runtime requirements.
# Classic and Modern (Xbox HD) assets on Quest

The in-game asset menu has two choices: **Classic** and **Modern (Xbox HD)**.
Modern is the complete GTA III Xbox Version HD re3 pack. Vehicles, pedestrians,
weapons, world and vegetation switch together, so a model cannot accidentally
use a different profile's texture dictionary. Fully restart after changing it.

Old `ModelSet=2` (Xbox) settings migrate to Modern. Old per-category settings no
longer split the pack. Xbox remains an internal calibration identity for the
measured vehicle/weapon offsets; it is not a third selectable asset profile.
The legacy `modelsets/xbox` folder is ignored and need not be deleted to test.

Classic reads the base installation. Keep the original GTA III game files as
the base; the modded Windows installation is a read-only comparison reference.

## Source and preparation

Supported release: **[III] Xbox Version HD (Supports re3)**, ModDB file 224588,
released December 4, 2021:
https://www.moddb.com/mods/gta-iii-xbox-version-hd/downloads/iii-xbox-version-hd-supports-re3

The ZIP is 1,552,864,677 bytes. SHA256:

```text
879024BED6FB89C218F6C75A46119F95FC068A64849BC74DEDF0851D3FD210A6
```

Python 3.9+ is required on Windows and Linux. Windows uses the bundled texture
compressor. Build the Linux compressor once:

```sh
c++ -O2 -o tools/modelsets/txdcompress tools/modelsets/txdcompress.cpp
```

Windows:

```powershell
.\INSTALL_MODERN_MODELS.bat -GameDir "C:\Games\GTA3" -XboxArchive "C:\Packs\III_Xbox_Version_HD_Supports_re3.zip"
```

Linux:

```sh
./INSTALL_MODERN_MODELS.sh --game-dir "$HOME/Games/GTA3" --xbox-archive "$HOME/Packs/III_Xbox_Version_HD_Supports_re3.zip"
```

Modern is the default and only generated profile. Old Xbox/Both profile switches
remain command aliases for Modern and do not generate duplicate overlays.
`-XboxSource` / `--xbox-source` accepts the extracted supported re3 folder.
`-ModernArchive` and `-HdArchive` accept the same ZIP; the old second pack is retired.
Use `-BuildOnly` / `--build-only` to avoid device access. A custom output directory
must end in `modern`. The default is `<original-game>/modelsets/modern`.

Without a supplied archive, download consent is requested. ModDB can block
automated clients; in that case supply the downloaded ZIP locally. Every prompt
is flushed before input, and preparation reports phase/file/byte progress.

Default cache: `C:\LCVRBuild\modern-assets` on Windows or
`$HOME/LCVRBuild/modern-assets` on Linux. Allow roughly 12 GB free while preparing;
the final IMG is about 2.01 GiB. Existing marked local outputs are replaced only
after a complete verified build. Unmarked folders are preserved and rejected.

The builder pins 93 source-file hashes and merges four archive layers in their
PC precedence order: `gta3`, `xbox_models`, `xbox_hd_textures`, `xbox_vehicles`.
Four empty placeholders are omitted. The output has 4,051 entries, 62 vehicle
models, 101 vegetation names and 91 files including its manifest. DFF geometry and UVs retain
their authored data; supported TXDs are compressed/mip-capped at 1024. Every
output file has a checksum; source files remain untouched.

Modern also includes the pack's `models/generic/player.bmp`. Claude's gameplay
body and cutscene head use this bitmap through re3's player-skin renderer, so
loading the Classic bitmap produces misplaced facial and clothing textures.
The default bitmap follows the active Classic/Modern profile; custom skins
remain user choices. Rebuild and reinstall older generated Modern overlays
after updating to the Claude skin fix, and fully restart the game.

No game/mod assets are included in the repository or APK. Generated assets
remain local and must not be redistributed.

The pinned wheel container references an absent `wheel_lightvan64back` texture
on six shared wheel materials. Preparation replaces only that texture name
with stock `wheel_lightvan64` from the base game's `models/MISC.TXD`, retaining
geometry, UVs and alpha. This provides a textured rim/tyre fallback instead of
the solid white surface. Rebuild older Modern overlays to apply the repair.
The Vehicle Editor loads both `MISC.TXD` and `generic.txd` as shared textures
and its catalogue validation rejects unresolved wheel textures.

## Installation

Do not delete modelsets folders first. The installer stages a complete Modern
copy and verifies file sizes and every manifest hash before replacing it.
Saves, settings and base game files are preserved. It discovers the build wizard's
SDK at `C:\LCVRBuild\.android-sdk` or `$HOME/LCVRBuild/.android-sdk`; custom roots
can be supplied with `-AndroidSdk` / `--android-sdk`.

Install an already prepared overlay directly:

```powershell
.\tools\install-modern-models.ps1 -Profile Modern -ModernDir "C:\Games\GTA3\modelsets\modern"
```

```sh
./tools/install-modern-models.sh --profile modern --modern-dir "$HOME/Games/GTA3/modelsets/modern"
```

## Loading and rendering compatibility

The Windows reference's archives, `generic.txd` and re3 executable match the
supported ZIP. Its loading relies on an existing re3 feature: after searching
the model's current dictionary, it searches the global generic dictionary.
Desktop re3 installs that callback with EXTENDED_PIPELINES. Quest now installs
and removes the same lookup behavior independently of the desktop Neo renderer.

The audited pack has 200 models whose own dictionaries omit referenced textures
that are supplied by `generic.txd`; terrain includes road, pavement, grass and
cliff textures. Another 351 models use that shared dictionary directly. Missing
the fallback makes those otherwise valid materials render white.

The Vulkan vehicle alpha pass also separates fully opaque body pixels from
translucent pixels in mixed paint/glass materials. Opaque pixels write depth,
preventing later interior surfaces from overwriting the body. Translucent glass
blends without depth writes, preserving visibility of hands behind the window.
Windscreens explicitly drawn without depth writes retain that behavior.

| Area | Modern pack content |
|---|---|
| Vehicles | Vehicle geometry/textures and loose wheels; Xbox calibration paths. |
| Pedestrians | Player/pedestrian geometry and textures, plus cutscene actors. |
| Weapons | GTA III weapon/projectile models and dictionaries; existing VR behavior. |
| World | Buildings, roads, props, LODs, radar dictionaries, matching IDE/IPL/map collision files. |
| Shared visuals | Global generic textures, HUD/fonts/menu/particles, prompts and loading artwork. |
| Vegetation | Pack geometry and textures loaded as part of the same profile. |

Retail vehicle collision files are retained, including train/boat records.
PC executables, mission scripts, settings, saves, optional audio, GXT language
files, skins and Neo renderer configuration are excluded. Desktop lighting and
reflection settings are separate from model/texture loading.

## Verification limits

The complete APK builds; host tests cover shared texture lookup, callback
lifecycle, two-profile migration, complete-pack routing and vehicle alpha depth
policy. Preparation tests validate pinning, archive precedence, failure
preservation and visible prompts. These are not visual headset acceptance.

Retest the white terrain locations, ambulance/police bodywork and windows,
other districts and LODs, pedestrian skinning, cutscenes, weapons/scopes, HUD/fonts
and sustained GPU/memory/thermal behavior. The full HD pack is heavier than
Classic, especially vegetation. See [the texture compatibility report](../../testreports/XBOX_HD_TEXTURE_COMPATIBILITY.md).
