# Classic, Xbox HD and Modern assets on Quest

Xbox and Modern preparation now use **[III] Xbox Version HD (Supports re3)**,
ModDB file 224588, released December 4, 2021:
https://www.moddb.com/mods/gta-iii-xbox-version-hd/downloads/iii-xbox-version-hd-supports-re3

This replaces the old vehicle pack, Google Drive downloads and two-pack Modern
workflow. The two profile names remain available for saved category choices;
newly generated Xbox and Modern contain the same assets. Installing files
preserves saved selections. Fully restart the game after changing categories.

No game/mod assets are included in the repository or APK. Build the overlays
locally for a legally owned GTA III PC installation. Never redistribute them.

## Prepare and install

Install **Python 3.9 or later** and put it on PATH (Windows and Linux). Windows
uses the bundled `txdcompress.exe`. Linux also needs a C++ compiler; first run:

```sh
c++ -O2 -o tools/modelsets/txdcompress tools/modelsets/txdcompress.cpp
```

Windows:

```powershell
.\INSTALL_MODERN_MODELS.bat -GameDir "C:\Games\GTA3" -Profile Both -XboxArchive "C:\Packs\III_Xbox_Version_HD_Supports_re3.zip"
```

Linux:

```sh
./INSTALL_MODERN_MODELS.sh --game-dir "$HOME/Games/GTA3" --profile both --xbox-archive "$HOME/Packs/III_Xbox_Version_HD_Supports_re3.zip"
```

Do not delete the headset's `modelsets/xbox` or `modelsets/modern` folders
before preparing or installing. The installer keeps each existing profile until
a staged replacement has passed verification. Use Both to replace both old
profiles; the default Xbox-only run leaves Modern untouched. Local output folders
with a generated profile marker are also replaced automatically after a complete
build. If an unrecognized local folder is rejected, preserve or rename it rather
than deleting arbitrary game files.

The console reports the current phase, file counts, and byte percentages during
large hashes/extractions/downloads. Prompts appear on complete lines before
waiting for input. Windows runs Python unbuffered and limits each interpreter
probe to five seconds. Progress is also recorded in the diagnostic log printed
at startup. Compression reports texture counts; its percentage is not an ETA.

Default profile is Xbox. Use `-Profile Modern` / `--profile modern` for Modern
only. `-XboxOnly`, `-ModernOnly` and `-SkipXbox` remain compatibility aliases.
`-ModernArchive` and `-HdArchive` also accept this same pinned archive.
`-ModsArchive` is rejected because the former second pack is retired.

Use `-BuildOnly` / `--build-only` to prepare without device access.
`-OutputDir` / `--output-dir` requires one profile and a matching `xbox` or
`modern` folder name. Both writes to `<game>/modelsets/xbox` and `modern`.
`-XboxSource` / `--xbox-source` accepts the extracted re3 folder or its wrapper;
its allowlisted files must match the supported release exactly.

Without a local input, `-AcceptDownloads` / `--accept-downloads` requests the
pinned release through a fresh official ModDB mirror. ModDB may block automated
clients; the wizard fails with the release URL and instructions to supply the
ZIP locally. It never substitutes another pack. Non-interactive use without
local input requires explicit download consent. Old cached vehicle ZIPs and
unversioned source folders are never auto-selected.

Default cache: `C:\LCVRBuild\modern-assets` on Windows,
`$HOME/LCVRBuild/modern-assets` on Linux. Allow roughly 12 GB free during
preparation; this pack is much larger than the former vehicle-only overlay.
A generated IMG is about 2.01 GiB; installing Both uses roughly twice that.

The supported ZIP is 1,552,864,677 bytes. Its SHA256 is:

```text
879024BED6FB89C218F6C75A46119F95FC068A64849BC74DEDF0851D3FD210A6
```

Builder `0.8.0-xbox-hd-re3-1` checks the archive and 92 allowlisted source-file
hashes. It merges `gta3`, `xbox_models`, `xbox_hd_textures`, `xbox_vehicles`,
with later archives taking precedence. Four empty base placeholders are omitted.
The result contains 4,051 archive entries, 62 vehicle DFFs and 101 vegetation
names. DFFs retain their authored bytes; TXDs are compressed/mip-capped at
1024 where supported by the converter. Input files remain untouched.

Every output file has a hash in `BUILD_INFO.txt`. Validation checks all hashes,
the exact output file inventory and IMG directory structure. Preparation builds
in temporary directories and replaces only folders marked as generated profiles.
Explicit local input rebuilds an existing generated profile. Without explicit
input, only a complete profile from this builder/source version is reused.

Device installers stage and verify file sizes and every manifest hash before
replacing a profile. They discover the build wizard's SDK at
`C:\LCVRBuild\.android-sdk` on Windows or `$HOME/LCVRBuild/.android-sdk` on
Linux. For a custom SDK location, pass `-AndroidSdk` / `--android-sdk` with
its root folder (the directory containing `platform-tools`). Saves, settings and the original game files are preserved.
Direct installation of a prepared profile remains available:

```powershell
.\tools\install-modern-models.ps1 -Profile Xbox -ModernDir "C:\Games\GTA3\modelsets\xbox"
.\tools\install-modern-models.ps1 -Profile Modern -ModernDir "C:\Games\GTA3\modelsets\modern"
```

```sh
./tools/install-modern-models.sh --profile xbox --modern-dir "$HOME/Games/GTA3/modelsets/xbox"
./tools/install-modern-models.sh --profile modern --modern-dir "$HOME/Games/GTA3/modelsets/modern"
```

## In-game effects and boundaries

`VR MENU > SETTINGS > MODEL ASSETS` offers a preset and per-category choices.
Classic forces retail assets. A non-Classic preset enables category mixing.
The Xbox startup default requests Xbox world, vehicles, peds and weapons, with
Classic vegetation. Existing saved choices take precedence. The existing Modern
startup defaults request Modern world/weapons and Classic vehicles/peds/vegetation.

| Category | What follows the selected profile |
|---|---|
| World / buildings | Buildings, roads, props, LODs, radar dictionaries; matching map IDE definitions, IPL placements and map COL files; `models/coll/generic.col`; HUD, fonts, menu, particles, controller prompt dictionaries and splash textures. |
| Vehicles | Cars, emergency vehicles, boats, aircraft and other vehicle DFF/TXD entries; loose `models/generic/wheels.dff`. Retail vehicle collision files remain in use, including train/boat collision records. |
| Pedestrians | Player/pedestrian DFF/TXD entries recognized by the engine, plus `3d` cutscene actor entries. Existing animation, body IK and ragdoll systems still apply. |
| Weapons | GTA III weapon IDs, including projectiles/bombs, and their associated dictionaries. Existing VR weapon behavior still applies. |
| Vegetation | The manifest identifies objects using the pack's authored tree dictionaries. Xbox and Modern can both be selected explicitly; Classic remains the recommended starting choice pending performance checks. |

Streaming filters DFFs by engine model type, weapon ID and vegetation manifest.
A texture dictionary shared by categories falls back to retail if its owners
use different profiles. Unreferenced dictionaries follow World, except `3d`
cutscene actors. Missing files fall back to retail. `models/generic.txd` is
shared by roads, props and trees and follows World, so Classic vegetation can
still receive those shared HD textures. World map definitions and collisions
follow World as a unit; this can change scene placement and collision behavior.

Both new profiles use the Xbox VR calibration namespace for weapon grips,
lasers/muzzles, seats, steering controls and radio placement. Old Modern geometry
offsets are inappropriate for this new source. Previously stored Xbox overrides
remain active; their fit with the new pack still needs visual checking.

Excluded from preparation: PC EXE/DLL/PDB files, re3 settings, controller mapping
configuration, bundled save files, mission scripts, optional audio, GXT language
files, skins, and Neo renderer configuration. Selecting this pack does not
activate the PC Neo renderer or import its graphics settings, freeroam scripts
or saves. Native Quest rendering, VR UI and gameplay settings remain authoritative.

## Validation and remaining headset work

The local source archive matched ModDB's published MD5 and the pinned SHA256.
Both complete profiles were built from it; Windows PowerShell preparation,
Bash cache reuse and a fresh Bash Modern build passed. Focused tests cover category routing, shared TXDs,
Classic fallback, calibration selection, failed conversion preservation,
archive/source validation, extraction exclusions and blocked download responses.
The full ARM64 debug APK compiled and updated the personal Quest 3 device.
The broader source gate finished with 55 passing suites and four pre-existing
fixture failures; see [the validation report](../../testreports/XBOX_HD_ASSET_PREPARATION.md).
Generated pack assets were not transferred in that validation run.

These checks do not establish visual compatibility or frame-time performance.
Before release, check both eyes during loading, every district/LOD, collisions,
radar/HUD/font glyphs, vehicles and wheels, player/pedestrian skinning and ragdolls,
cutscenes, every weapon grip/muzzle/scope, mixed categories, and sustained memory,
GPU and thermal behavior. The PC pack's larger textures, changed world geometry
and changed collision data expand validation well beyond vehicles.
