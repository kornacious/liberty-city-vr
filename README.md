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

Android positional sound effects use the full listener-relative 3D position
for distance attenuation. Gain stays full inside the supplied minimum range,
falls quadratically to silence at the maximum range, and updates as sources
move. The existing effects/fade controls and 2D audio paths remain in effect.

Quest swinging-door motion and aircraft destruction sequences follow elapsed
game time with their original 30 FPS reference behavior. Pickup partitions,
population/world cleanup, garage checks, siren yielding and distant train/plane
updates use a separate 30 Hz gameplay clock that respects pause and slow motion.
Crossed schedule phases are processed when a frame spans multiple ticks.

Current source-kit version: **v0.5.6**.

This release adds articulated ragdolls with car and wall contact, optional
vehicle body deformation, clearer menus and fixes for tracked weapon aiming,
melee, vehicle entry and seat centering. Existing graphics and calibration
preferences are preserved. See [the changelog](CHANGELOG.md) for the
0.5.6 feature summary and [performance notes](PERFORMANCE.md) for the retained rendering
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

Mission autosaves use a separate `autosave.b` file and **Load Game → Load
Autosave** entry. All eight manual slots remain available. Autosave writes a
temporary file, flushes and validates it, keeps the previous valid autosave in
`autosave.b.bak`, then atomically replaces the primary file. Loading autosave
automatically tries the backup if the primary is missing or corrupt. These
files use the existing save directory/prefix. Older releases wrote autosaves
into manual slot 1; that file remains in slot 1 and is not moved or overwritten
by the new autosave system. A new autosave is created after the next eligible
mission completion. Storage providers without directory flush support retain
atomic replacement, but cannot guarantee rename durability after power loss.

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
that checkout: they copy it to a new directory, apply all thirteen patches in
`patches/` in filename order, and add the port overlay and bundled librw fork.
No private repository is required or referenced.

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
existing headset. It replaces selected GXT and frontend/shared TXD files,
including files under `models/`; saves, settings, model archives and optional
model overlays are left alone.

The source-only maintainer checks are in [RELEASING.md](RELEASING.md).

## Current Quest build

- Native arm64 Android application using OpenXR and Vulkan multiview.
- Room-scale movement, physical weapons, two-hand support, holsters, scoped
  aiming and physical steering for cars and boats.
- Remappable face buttons, grips and stick clicks; a swapped-hands layout;
  per-weapon and per-seat calibration.
- In-headset controls, calibration, categorized cheats, traffic, ragdoll,
  graphics and model-set menus.
- Optional articulated ragdolls respond to bullets, explosions, cars and nearby world geometry.
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
- Classic/Modern/Xbox asset categories. The Quest baseline keeps vehicles,
  pedestrians and vegetation Classic and requests Modern world/weapons when
  available. Optional asset overlays are built separately and are not distributed here.

Defaults retain 100% render scale and sustained CPU/GPU performance hints.
Ragdolls and vehicle deformation default to OFF; saved preferences take precedence.
Normal frontend and save loading are used on startup. Experimental temporal AA,
SGSR and runtime-only foveation remain disabled.

Runtime/data layout is documented in
[QUEST_PORT.md](overlay/docs/QUEST_PORT.md). See
[QUEST_GAMEPLAY.md](overlay/docs/QUEST_GAMEPLAY.md) for gameplay changes,
[QUEST_RAGDOLL.md](overlay/docs/QUEST_RAGDOLL.md) for ragdoll behavior and limits,
[QUEST_VEHICLE_DEFORMATION.md](overlay/docs/QUEST_VEHICLE_DEFORMATION.md) for
body deformation, and [QUEST_MODELSETS.md](overlay/docs/QUEST_MODELSETS.md)
for optional model mixing.
The [asset guide](overlay/docs/QUEST_MODELSETS.md) also describes the current
Xbox-first installer and limitations of the Modern preparation branches.

## Troubleshooting and Game Data

The final Quest path must be exactly
`/sdcard/Android/data/com.libertyvr.quest/files/gamedata`.
Fully restart the game after copying assets.

For a fresh installation, install the APK first and bootstrap its app-owned
storage through the save provider before copying data. Do not create the
`files` root with ADB before this bootstrap. For example, in PowerShell:

```powershell
$adb = "C:\LCVRBuild\.android-sdk\platform-tools\adb.exe"

& $adb devices
& $adb shell am force-stop com.libertyvr.quest
& $adb shell content query --uri content://com.libertyvr.quest.saves/slot/1 --projection _display_name:_size
$gameData = "/sdcard/Android/data/com.libertyvr.quest/files/gamedata"
foreach ($folder in @("Audio", "TEXT", "anim", "data", "models", "movies", "mp3", "skins", "txd")) {
    & $adb push "C:\Games\GTA3\$folder" $gameData
}
```

Then copy the assembled tree's replacement text/frontend files and VR hand
assets as described in [BUILDING.md](BUILDING.md#6-supply-your-own-game-data).
The guided installer performs these steps automatically.

## Credits and Acknowledgments

- **GeT_RiCh** — Original author (based on his Discord account) of the [vice-city-vr-quest](https://github.com/dubrovskiy-yevhen-stakelogic/vice-city-vr-quest) VR port for the major implementation made for GTA Vice City using the reVC engine as a contribution for VR support.
- **whachuvr** — For the full body IK implementation fork on top of GeT_RiCh's repository ([whachuvr/vice-city-vr-quest](https://github.com/whachuvr/vice-city-vr-quest)).
- **re3 and reVC teams** — Huge thanks for their amazing effort and groundbreaking reverse-engineering work.
- **Gemini 4 Argon and Gemini 3.8 Flash** — Huge thanks for porting GeT_RiCh and whachuvr's work from reVC / Vice City to re3 / GTA 3.
- Their collective work offered a huge contribution for a solid starting point for the GTA III VR for standalone Quest porting efforts.
- **OpenAI Codex** — Assisted with ongoing Liberty City VR Quest development,
  including implementation, debugging, regression testing and documentation.

## Planned development with Codex

Codex is planned to take over the ongoing coding-assistance role for Liberty
City VR Quest, with priorities and changes directed and reviewed by the project
maintainer. Planned work includes:

- Fixing reported gameplay, audio, save-system and VR interaction issues.
- Refining tracked controls, vehicle interactions and cutscene comfort.
- Improving Quest rendering performance and stability, including Vulkan resource
  handling and visual correctness.
- Maintaining regression coverage, source assembly, build/update tooling and
  contributor documentation as features evolve.

These are future work areas, not completed features or a release schedule.
Implementation and validation will happen through maintainer-directed development
sessions; headset behavior and performance still require local device testing.

## Distribution boundary

Do not upload assembled source trees, APKs, original game files, saves, built
model overlays or signing keys to this repository. Publish only this
build kit. Each player must obtain re3 and the original game separately and
perform the documented build locally.

## No affiliation

This is an unofficial fan project. It is not affiliated with or endorsed by
the publishers or developers of GTA III, Meta, Khronos, Qualcomm or
NVIDIA. All trademarks belong to their owners.
