# Liberty City VR for Meta Quest — source build kit

> **NO APK IS PROVIDED. THIS IS A SOURCE-BUILD RELEASE.**
>
> On Windows, connect the Quest and double-click **`BUILD_AND_INSTALL.bat`**.
> On Linux, connect the Quest and run **`./BUILD_AND_INSTALL.sh`**.
> The wizard obtains portable Git, JDK 21, the Android command-line SDK,
> the exact re3 source, assembles the port, builds your personal APK, installs
> it without clearing existing data, and can copy only the required folders
> from your legally owned GTA III installation.
> Missing build tools and SDK packages are downloaded from their official
> sources and verified against pinned SHA256 hashes automatically. Google SDK
> licenses are still shown for the user to accept.
> If an older installation has an incompatible signing key, the wizard warns
> that replacing it can erase saves/game data and uninstalls it only after an
> explicit `Y` confirmation. It validates the supplied GTA III PC folder
> before removal so the required game data can be restored afterward.
> You may also download only **`BUILD_AND_INSTALL.bat`** (Windows) or
> **`BUILD_AND_INSTALL.sh`** (Linux). If the rest of the
> source kit is not beside it, the wrapper downloads and extracts the complete
> public repository automatically.
>
> Run the wrapper (`.bat` on Windows, `.sh` on Linux), not the internal
> wizard file. The window remains open on
> errors and writes `%TEMP%\LibertyCityVR-Build-And-Install.log` (Windows) or
> `$TMPDIR/LibertyCityVR-Build-And-Install.log` (Linux); send that log
> when asking for help.


> [!TIP]
> **Join the Flat2VR Discord!** Development updates, player feedback, testing,
> and discussion of the mod take place in the Flat2VR community.

This repository contains the original Quest/OpenXR port layer, Vulkan backend
changes, build scripts and re3 patch files needed to build Liberty City VR for a
Meta Quest headset.

Current source-kit version: **v0.5.6**.

This release adds articulated ragdolls with car and wall contact, optional
vehicle body deformation, clearer menus and fixes for tracked weapon aiming,
melee, vehicle entry and seat centering. Existing graphics and calibration
preferences are preserved. See [the changelog](CHANGELOG.md) for the complete
0.5.6 changes and [performance notes](PERFORMANCE.md) for the retained rendering
optimizations.

It intentionally contains **no APK, complete re3 source tree, original game
files, saves, logs or third-party Modern model packs**. Every user builds their
own APK and supplies data from a legally owned PC copy of GTA III.

## Updating an existing installation

After the initial setup, keep this folder and run **`UPDATE.bat`** on Windows
or **`./UPDATE.sh`** on Linux. There is no need to download the source ZIP again
for each release. Connect and authorize the Quest before installing the update.

The updater fetches newer source and reuses the existing tool and dependency
cache. From a ZIP installation, its first run creates a managed Git checkout
under the build work directory; later runs update that checkout. Each APK is
assembled in a new directory, preserving earlier builds and local files.

Updates replace only the APK. Saves, settings, game data and optional model
overlays stay in place. An incompatible signing key stops the update instead
of uninstalling the game. If a future release requires new game assets, the
updater asks you to use the ordinary installer for that separate step.

The default builds a personal debug APK using the same Android debug key as
the original wizard. To update an app signed with your release key, use
`-Release` / `--release` and set `LIBERTYVR_RELEASE_SIGNING_PROPERTIES` to your
private signing configuration. It must use the installed app's signing key;
no signing keys are included in this source kit.

Use the same work directory as the original installation to reuse its cache:

```powershell
.\UPDATE.bat -WorkDir C:\LCVRBuild
```

```sh
./UPDATE.sh --work-dir "$HOME/LCVRBuild"
```

The Git update is fast-forward only. Local changes or a diverged checkout stop
it; it does not reset, stash or clean your files. `-BuildOnly` / `--build-only`
builds without installing, and `-DryRun` / `--dry-run` checks the update plan
without fetching, downloading, building or accessing the headset.

## What is included

| Path | Contents |
|---|---|
| `overlay/` | New Android, OpenXR, Quest VR and tooling files |
| `patches/` | Changes applied to a user-supplied re3 checkout |
| `librw/` | MIT-licensed librw fork with the Quest Vulkan backend |
| `tools/` | Windows and Unix source-assembly scripts |

Modified upstream re3 files are distributed as patches, not as a second copy
of the re3 repository. The only bundled runtime art is the credited MIT-licensed VR
hand asset under `overlay/gamefiles/models/vrhands/`.

## Required external source

The exact patch base comes from the public upstream re3 project:

<https://github.com/mrxenginner/re3>

The tested public `master` commit is
`b050e1cc63b1869e6b514ab2f1a00ef4969e0c84`. The assembly scripts never modify
that checkout: they copy it to a new directory, apply the 4 consolidated
clean patches, and add Quest-only files. No private repository is required or referenced.

Complete prerequisites and copy-paste commands are in
[BUILDING.md](BUILDING.md).

For most Windows users, the recommended route is the guided
[`BUILD_AND_INSTALL.bat`](BUILD_AND_INSTALL.bat) wizard (Linux:
[`BUILD_AND_INSTALL.sh`](BUILD_AND_INSTALL.sh)) instead of entering
those commands manually.

If the OPTIONS menus show entries like `FET_GFX missing` or `FED_AAS missing`
instead of labels, the installation predates the wizard copying the port's
replacement text and frontend files.
[`REPAIR_GAME_FILES.bat`](REPAIR_GAME_FILES.bat) (Windows) or
[`REPAIR_GAME_FILES.sh`](REPAIR_GAME_FILES.sh) (Linux) installs just those onto an
existing headset; saves, settings and models are left alone.

The source-only maintainer checks are in [RELEASING.md](RELEASING.md).

## Current Quest build

- Native arm64 Android application using OpenXR and Vulkan multiview.
- Room-scale movement, physical weapons, two-hand support, holsters, scoped
  aiming and physical steering for cars and boats.
- Remappable face buttons, grips and stick clicks; a swapped-hands layout;
  per-weapon and per-seat calibration.
- In-headset controls, calibration, categorized cheats, traffic, ragdoll,
  graphics and model-set menus.
- Articulated ragdolls respond to bullets, cars and nearby world geometry.
  Body weight, car braking, grip and bullet response can be tuned in the menu.
- Optional vehicle deformation affects opaque body panels, bumpers and grilles.
  It is a bounded visual effect; severe repeated impacts can open component seams.
- Stereo cutscenes with selectable staged cameras, with CINEMA also available.
- Generated texture mipmaps without modifying the player's TXD files;
  OFF/2X/4X multisampling and spatial antialiasing.
- Optional dynamic lights, vehicle reflections, modern water and PS2-style
  two-pass alpha. These increase GPU work and remain off by default.
- Stereo-aware and authored building culling, with an OFF fallback. Authored
  culling is experimental; select STEREO SAFE or OFF if geometry disappears.
- Physics Director V2 with an ORIGINAL/OFF fallback; separate pedestrian and
  vehicle density controls from OFF to 300%. OFF prevents new spawns rather
  than deleting existing traffic.
- Classic/Modern asset categories. Classic vehicles remain the default;
  generated Modern overlays are optional and are not distributed here.

Defaults retain 100% render scale and sustained CPU/GPU performance hints.
Normal frontend and save loading are used on startup. Experimental temporal AA,
SGSR and runtime-only foveation remain disabled.

Runtime/data layout is documented in
[QUEST_PORT.md](overlay/docs/QUEST_PORT.md). See
[QUEST_GAMEPLAY.md](overlay/docs/QUEST_GAMEPLAY.md) for gameplay changes,
[QUEST_RAGDOLL.md](overlay/docs/QUEST_RAGDOLL.md) for ragdoll behavior and limits,
[QUEST_VEHICLE_DEFORMATION.md](overlay/docs/QUEST_VEHICLE_DEFORMATION.md) for
body deformation, and [QUEST_MODELSETS.md](overlay/docs/QUEST_MODELSETS.md)
for optional model mixing.

## Troubleshooting and Game Data

The final Quest path must be exactly
`/sdcard/Android/data/com.libertyvr.quest/files/gamedata`.
Fully restart the game after copying assets.

To manually inspect or transfer files via ADB:

```powershell
$adb = "C:\LCVRBuild\.android-sdk\platform-tools\adb.exe"

& $adb devices
& $adb shell am force-stop com.libertyvr.quest
& $adb shell mkdir -p /sdcard/Android/data/com.libertyvr.quest/files/gamedata
& $adb push "C:\Games\GTA3\*" /sdcard/Android/data/com.libertyvr.quest/files/gamedata/
```

## Distribution boundary

Do not upload assembled source trees, APKs, original game files, saves, built
model overlays or signing keys to this repository. Publish only this
build kit. Each player must obtain re3 and the original game separately and
perform the documented build locally.

## No affiliation

This is an unofficial fan project. It is not affiliated with or endorsed by
the publishers or developers of GTA III, Meta, Khronos, Qualcomm or
NVIDIA. All trademarks belong to their owners.
