# Build and manage Liberty City VR

This guide builds **0.8.0**, the release candidate ready for public testing and
playing with finishable main missions. Android uses version code **800**; the
APK and native About/menu version strings are **0.8.0**. See the
[changelog](CHANGELOG.md#080--release-candidate) for the release changes.

The supported entrypoint on Windows and Linux is the .NET 10 console manager:

```text
dotnet run --project tools/manager -c Release
```

Launch it with `MANAGE.bat` on Windows or `./MANAGE.sh` on Linux. The launchers
prefer the published manager in `out/manager-windows` or `out/manager-linux`,
falling back to the .NET 10 SDK. Keep the complete source kit together. The old
per-action root BAT shortcuts have been removed; select actions in the manager.
Standalone PowerShell, Bash and Python action entrypoints have been integrated into the manager.
See [the manager guide](tools/manager/README.md) for publishing standalone Windows
and Linux binaries, dependencies, auxiliary runtimes and command-line use.

## Configure once

Open **Settings** in the left navigation panel. Choose the source repository,
workplace, optional legally owned GTA III PC installation, Android SDK, JDK,
ADB, ffprobe and CMake paths. Every path has a terminal file/directory browser.
Blank toolchain paths use the workplace cache or normal discovery. **Save**
stores settings atomically; **Cancel** discards edits and returns to navigation.

Defaults are `C:\LCVRBuild` on Windows and `~/LCVRBuild` on Linux. Keep the
workplace separate from the repository; short Windows paths avoid native build
path limits. Settings live in `%APPDATA%\LibertyCityVR` or `~/.liberty-city-vr`.
Action logs are stored in that directory's `logs` folder. Device identifiers
are never printed or saved in settings/logs.

## Build and install

Choose **Build and install**, configure the options on the right, then
**Perform**. Build only skips installation. Release requires your private
signing configuration; debug uses the local Android debug key. Optional retail
copying requires a complete local GTA III PC installation containing `data`,
`TEXT`, `anim`, `models`, `movies`, and `Audio`, including `sfx.RAW` and `sfx.SDT`.
The manager copies only the required folders and optional `mp3`, `skins`, `txd`.
Game files and saves remain local and must never be committed or uploaded.

**Convert and compress radio stations** is optional and off by default. It uses
local FFmpeg/ffprobe (configure their executable paths in Settings) to encode the
nine native music stations as AAC-LC, 128 kb/s stereo at 48 kHz in seekable `.m4a`
containers. The game prefers these files directly, avoiding whole-station WAV
decoding at tune time. AAC-LC/M4A is supported by Android's media framework
([supported formats](https://developer.android.com/media/platform/supported-formats)).

- Copy game data only: copies the original WAVs and removes their old compressed
  overrides so the game uses the WAVs again.
- Convert only: pulls existing station WAVs from the selected headset into a
  temporary local workplace folder, converts them and transfers the replacements.
- Copy and convert: converts the PC station WAVs locally and sends only the
  compressed stations. PC originals are preserved.

All selected stations are converted and decoded for validation before any station
is replaced. Each upload is hash verified and renamed into place before its
headset WAV is removed. A failure preserves that station's WAV; previously
completed replacements remain usable. Temporary PC files are cleaned up. Missing
station WAVs are skipped, so conversion can be rerun after a completed or partial
installation. Mission dialogue, sound effects, custom music and saves are outside
the conversion. Build only skips conversion and all headset operations.

The x64 tool bootstrap retains the original verified versions:

- Git for Windows 2.55.0.3 (Linux requires installed Git)
- Eclipse Temurin JDK 21.0.11+10
- Android command-line tools 15859902
- Android Platform 35, Build Tools 34.0.0 and Platform Tools
- Android NDK 27.2.12479018 and CMake 3.22.1
- Gradle 8.13

Portable archives are SHA256 checked before extraction. Installing missing SDK
packages requires explicit acceptance of Google's SDK licenses in the action
configuration. Existing configured tools are reused. No original game data is
fetched by the manager.

The required public source is <https://github.com/mrxenginner/re3>, clean at
`b050e1cc63b1869e6b514ab2f1a00ef4969e0c84`. The manager obtains or verifies that
checkout, copies it into a new private build directory, applies `patches/*.patch`
in filename order, overlays the Quest files and installs the bundled librw fork.
Previous builds and the clean upstream checkout are preserved.

The APK is produced at:

```text
<new-build>/android/app/build/outputs/apk/debug/app-debug.apk
```

A release build uses the corresponding `release/app-release.apk` path. The
SHA256 and actual build path appear in the action log. Assembled source contains
re3-derived code and is a private local build product; do not redistribute it.

After a successful build, the manager discovers authorized devices locally,
queries their model with a deadline and installs only on a responsive Quest 3.
Multiple eligible devices require a numbered choice. Missing ADB or an eligible
device skips deployment and leaves the successful build available. Installation
uses `install -r` with an explicit private target. A signing/version/install
failure stops; the manager never uninstalls the app or clears saves/game data.

A full installation bootstraps application storage through its save provider,
optionally copies your game data, installs the port's replacement frontend/text
assets and bundled hands, verifies transfers and leaves the game stopped.
Launch it from the headset when ready.

## Other pages

**Vehicle editor** builds and tests the C++ desktop editor, installs it into the
workplace (or chosen output folder), and optionally creates a ZIP. Windows
requires Visual Studio/Build Tools with the Desktop C++ workload. Linux needs a C++ compiler,
Ninja, OpenGL/X11/GLFW development dependencies. Device transfer is routed through
the packaged manager; its embedded bridge needs configured Python 3 and ADB.

**Manage music** lists the connected headset's music and shows selected-file
metadata, size, format, bitrate, channels and sampling rate. Drag entries or use
Up/Down (Alt+Up/Down) to insert them at a new playlist position. **Perform** saves
the order; **Cancel** discards pending changes. Refresh reloads the headset library.
New imports append after ordered tracks. Concurrent library changes require a
refresh before saving. The game preserves the playing file and position on rescan.

The optional Import field accepts a path or a selection of multiple audio files
and folders from Browse. In the browser, Ctrl-click or Space chooses individual
entries and Alt-click extends a range; only chosen entries are imported. It uses
ffprobe to validate codec, channels, sample rate, size and duration before any
transfer. Validation only needs no headset. Imports use provider staging and
SHA256-verified commits, skip duplicates and preserve existing tracks. Optional
radio-board export requires a new output filename.

**Original Xbox assets** accepts only your local GTA III US 1.0 original Xbox
`.iso` or `.xiso.iso`. No game image or asset pack is downloaded. The Manager
reads the disc filesystem, verifies retail file fingerprints, decompresses and
converts native assets using the bundled converter source, and validates a
complete `modern` overlay. CMake, Ninja and C++ build tools are required.
Build only skips headset access. Installation verifies staged files and activates
the profile with rollback. See [the asset guide](overlay/docs/QUEST_MODELSETS.md).

**Repair game files** replaces the port's allowlisted GXT and frontend/shared
TXD files when menu entries show missing labels. It requires existing game data
and takes assets from the pinned re3 `gamefiles` folder or the folder you select.
It does not replace retail IMG archives, optional model overlays or saves.

**Update** requires clean master and a fast-forward from official master. It
checks the version floor and asset-update marker, rebuilds the fetched manager,
then builds and updates only the APK. Dry run performs local checks without
network/build/device access. Build only produces the APK without installing.
For release signatures set `LIBERTYVR_RELEASE_SIGNING_PROPERTIES` to your private
signing properties and select Release. Keep keys and signing values out of Git.

**Transfer save** offers Quest → PC and PC → Quest, a multi-select checkbox list
for manual slots 1–8 and Autosave, and a PC
save-folder browser. It uses the selected slot's `GTA3sfN.b`, validates the GTA III
compatible save format/checksum, backs up existing destinations under the
workplace's `save-transfer/backups`, and verifies Quest read-back. Both directions
are native C# and use ADB on Windows/Linux; no Python, PowerShell or Bash is
needed. Close both games before performing a transfer. Legacy save-transfer
modules are hidden from Tools and tests. See [save transfer](overlay/tools/saves/README.md).

**Tools and tests** exposes the auxiliary authoring, model, capture and
regression modules. They are embedded into the application and materialized in
a fresh local source-only workplace snapshot when selected. Python 3, PowerShell
7, Bash and specialised dependencies remain necessary for those modules. Legacy
module names are resource IDs, not standalone files in the checkout. Historical
installer fixture tests cover their compatibility sources; the active C# manager
has its own native regression checks.

## Maintainer verification

```text
dotnet build tools/manager -c Release
dotnet run --project tools/manager/tests -c Release
dotnet run --project tools/manager -c Release -- --tool tools/tests/test-patch-series.py -- C:/LCVRBuild/re3-public-b050e1c
dotnet run --project tools/manager -c Release -- --privacy install-hooks
dotnet run --project tools/manager -c Release -- --privacy history
```

The optional ASan Android shell wrapper is generated only in the private build
from the manager resource. Player builds exclude it. Inspect APK packaging using
the embedded `tools/tests/test-asan-apk.ps1` module (`-Apk`, optional `-ExpectAsan`).
No manager binary, APK, assembled tree, downloaded dependency or game file belongs
in the source repository. See [RELEASING.md](RELEASING.md).
