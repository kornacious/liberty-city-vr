# Classic, Modern and Xbox model assets on Quest

The Quest port supports the same category-based model overlay shell as PC:

- World
- Vegetation
- Vehicles
- Peds
- Weapons

Classic vegetation is mandatory in the tested Quest build. HD tree models
can be dramatically more expensive on a mobile GPU even when draw-call counts
look modest, so the one-button builder does not download or use the separate
vegetation pack.

## One-button download, build and installation

The player needs only:

- a legal original GTA III PC installation;
- the Quest connected by USB with the computer authorized;
- the APK already installed by `BUILD_AND_INSTALL.bat`;
- about 4 GB of download bandwidth and at least 24 GB of temporary free space.

Double-click `INSTALL_MODERN_MODELS.bat` and choose the original GTA III
folder. The wizard downloads and pins the exact tested
[GTA VC HD + Weapons](https://drive.google.com/file/d/1Swe1dVWDnKz8ad51y8L0ihPWVCxmFRYj/view)
and [Mods / Atmosphere](https://drive.google.com/file/d/1y9KpKjLSna76bjz1Lf2DzP0G4AnkN_2d/view)
archives. It then extracts, builds, validates and installs the overlay without
requiring 7-Zip, Git, Android Studio or any manual ADB command. Downloads are
cached under `C:\LCVRBuild\modern-assets` and resume after interruption. A
second run reuses verified downloads and extractions. If a complete overlay was
already built, it skips downloading, extracting and rebuilding and retries only
the Quest installation. When the bundled builder is updated, an older overlay
is rebuilt automatically; the old Quest folder is replaced only after the new
copy and the wheel asset hashes have been verified.

No third-party assets are stored in this source kit. The generated directory
contains original game and external mod data and must not be redistributed.
The completed overlay contains at least:

```text
modelsets/modern/models/gta3.img
modelsets/modern/models/gta3.dir
modelsets/modern/models/generic/wheels.dff
modelsets/modern/models/generic/wheels.txd
modelsets/modern/vegetation_models.txt
```

The LibertyCity/HD vegetation archive is not part of this workflow. The small
`vegetation_models.txt` file contains only names used by the runtime to select
the original Classic tree entries. Matching Modern vegetation DFF entries
are physically removed from the generated archive as an additional fallback.

The wizard installs the complete folder at:

```text
/sdcard/Android/data/com.libertyvr.quest/files/gamedata/modelsets/modern
```

The installer stages and verifies the complete folder before replacing any
previous Modern overlay. Wait for `DOWNLOAD, BUILD AND QUEST INSTALL COMPLETED`,
then fully restart the game.

## Advanced manual transfer

If an overlay was already generated, it can be uploaded without rebuilding:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\install-modern-models.ps1 -ModernDir "C:\path\to\modern"
```

The commands below are a lower-level fallback.

The Windows wizard normally installs ADB under
`C:\LCVRBuild\.android-sdk\platform-tools\adb.exe`. For example, in PowerShell:

```powershell
$adb = "C:\LCVRBuild\.android-sdk\platform-tools\adb.exe"
$modern = "C:\Games\Liberty City VR\modelsets\modern"
& $adb shell am force-stop com.libertyvr.quest
& $adb shell mkdir -p /sdcard/Android/data/com.libertyvr.quest/files/gamedata/modelsets
& $adb push $modern /sdcard/Android/data/com.libertyvr.quest/files/gamedata/modelsets
```

The final device path must not contain `modern/modern`. Verify it with:

```powershell
& $adb shell ls /sdcard/Android/data/com.libertyvr.quest/files/gamedata/modelsets/modern/models
& $adb shell ls /sdcard/Android/data/com.libertyvr.quest/files/gamedata/modelsets/modern/vegetation_models.txt
```

In the headset open `VR MENU > SETTINGS > MODEL ASSETS`. Category changes are startup
choices: select the desired categories and fully restart the game. Keep
Vegetation on Classic.

If the manifest is absent, the menu reports Vegetation as unavailable and the
loader forces that category to Classic instead of guessing model names.

## Xbox assets profile

The normal preparation wizard also prepares the **Xbox Assets** profile
(vehicles, characters and pedestrians, weapons, and world/environment) as a
separate profile. The kit includes only download metadata and tools, not these
model assets. The user-supplied archive contains no separate license/readme; it is not repackaged
or redistributed by this project.

The profile contains Xbox DFF/TXD pairs, their collision data and shared assets.
The main models use PC-compatible RenderWare data. The builder preserves
geometry and COLL bytes and uses the existing compressor on staged texture
copies. No handling, IDE, scripts or base game archives are replaced.

```text
modelsets/xbox/models/gta3.img
modelsets/xbox/models/gta3.dir
modelsets/xbox/models/coll/vehicles.col
modelsets/xbox/models/generic/wheels.dff
modelsets/xbox/models/generic/wheels.txd
modelsets/xbox/vehicle_models.txt
modelsets/xbox/BUILD_INFO.txt
```

The archive and all 217 selected input files are checked against pinned SHA256
hashes. Complete outputs are checked before cache reuse and before upload;
the small Xbox profile is also hashed on the headset before activation. A
failed transfer leaves the previous profile active. The separate Modern folder,
game data, saved games and profile settings are preserved.

For Xbox only, without downloading the larger Modern packs:

```powershell
.\INSTALL_MODERN_MODELS.bat -XboxOnly
```

```sh
./INSTALL_MODERN_MODELS.sh --xbox-only
```

Add `-BuildOnly` / `--build-only` to build without device access. A previously
downloaded archive can be supplied with `-XboxArchive` / `--xbox-archive`;
it must match the pinned version. Use `-SkipXbox` / `--skip-xbox` for the older
two-pack Modern workflow. The existing `-OutputDir` / `--output-dir` selects
the Xbox output when used with XboxOnly; its leaf directory must be `xbox`.
Both preparation paths reuse the same `modern-assets` work directory.

Linux requires Python 3, a C++ compiler and a RAR-capable extractor (`bsdtar`,
7-Zip or `unrar`). The bundled compressor is used on Windows. No executable
from the downloaded model archive is run.

For a profile that is already built:

```powershell
.\tools\install-modern-models.ps1 -Profile Xbox -ModernDir "C:\path\to\xbox"
```

```sh
./tools/install-modern-models.sh --profile xbox --modern-dir /path/to/xbox
```

After installation, choose **VR MENU > SETTINGS > MODEL ASSETS > VEHICLES > XBOX** and
restart the process. Xbox can be combined with Modern world textures/weapons,
or used alone with the Classic base. Missing archive entries fall back to the
base game; non-vehicle models are never selected from the Xbox archive. The
installer does not select profiles automatically.
