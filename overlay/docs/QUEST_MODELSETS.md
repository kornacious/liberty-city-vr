# Classic, Modern and Xbox model assets on Quest

The runtime exposes World, Vegetation, Vehicles, Peds and Weapons categories
under `VR MENU > SETTINGS > MODEL ASSETS`. Select a non-Classic preset before
mixing overlay categories; the Classic preset forces every category to the base
installation. Changes require a full process restart.

The Quest baseline requests Modern world and weapons, with Classic vegetation,
vehicles and pedestrians. Missing overlays fall back to the base installation.
Existing saved choices take precedence after the one-time baseline migration.
Keep vegetation Classic: HD trees can be expensive on the mobile GPU. The Xbox
preset keeps vegetation Classic and requests Xbox for the other categories.

No third-party pack is included in the source kit. Players supply their own
legally obtained assets; generated overlays must not be redistributed.

## Xbox preparation and installation

Despite its name, `INSTALL_MODERN_MODELS.bat` / `INSTALL_MODERN_MODELS.sh`
currently defaults to the Xbox profile. It requires an original GTA III PC
installation and, unless building only, an installed Quest APK and an authorized
USB connection.

On Windows, the wizard tries an automatic pack download and can ask for a local
archive or extracted folder if it is unavailable. On Linux, supply a local
archive or extracted folder; the current Python preparation path does not
download a pack automatically. Explicit local-source examples:

```powershell
.\INSTALL_MODERN_MODELS.bat -GameDir "C:\Games\GTA3" -XboxSource "C:\Packs\Xbox"
```

```sh
./INSTALL_MODERN_MODELS.sh --game-dir "$HOME/Games/GTA3" --xbox-source "$HOME/Packs/Xbox"
```

Use `-XboxArchive` / `--xbox-archive` for an archive instead of a folder.
`-XboxOnly` / `--xbox-only` explicitly selects the default profile.
`-BuildOnly` / `--build-only` builds without device access. `-OutputDir` /
`--output-dir` selects the generated profile folder; its leaf name must be
`xbox`. The default output is the selected PC game's `modelsets/xbox` folder.
Work files are stored in `C:\LCVRBuild\modern-assets` on Windows or
`$HOME/LCVRBuild/modern-assets` on Linux unless overridden.

Windows uses the bundled texture compressor. Linux needs Python 3, a C++
compiler, and an extractor suitable for the supplied archive. Build the
compressor once:

```sh
cd tools/modelsets
g++ -O2 -o txdcompress txdcompress.cpp
```

The generated profile includes:

```text
modelsets/xbox/models/gta3.img
modelsets/xbox/models/gta3.dir
modelsets/xbox/vehicle_models.txt
modelsets/xbox/BUILD_INFO.txt
```

Pedestrian and weapon manifests and loose wheel assets depend on the input
pack. A collision file may be retained by the builder, but the runtime keeps
base `models/coll/` files rather than redirecting them to an overlay.

The current workflow accepts user-selected sources and does not enforce a
pinned source archive hash or a fixed count of source files. Xbox validation
checks the generated archive pair and directory structure. The device installer
checks copied file sizes and SHA256 hashes for `gta3.img` and `gta3.dir` before
replacing the previous profile. Those transfer checks do not establish the
source pack's provenance or visual compatibility.

After installation, select the Xbox preset or the desired category mix in
`VR MENU > SETTINGS > MODEL ASSETS`, then fully restart. The installer preserves
saved profile choices; installing files does not select the profile.

Builder 0.7.1 selects the main `Fixed Xbox Vehicles` folder rather than merging
optional variants. When its police dictionary identifies Vice City liveries,
ambulance, Enforcer, fire truck, police car and police helicopter use matching
base GTA III geometry and textures. These are Classic fallbacks until compatible
GTA III Xbox replacements are supplied. The Predator keeps its supplied model;
opaque C888 textures ignore their unused alpha byte during compression.
Older generated profiles must be rebuilt with the updated compressor. The
preparation wizard rejects older builder versions instead of reusing them.

## Modern preparation limitations

The old two-pack automatic Modern workflow is no longer implemented by the
current wrapper. On Windows, the Modern branch passes a `modern` output folder
to a builder that requires an `xbox` folder and fails. On Linux, the Modern/Both
branches do not build or install a Modern overlay, even if the wrapper prints
a completion message. Do not use `-ModernOnly`, `-SkipXbox`, or their Unix
counterparts as a working Modern preparation route in this revision.

The older `tools/modelsets/build-modern-modelset.ps1` and `.sh` remain in the
repository, but contain Vice City-specific input assumptions. They are not a
verified GTA III replacement for the current wrapper. An already generated,
compatible Modern overlay can still be installed using the commands below.

A Modern overlay requires `models/gta3.img`, `models/gta3.dir`,
`vegetation_models.txt` and `BUILD_INFO.txt`. Preserve any generated loose wheel
assets under `models/generic/`. If the vegetation manifest is missing, the
runtime makes that category unavailable rather than guessing model names.

## Installing an existing overlay

Use the installer for the matching profile:

```powershell
.\tools\install-modern-models.ps1 -Profile Xbox -ModernDir "C:\path\to\xbox"
.\tools\install-modern-models.ps1 -Profile Modern -ModernDir "C:\path\to\modern"
```

```sh
./tools/install-modern-models.sh --profile xbox --modern-dir /path/to/xbox
./tools/install-modern-models.sh --profile modern --modern-dir /path/to/modern
```

These installers stage the copy and verify it before replacing the profile
folder. They preserve saves, settings, the base game and the other overlay.
Final device paths are:

```text
/sdcard/Android/data/com.libertyvr.quest/files/gamedata/modelsets/xbox
/sdcard/Android/data/com.libertyvr.quest/files/gamedata/modelsets/modern
```

Each profile must directly contain its `models` directory; avoid nesting
`xbox/xbox` or `modern/modern`. Fully restart after copying or changing categories.
See [QUEST_PORT.md](QUEST_PORT.md) for the base game's storage bootstrap and
[BUILDING.md](../../BUILDING.md) for the APK installation workflow.
