# Liberty City VR Manager

The Windows/Linux project manager is a .NET 10 console application using
Terminal.Gui **2.5.0**. Its GTA III inspired palette uses midnight blue,
cream, menu cyan and HUD amber. Use a terminal of at least 80 × 24; 110 × 36
shows more configuration without scrolling.

From the repository root:

```text
dotnet run --project tools/manager -c Release
```

Windows users can double-click `MANAGE.bat`. The existing six BAT shortcuts
open the corresponding page. They need the complete checkout and .NET 10 SDK,
or a published manager in `out/manager-windows`. They no longer download a kit
or launch separate PowerShell/Python scripts.

Choose an action on the left, configure it on the right, then **Perform**.
**Cancel** discards unperformed edits and returns focus to navigation. During
an action it cancels the tool process, waits for cleanup, then returns focus.
Only one action runs at a time. Tab, Shift+Tab, arrows, Enter and the mouse
work throughout the interface; Ctrl+Q quits after cancelling active work.
Forms scroll and every path field has a terminal file/directory browser.

## Actions

| Page | Configuration and behavior |
| --- | --- |
| Build and install | Debug/release, build only, optional retail data copy, explicit SDK license consent. Downloads verified tools, checks the pinned re3 checkout, applies all patches, assembles in a fresh directory, builds the signed APK and updates an eligible Quest 3. |
| Vehicle editor | Output folder and ZIP packaging. Configures CMake, builds the editor and three native test targets, runs CTest, installs and optionally packages. Includes the manager runtime for its device bridge. |
| Manage music | Browse headset music and file details. Ctrl-click selects an inclusive range; Range end then an ordinary click also selects a range. Shift-click is supported where the terminal forwards it; Windows Terminal consumes it for text selection. Plain click selects one. Drag or Up/Down moves the selected group; Delete queues selected files for removal. Perform applies order/deletion and Cancel discards them. The import browser supports multiple files/folders with Ctrl-click or Space (Alt-click for ranges), plus validation only and radio board export. Imports use ffprobe validation, duplicate hashes and verified atomic commits. |
| Modern models | Pinned ZIP or extracted source, optional explicit ModDB download, generated `modern` output folder, build only. Verifies source hashes, merges IMG archives, repairs wheel texture references, compresses textures, writes and validates manifests, stages and verifies device transfers before activation with rollback. Existing verified overlays can be reused. |
| Repair game files | Optional port `gamefiles` folder; otherwise obtains the pinned source. Requires an existing game installation and repairs the allowlisted replacement text/frontend files. |
| Update | Dry run, build only, debug/release and SDK license consent. Verifies clean master, fetches official master, enforces version floor and asset requirements, fast-forwards without reset/stash, rebuilds the updated manager and updates only the APK. |
| Transfer save | Quest → PC or PC → Quest, manual slot 1–8, and PC save-folder browser. Validates the compatible GTA III container/checksum, backs up the destination, preserves save bytes and verifies Quest read-back. Native C# on Windows/Linux; requires ADB only. |
| Settings | Repository, workplace, retail folder, Android SDK, JDK, ADB, ffprobe, CMake and optional auxiliary runtimes; pinned tool download preference. |
| Tools and tests | Source authoring, placement transfer, capture analysis, RenderDoc export, archive utilities, HRTF generation, artwork, regression adapters and optional librw packaging tools. Legacy save-transfer modules are hidden; use Transfer save. |

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
Quest slot N. Its default PC folder is `<workplace>/save-transfer/pc`. Existing
destinations are backed up under `<workplace>/save-transfer/backups` before
replacement. Close both games and other save-transfer tools first; the Quest
app is force-stopped and left stopped. Failed/interrupted Quest writes trigger
best-effort restoration with an independent recovery timeout. If the device
disconnects and recovery fails, the raw local backup remains available.
See [save transfer](../../overlay/tools/saves/README.md) for the supported format.

## Dependencies

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
Primary-workflow compatibility sources in the resource support historical test
fixtures; they are not selectable as alternative installers. The supported
installer/update behavior is the native C# implementation.

## Command interface

```text
dotnet run --project tools/manager -c Release -- --help
dotnet run --project tools/manager -c Release -- --page Settings
dotnet run --project tools/manager -c Release -- --list-tools
dotnet run --project tools/manager -c Release -- --tool tools/vehicle-editor/test_device_bridge.py -- -v
dotnet run --project tools/manager -c Release -- --privacy install-hooks
```

Module names are resource identifiers; those paths need not exist on disk.
The old modules under `overlay/tools/saves/` cannot be launched with `--tool`;
they remain embedded only as historical sources. Use `--page TransferSave`,
`--page QuestToPc`, `--page PcToQuest` or the native `--run TransferSave` action.
Use **Tools and tests** to see parameters before running. Quote paths with spaces.
Device selection for auxiliary PowerShell tools with a Serial parameter is
injected locally by the manager; do not supply or share a serial argument.
Auxiliary tool outputs use the same sanitizing logger. Historical installer
fixture tests validate compatibility sources; use the native manager checks
below to validate the active primary workflows.

For unattended native actions, pass `--run ACTION REQUEST.json`. The JSON has
`Settings` (shared `ManagerSettings` properties) and `Options` (`ActionOptions`).
For example, build only with an already installed SDK:

```json
{
  "Settings": {
    "Repository": "D:\\Projects\\GTAVR\\liberty-city-vr-quest",
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
