# Liberty City VR Manager

Included with **Liberty City VR 0.8.5 — release candidate**, ready for public
testing and playing with finishable main missions. Quest build/install and update
read the Android version code (**805**) from the source kit. See the
[release changelog](../../CHANGELOG.md#085--release-candidate) for manager, music,
save-transfer and game changes since 0.8.0; earlier changes have their own entries.

Build and install includes an optional **Convert and compress radio stations**
checkbox. Configure FFmpeg and ffprobe in Settings. It converts locally to
AAC-LC/M4A for direct Quest playback, either from your PC game folder alongside
Copy game data or from the installed headset WAVs when Copy is off. Original PC
files are preserved; headset WAVs are removed after verified replacement.
Copy alone restores WAV playback. See [the build guide](../../BUILDING.md#build-and-install).

The interactive manager builds Quest or experimental Windows PCVR. Windows has
versioned local installation, isolated data preparation, launch and runtime diagnostics.
Developer CLI commands `--assemble RE3 OUTPUT` and
`--prepare-windows-data GTA OUTPUT` prepare sources and an isolated local asset
copy for the experimental [Windows CMake build](../../overlay/windows/README.md).
Assembly requires the clean pinned public re3 commit; data preparation refuses
existing or overlapping destinations and never accesses a headset. Updates use
[the renamed official repository](https://github.com/kornacious/liberty-city-vr).
Existing local checkout paths and saved settings remain valid after the rename.
See [platform scope](../../PLATFORMS.md).

The Windows/Linux project manager is a .NET 10 console application using
Terminal.Gui **2.5.0**. Its GTA III inspired palette uses midnight blue,
cream, menu cyan and HUD amber. Use a terminal of at least 80 × 24; 110 × 36
shows more configuration without scrolling.

From the repository root:

```text
dotnet run --project tools/manager -c Release
```

Windows users can double-click `MANAGE.bat`; Linux users run `./MANAGE.sh`.
Both launchers prefer the published executable in `out/manager-windows` or
`out/manager-linux`, otherwise they use the .NET 10 SDK. The Linux launcher also
works from another working directory and forwards command-line arguments.
The six per-action root BAT shortcuts have been removed; select actions in the
manager or pass `--page ACTION` to either launcher. Keep the complete checkout.

Choose an action on the left, configure it on the right, then **Perform**.
**Cancel** discards unperformed edits and returns focus to navigation. During
an action it cancels the tool process, waits for cleanup, then returns focus.
Only one action runs at a time. Tab, Shift+Tab, arrows, Enter and the mouse
work throughout the interface; Ctrl+Q quits after cancelling active work.
Forms scroll and every path field has a terminal file/directory browser.

## Windows PCVR

On a Windows x64 host, select **Windows PCVR (experimental)** on **Build and
install**. Quest remains the default for existing settings. Changing targets
preserves unsaved options; Cancel discards them. The other headset asset/music,
repair and update actions remain Quest workflows. Update PCVR by building and
installing its target again; the **Update** page still updates official main and Quest.

Configure Visual Studio Desktop C++ x64 tools, CMake (with CTest), Ninja and a
Vulkan SDK in **Settings**. The SDK must provide `Include/vulkan/vulkan.h`, x64
`Lib/vulkan-1.lib` and `Bin/glslc.exe`; a separate glslc path is optional. The
manager reads the Visual Studio compiler environment automatically. OpenXR uses
the pinned, SHA256-checked 1.1.58 source loader and includes its license notices.
Choose a local complete OpenXR-SDK source directory for offline builds, or allow
the existing pinned download/cache workflow. MSVC, CMake/Ninja and Vulkan SDK
installation are prerequisites; the manager does not download those tools for PCVR.

The Windows build assembles fresh sources, compiles Release and always runs
CPU/CLI checks. GPU and audio checks are opt-in because they require an active
Vulkan GPU/audio endpoint. Build only skips installation and data preparation.
It does not request Android licenses, build an APK, or invoke ADB.

For the first playable install, enable **Prepare local PCVR data** and select
your owned GTA III PC installation. Alternatively select an existing copy made
with `--prepare-windows-data`. The default directories are separate:

| Contents | Default under workplace |
| --- | --- |
| Versioned executables, loader and notices | `windows-pcvr/app/versions/<build-id>` |
| Current installation record | `windows-pcvr/app/current.json` |
| Local prepared game data and preferences | `windows-pcvr/data` |
| Saves, audio cache and game diagnostics | `windows-pcvr/user` |
| Private assembled sources and build outputs | `windows-pcvr-builds/<build-id>` |

Custom app, data and save directories must be absolute, separate from each other,
retail data and source/build trees, and contain no linked paths. Data preparation
requires a new destination and writes its readiness marker last. If interrupted,
inspect the partial copy locally and select a new destination; nothing is erased
automatically. Routine installs reuse prepared data and refresh only authored port
assets. They preserve preferences, saves and previous versions. The current install
record changes only after the new binaries and notices are validated. These
folders are local builds; no public binary package or Windows system installer is provided.

**PCVR diagnostics** checks the environment override (`XR_RUNTIME_JSON`) or active
x64 OpenXR registry runtime and its library, then runs Vulkan and XR session probes.
It never selects or changes the runtime. Connect through your PCVR software; an
ADB cable alone is insufficient. Missing headset, missing runtime/library, Vulkan
failure and probe timeout are reported separately. Probes time out after 60 seconds
and do not require game data. Passing them does not verify stereo presentation.

**Launch PCVR** checks the installed build, prepared data and runtime before
launching with the isolated data/save paths. The manager captures sanitized logs
and waits until the game exits. Quit in-game for a normal exit; Cancel forcibly
stops the game process. There is no manager-imposed playtime limit. The manager
can also run without build tools once the local package is installed.

The game opens a desktop eye preview with ImGui controls and F1–F12 shortcuts,
plus focused keyboard/mouse and XInput gamepad input. See the
[preview controls](../../overlay/windows/README.md#desktop-preview-and-input).
Preview preferences are separate from Quest settings and stored in the PCVR user directory.
The optional **Quest profile** field selects a captured Quest rendering workload;
blank uses native PCVR. Restart to change profiles. `--benchmark-collect OUTPUT`
copies recent allowlisted diagnostics locally; `--benchmark-compare LEFT.run.csv
RIGHT.run.csv` reports matched workload checks and per-device measured timings.
See [benchmark workflow](../../BENCHMARKS.md); desktop timings are not Quest estimates.
`--benchmark-inventory OUTPUT` compares local Quest/PCVR asset hashes;
`--benchmark-align-quest OUTPUT` creates a new verified Quest-aligned PCVR data/user
copy. Original data and saves are preserved; global launch paths remain unchanged.
These operations require local headset access and must never run against cloud
storage or publish original assets.

CLI callers use `--run BuildAndInstall REQUEST.json` with `Options.Target: 1` and
PCVR-specific flags `PcvrBuildOnly`, `PcvrPrepareData`, `PcvrGpuTests` and
`PcvrAudioTests`. `BuildOnly`, `Release`, `CopyGameData` and radio flags remain
Quest options. Use the same Settings object with `--run DiagnosePcvr REQUEST.json`
or `--run LaunchPcvr REQUEST.json`; launch optionally accepts `PcvrFrameLimit`
(0 means unlimited). Relevant settings are `VulkanSdk`, `PcvrShaderCompiler`,
`PcvrOpenXrSource`, `PcvrInstallDirectory`, `PcvrDataDirectory` and `PcvrUserDirectory`.
Old JSON requests without Target continue to build Quest.

**Original Xbox assets** also offers Quest/Windows target selection. Conversion
still accepts only the supported locally owned Xbox disc image; **Use verified
prepared output** rechecks the complete collection and skips conversion. Windows
installs it under the prepared data directory's `modelsets/modern`, with verified
staging and retained backups. Quest retains previous collections too. Staging and
backups live outside active asset roots, so they do not alter matched fingerprints.
No asset downloads, save replacement or automatic asset-source switch occurs.
PCVR builds/data preparation also install the same bundled vehicle defaults as Quest.

The vehicle editor shares authored templates across Quest/PCVR and keeps personal
overrides separate. Its bridge resolves manager PCVR paths and configured ADB;
see the [target transfer workflow](../vehicle-editor/README.md#quest-and-pcvr-targets).

## Actions

| Page | Configuration and behavior |
| --- | --- |
| Build and install | Select Quest or Windows PCVR. Quest retains debug/release APK builds, optional data/radio conversion and SDK consent. Windows builds Release with local MSVC/CMake/Ninja/Vulkan, runs checks, optionally prepares local data, and installs a versioned desktop build. |
| Launch PCVR | Validate installed files, prepared data and active runtime, probe Vulkan/OpenXR, then run the game with isolated saves. Quit in-game to return; Cancel stops the process. |
| PCVR diagnostics | Check the active x64 runtime manifest/library and run Vulkan and OpenXR session probes. Does not change runtime selection or require game assets. Each probe has a 60-second timeout. |
| Vehicle editor | Output folder and ZIP packaging. Configures CMake, builds the editor and three native test targets, runs CTest, installs and optionally packages. Includes the manager runtime for its device bridge. |
| Manage music | Browse headset music and file details. Ctrl-click selects an inclusive range; Range end then an ordinary click also selects a range. Shift-click is supported where the terminal forwards it; Windows Terminal consumes it for text selection. Plain click selects one. Drag or Up/Down moves the selected group; Delete queues selected files for removal. Perform applies order/deletion and Cancel discards them. The import browser supports multiple files/folders with Ctrl-click or Space (Alt-click for ranges), plus validation only and radio board export. Imports use ffprobe validation, duplicate hashes and verified atomic commits. |
| Original Xbox assets | User-provided GTA III US 1.0 `.iso`/`.xiso.iso` only; no asset downloads. Verifies retail fingerprints and converts visuals, particles, Neo effect textures/tuning and audio locally using CMake/Ninja/C++ and FFmpeg/ffprobe. Sound banks become PCM with rebuilt loop/offset metadata; speech and radio become lossless FLAC at authored rates. Installs the checksummed `modern` collection with staging/rollback. Select Classic / Original Xbox under Appearance > Asset Source and fully restart. Neo materials and optional rain droplets have live controls on the same page. Reimport older collections to add the new effects. Build only skips headset access. See [asset guide](../../overlay/docs/QUEST_MODELSETS.md). |
| Repair game files | Optional port `gamefiles` folder; otherwise obtains the pinned source. Requires an existing game installation and repairs the allowlisted replacement text/frontend files. |
| Update | Dry run, build only, debug/release and SDK license consent. Verifies clean main, fetches official main, enforces version floor and asset requirements, fast-forwards without reset/stash, rebuilds the updated manager and updates only the APK. |
| Transfer save | Quest → PC or PC → Quest, checkbox multi-selection of slots 1–8 and Autosave, and PC save-folder browser. Validates every selected source and backs up destinations before writing, preserves save bytes and verifies Quest read-back. Native C# on Windows/Linux; requires ADB only. |
| Settings | Repository, workplace, retail folder, Android SDK, JDK, ADB, ffprobe, CMake, Vulkan SDK, PCVR tools/directories and optional auxiliary runtimes; pinned tool download preference. |
| Mods | Curated Upstate Liberty v1.1.2: validate a local `UL.7z` or download the pinned release, install, check status, enable, disable or delete device mod files. Requires installed 7-Zip (`7zz`/`7z`, or select its executable in Settings). |
| Tools and tests | Source authoring, placement transfer, capture analysis, RenderDoc export, archive utilities, HRTF generation, artwork, regression adapters and optional librw packaging tools. Legacy save-transfer modules are hidden; use Transfer save. |

The [curated mod guide](../../overlay/docs/QUEST_MODS.md) documents package handling
and the runtime protocol. Current APKs include a native Upstate engine adapter with
isolated engine limits and saves; headset gameplay/performance acceptance remains
experimental. Older APKs keep the package disabled until a compatible runtime
advertises support. Automatic enablement and re-enabling are gated by that
capability; Windows ASI binaries are excluded from device packages.

Saved configuration lives in `%APPDATA%\LibertyCityVR\settings.json` on Windows
and `~/.liberty-city-vr/settings.json` on Linux. Action options are saved when
performed; SDK license acceptance must be explicit on each run. Shared settings
are saved with **Save**. Writes are atomic. A malformed configuration is reported
without silently replacing it. `LCVR_SETTINGS_DIR` or `--settings DIRECTORY`
selects a separate local configuration directory for testing.

The workplace defaults to `C:\LCVRBuild` or `~/LCVRBuild`, matching the old
tools. Keep it separate from the repository. Download caches, pinned source,
fresh build trees, model products, editor output and auxiliary snapshots are
kept there. Logs are under the configuration directory's `logs` folder.
The manager never persists device identifiers in settings or prints raw ADB
discovery/command output. Multiple eligible headsets require numbered selection;
headless runs require exactly one. Model queries have a deadline. An APK build
can succeed while deployment is skipped for missing ADB or an eligible device.
Signature/install failures stop without uninstalling or clearing application data.

The **Transfer save** page uses `GTA3sfN.b` in the chosen PC folder and the same
Quest slot N. Autosave uses `GTA3sfautosave.b` on PC and `autosave.b` on Quest.
Click or Space toggles a slot checkbox; Ctrl+A selects all nine. Its default PC
folder is `<workplace>/save-transfer/pc`. Existing
destinations are backed up under `<workplace>/save-transfer/backups` before
replacement. Close both games and other save-transfer tools first; the Quest
app is force-stopped and left stopped. Failed/interrupted Quest writes trigger
best-effort restoration with an independent recovery timeout. If the device
disconnects and recovery fails, the raw local backup remains available.
See [save transfer](../../overlay/tools/saves/README.md) for the supported format.

## Dependencies

See [NOTICE.md](NOTICE.md) for Terminal.Gui, every locked transitive NuGet
package and native Oniguruma attribution, and the [root tools inventory](../README.md)
for tool roles, optional authoring packages and external licenses. FFmpeg and
FFprobe are user-installed external processes. Xbox extraction uses the native
XDVDFS reader plus librw/lzokay; extract-xiso and a downloaded HD pack are not needed.


Primary workflows run natively in C# and do not use the legacy script engines.
Build-tool downloads retain the original pinned versions and SHA256 checks:
Git for Windows 2.55.0.3, Temurin 21.0.11+10, Android command-line tools 15859902,
Gradle 8.13, Platform 35, Build Tools 34.0.0, NDK 27.2.12479018 and CMake 3.22.1.
Linux needs Git installed. Automatic portable tool downloads support x64;
other architectures need compatible configured tools. ffprobe is required for
music, and Linux model conversion needs a C++ compiler. The editor needs desktop
C++/OpenGL development tools (Visual Studio/Build Tools with Desktop C++ on Windows; C++, Ninja and X11/
GLFW dependencies on Linux). No game data is downloaded by the manager.

Auxiliary implementations and historical regression adapters are compiled into
`EmbeddedTools.resx`, including their original relative module names. They are
**embedded modules, not C# rewrites**: the selected module still needs Python 3,
PowerShell 7 or Bash, plus any tool-specific packages such as NumPy. Configure
these executable paths in Settings. Some original diagnostic/test adapters are
platform-specific; exposing them does not make their MSVC/System.Drawing or
PS2/Conan dependencies portable.

The host copies only Git-tracked public source into a fresh local workplace
snapshot and materializes the embedded modules there. It never copies untracked
retail data, saves, private build trees, caches or Git metadata. This preserves
relative imports, native test fixtures and authoring outputs while removing all
standalone `.ps1`, `.sh` and `.py` source entrypoints from the checkout. Snapshots
and outputs remain local for inspection. Scripts referenced by Android itself
(the optional ASan wrapper) are generated only in the private assembled build.
Superseded script installers, the Vice City save converter and downloaded
HD-pack builder/tests have been removed from the resource. Build/update, repair,
music, save transfer and local retail ISO preparation use native C# services.
See the [tools audit](../README.md#removed-inherited-workflows).

## Command interface

```text
dotnet run --project tools/manager -c Release -- --help
dotnet run --project tools/manager -c Release -- --page Settings
dotnet run --project tools/manager -c Release -- --list-tools
dotnet run --project tools/manager -c Release -- --tool tools/vehicle-editor/test_device_bridge.py -- -v
dotnet run --project tools/manager -c Release -- --privacy install-hooks
```

Module names are resource identifiers; those paths need not exist on disk.
The old modules under `overlay/tools/saves/` have been removed. Use `--page TransferSave`,
`--page QuestToPc`, `--page PcToQuest` or the native `--run TransferSave` action.
Use **Tools and tests** to see parameters before running. Quote paths with spaces.
Device selection for auxiliary PowerShell tools with a Serial parameter is
injected locally by the manager; do not supply or share a serial argument.
Auxiliary tool outputs use the same sanitizing logger. Use the native manager
checks below to validate the active primary workflows.

For unattended native actions, pass `--run ACTION REQUEST.json`. The JSON has
`Settings` (shared `ManagerSettings` properties) and `Options` (`ActionOptions`).
For example, build only with an already installed SDK:

```json
{
  "Settings": {
    "Repository": "D:\\Projects\\GTAVR\\liberty-city-vr",
    "Workplace": "C:\\LCVRBuild"
  },
  "Options": { "BuildOnly": true }
}
```

## Build and verification

```text
dotnet build tools/manager -c Release
dotnet run --project tools/manager/tests -c Release
dotnet publish tools/manager -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -o out/manager-windows
dotnet publish tools/manager -c Release -r linux-x64 --self-contained true -p:PublishSingleFile=true -o out/manager-linux
```

Published platform binaries do not need an installed .NET runtime. Building the
Android application and updating the manager from source still need their listed
toolchains. Keep all generated binaries outside Git. No manager distribution
should contain an APK, original game data, saves or model packs.
