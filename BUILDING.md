# Build the Quest APK yourself

This repository does not distribute an APK or a complete re3 tree. The steps
below produce a personal debug-signed APK on your own machine.

## Easiest Windows method

1. Connect the Quest, enable USB debugging, and accept the authorization prompt
   inside the headset.
2. Double-click **`BUILD_AND_INSTALL.bat`** in this repository.

No Android Studio, system Git, system Java or manually prepared Android SDK is
required for this route. The wizard downloads portable Git, JDK 21, Android
command-line tools, Platform 35, Build-Tools 34.0.0, Platform-Tools, NDK
27.2.12479018, CMake 3.22.1 and Gradle 8.13. The user must accept Google's SDK
licenses when prompted.

## Easiest Linux method

1. Connect the Quest, enable USB debugging, and accept the authorization prompt
   inside the headset.
2. Run **`./BUILD_AND_INSTALL.sh`** in this repository.

The wizard needs `git`, `curl`, `unzip` and `tar` on `PATH` (on CachyOS/Arch:
`sudo pacman -S git curl unzip`). It then discovers or downloads JDK 21
(pinned official Eclipse Temurin 21.0.11+10, SHA256-verified), the Android
command-line tools (pinned, SHA256-verified) and Gradle 8.13 (pinned,
SHA256-verified), and installs Platform 35, Build-Tools 34.0.0, Platform-Tools,
NDK 27.2.12479018 and CMake 3.22.1 through `sdkmanager`. A system JDK 21
(`JAVA_HOME`) or Android SDK (`ANDROID_HOME`/`ANDROID_SDK_ROOT`) is used when
present. The user must accept Google's SDK licenses when prompted.

The Linux entry points mirror the Windows ones:

| Windows | Linux |
|---|---|
| `BUILD_AND_INSTALL.bat` | `BUILD_AND_INSTALL.sh` |
| `INSTALL_MODERN_MODELS.bat` | `INSTALL_MODERN_MODELS.sh` |
| `REPAIR_GAME_FILES.bat` | `REPAIR_GAME_FILES.sh` |

The model builders need one extra step on Linux: the texture compressor
is built from source once, next to the other model-set tools:

```sh
cd tools/modelsets
g++ -O2 -o txdcompress txdcompress.cpp
```

(`txdcompress` is gitignored; `txdcompress.exe` is the committed Windows build.)

The asset wizard currently defaults to Xbox. Linux needs a local Xbox archive
or extracted folder; the Modern preparation branches are incomplete. See
[QUEST_MODELSETS.md](overlay/docs/QUEST_MODELSETS.md) for current commands and
limitations before preparing optional assets.

Do not launch `tools\build-and-install.ps1` directly. The `.bat` wrapper keeps
the window open and always prints the location of the persistent diagnostic
log: `%TEMP%\LibertyCityVR-Build-And-Install.log`. If the build fails, attach that
file instead of sending a video of a closing console window.

The wizard downloads and SHA256-verifies pinned official portable Git, Eclipse
Temurin JDK 21, Android command-line tools and Gradle. It then installs the
required SDK packages, obtains the exact tested public re3 commit, assembles
the private tree, builds a personal debug APK, installs it with
`adb install -r`, bootstraps application-owned storage, and asks for the legally
owned GTA III folder when game data must be copied. It never uninstalls the
app or clears its data silently. If Android reports that an older installation
was signed with a different key, the wizard explains that replacement can erase
saves and game data and asks for explicit confirmation before uninstalling it.
It validates the supplied GTA III PC folder before deleting the old app and
then restores the required game data after installing the new build.
It never uploads anything or bundles an APK/GTA data into this repository.

If somebody downloaded only `BUILD_AND_INSTALL.bat`, the BAT downloads and
extracts the complete public source kit automatically before starting the
wizard. Extracting the repository ZIP manually is still supported.

On Linux, run `./BUILD_AND_INSTALL.sh` instead. It mirrors the BAT: it keeps
the terminal open on error, always prints the location of the persistent
diagnostic log (`$TMPDIR/LibertyCityVR-Build-And-Install.log`, usually
`/tmp/LibertyCityVR-Build-And-Install.log`), and downloads the complete public
source kit if only the script itself was downloaded. Do not launch
`tools/build-and-install.sh` directly: the wrapper is the supported entry
point.

Advanced command-line example:

```powershell
.\BUILD_AND_INSTALL.bat -GameDir "C:\Games\Grand Theft Auto III"
```

```sh
./BUILD_AND_INSTALL.sh --game-dir "$HOME/Games/Grand Theft Auto III"
```

Use `-BuildOnly` / `--build-only` to stop after producing the APK, or
`-SkipGameData` / `--skip-game-data` when the Quest already contains the
required data. The manual steps below remain the reference and troubleshooting
path.

## 1. Install the toolchain

Tested versions:

- Git
- JDK 21
- Android SDK Platform 35 and Build Tools 34.0.0
- Android NDK `27.2.12479018`
- CMake 3.22.1
- Gradle 8.13, or Android Studio with an equivalent Gradle installation
- Android Platform Tools (`adb`)

On Windows, use short working paths such as `C:\LCVRBuild\re3-public-b050e1c` and
`C:\LCVRBuild\liberty-city-vr-build`; native object paths can otherwise exceed Windows
path limits.

## 2. Obtain the required re3 source

The source is supplied by the public upstream project, not by this repository:

```powershell
git clone --no-checkout https://github.com/mrxenginner/re3.git C:\LCVRBuild\re3
git -C C:\LCVRBuild\re3 checkout --detach b050e1cc63b1869e6b514ab2f1a00ef4969e0c84
```

Tested build-base repository: <https://github.com/mrxenginner/re3>.

Use a clean checkout at that exact commit. A different revision or local edits
can make a patch fail; the script stops instead of silently producing a mixed
tree.

## 3. Assemble the private build tree

From this repository on Windows:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\assemble.ps1 `
  -Re3 C:\LCVRBuild\re3 `
  -Out C:\LCVRBuild\liberty-city-vr-build
```

Linux/macOS:

```sh
./tools/assemble.sh /path/to/re3 /path/to/liberty-city-vr-build
```

The output directory must not already exist. The script copies re3, applies
the Quest and runtime patch layers, overlays only new port files, and installs
the bundled librw fork under `vendor/librw`. The original checkout is not
modified. The assembled directory contains re3-derived source and is a local
build product: do not commit or redistribute it.

## 4. Build a personal debug APK

Windows PowerShell:

```powershell
$env:JAVA_HOME = "C:\path\to\jdk-21"
$env:ANDROID_HOME = "C:\path\to\Android\Sdk"
$env:ANDROID_SDK_ROOT = $env:ANDROID_HOME
Set-Location C:\LCVRBuild\liberty-city-vr-build\android
gradle wrapper
.\gradlew.bat :app:assembleDebug
```

Linux/macOS:

```sh
cd /path/to/liberty-city-vr-build/android
gradle wrapper
./gradlew :app:assembleDebug
```

Output:

```text
android/app/build/outputs/apk/debug/app-debug.apk
```

The debug build is signed with your machine's local Android debug key and is
sufficient for personal sideloading. `assembleRelease` requires private signing configuration unless the unsigned build option below is explicitly selected. Never commit signing keys or values.

## 5. Install without deleting existing data

Enable Developer Mode and USB debugging on the headset, then verify it appears:

```powershell
adb devices
adb install -r C:\LCVRBuild\liberty-city-vr-build\android\app\build\outputs\apk\debug\app-debug.apk
```

`-r` updates the application in place and preserves its settings and saves.
Do not uninstall or clear application data when you want to retain them.

On a completely fresh install, bootstrap the external-data layout through the
APK's narrow save provider before copying any files:

```powershell
adb shell content query --uri content://com.libertyvr.quest.saves/slot/1 `
  --projection _display_name:_size
```

The expected result is a zero-byte `GTA3sf1.b` row. This creates the root and
the documented retail-data child directories as the application UID. Do
**not** pre-create `/sdcard/Android/data/com.libertyvr.quest/files` with
`adb shell mkdir -p`: on current Horizon OS that can leave it owned by the ADB
shell and the game immediately exits with `cannot enter game data directory`.

## 6. Supply your own game data

The APK contains no GTA III data. Copy the required directories from
your legally owned PC installation to:

```text
/sdcard/Android/data/com.libertyvr.quest/files/gamedata/
```

The expected top-level data includes `data`, `TEXT`, `anim`, `txd`, `skins`,
`mp3`, `movies`, `models` and `Audio`. Saves/settings live separately under:

```text
/sdcard/Android/data/com.libertyvr.quest/files/userfiles/
```

After the provider bootstrap has created the application-owned root, Android
scoped storage may still require individual child directories. At that point
they may be created with `adb shell mkdir -p`. Push each top-level folder into
`gamedata`, not into another folder of the same name. Detailed examples are in
[overlay/docs/QUEST_PORT.md](overlay/docs/QUEST_PORT.md).

Retail data alone is not enough. re3 replaces several stock assets and its
frontend reads strings and button icons that a 2001 installation never shipped;
without them the OPTIONS menus print `FET_GFX missing`, `FED_AAS missing` and
the like where the labels belong. The wizard copies them automatically. Doing
it by hand, from the assembled tree:

```powershell
$data = "/sdcard/Android/data/com.libertyvr.quest/files/gamedata"
adb push C:\LCVRBuild\liberty-city-vr-build\gamefiles\TEXT $data
adb push C:\LCVRBuild\liberty-city-vr-build\gamefiles\models $data
```

An installation made before the wizard did this can be repaired without
rebuilding: double-click `REPAIR_GAME_FILES.bat` (Windows) or run
`./REPAIR_GAME_FILES.sh` (Linux). It replaces selected GXT files and
frontend/shared TXDs under `models/`. Saves, settings, model archives and
optional model overlays are left alone.

The VR hand meshes are original MIT-licensed port assets, not GTA data and not
part of the APK. Copy them once from the assembled tree:

```powershell
adb shell mkdir -p /sdcard/Android/data/com.libertyvr.quest/files/gamedata/models
adb push C:\LCVRBuild\liberty-city-vr-build\gamefiles\models\vrhands `
  /sdcard/Android/data/com.libertyvr.quest/files/gamedata/models
adb shell ls /sdcard/Android/data/com.libertyvr.quest/files/gamedata/models/vrhands
```

The final directory must directly contain `BigHandLeft.uxrh`,
`BigHandRight.uxrh` and `BigHandsAlbedo.png`; avoid a nested
`vrhands/vrhands` directory.

## Troubleshooting

- `patch does not apply`: reset re3 to the exact tested commit and update its
  submodules; do not assemble from a modified checkout.
- `ninja: mkdir ... No such file or directory`: use shorter source/output paths.
- SDK/NDK not found: set `ANDROID_HOME`/`ANDROID_SDK_ROOT` or create
  `android/local.properties` with `sdk.dir=...`.
- release signing error: check your private release configuration. A personal
  debug build uses a different key and cannot update a release-signed app.
- app exits or stays in theater mode: verify the game-data path and filename
  case, then inspect `adb logcat -s LibertyVR:V librw-vk:V`.

## Release and developer build options

A signed release may read its configuration from outside the source tree using
LIBERTYVR_RELEASE_SIGNING_PROPERTIES. The storeFile path is resolved relative to
that configuration file. Keep both configuration and keystore private.

For an unsigned release artifact, pass -PlibertyvrUnsignedRelease=true to
:app:assembleRelease and sign the APK separately. Release builds are not
debuggable. Neither path changes the source-only distribution policy.

The culling inspector and other developer-only diagnostics require the explicit
-PlibertyvrDevTools=true option. It is off by default for both debug and release
builds; ordinary player graphics settings are independent of it.

## Updating without downloading another source ZIP

Use the root `UPDATE.bat` / `UPDATE.sh` wrapper after the initial installation.
It uses the same work directory and tool caches as the build wizard. See the
[update instructions](README.md#updating-an-existing-installation) for Git/ZIP
behavior, work-directory selection, dry runs and building without installing.
Updates never launch the game or perform an uninstall fallback.

The default personal debug build retains the original wizard's signing
identity. For an installation signed with your release key, provide its
external configuration and select the release variant:

```powershell
$env:LIBERTYVR_RELEASE_SIGNING_PROPERTIES = "C:\Private\libertyvr-signing.properties"
.\UPDATE.bat -Release
```

```sh
LIBERTYVR_RELEASE_SIGNING_PROPERTIES="$HOME/private/libertyvr-signing.properties" ./UPDATE.sh --release
```

The configuration must reference the same signing key as the installed app.
Keep it and the keystore outside the source kit.
