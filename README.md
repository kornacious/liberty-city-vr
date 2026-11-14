# Liberty City VR — source build kit

> **NO APK IS PROVIDED. THIS IS A SOURCE-BUILD RELEASE.**
>
> Open the cross-platform **Liberty City VR Manager** with `MANAGE.bat` on
> Windows or `./MANAGE.sh` on Linux. Both also support
> `dotnet run --project tools/manager -c Release`.
> Select **Build and install**, configure the action, then choose **Perform**.
> The .NET 10 / Terminal.Gui 2.5.0 application also includes the vehicle editor
> builder, music importer, Original Xbox importer, game-file repair, updater,
> shared settings, file browsing, and auxiliary tools/tests.
>
> Downloads of portable build tools retain pinned SHA256 checks. SDK package
> installation requires explicit license consent. Game data comes only from
> your legally owned local GTA III installation. Existing app data and saves
> are preserved; an incompatible signing key stops installation.
>
> See [the manager guide](tools/manager/README.md) for configuration,
> prerequisites, embedded auxiliary runtimes, logs and standalone publishing.

> [!TIP]
> **Join the Flat2VR Discord!** Development updates, player feedback, testing,
> and discussion of the mod take place in the Flat2VR community.

Liberty City VR brings GTA III to VR using re3, OpenXR and the bundled librw
Vulkan renderer. The project covers **standalone Meta Quest 3** and experimental
**Windows/Linux PCVR targets** in one source repository:
[github.com/kornacious/liberty-city-vr](https://github.com/kornacious/liberty-city-vr).

| Target | Status in 0.8.5 |
| --- | --- |
| Quest 3 / Android ARM64 | Implemented; source-build release candidate. |
| Windows PCVR / x64 / OpenXR + Vulkan | Experimental playable build; manager builds, installs locally, launches and diagnoses the PC runtime. |
| Linux PCVR / x64 / OpenXR + Vulkan | Validated on native CachyOS/WiVRn: build, launch, PC/Xbox audio, first mission and saving. See [validation scope](PLATFORMS.md). |

Manager/editor host support and PCVR game support are validated separately.
See [platform scope and roadmap](PLATFORMS.md)
for the shared architecture and target boundaries, and [BENCHMARKS](BENCHMARKS.md)
for the captured Quest workload profile.
Developers can build the [shared desktop game and renderer diagnostic](overlay/desktop/README.md).
The game uses separate local data/save directories; the diagnostic requires no game data.
In the manager, select **Windows PCVR** or **Linux PCVR** on the matching host under **Build and install**, then use
**Launch PCVR** or **PCVR diagnostics**. See the [PCVR manager workflow](tools/manager/README.md#windows-pcvr).
The game also opens a [desktop eye preview](overlay/windows/README.md#desktop-preview-and-input)
with ImGui/F1–F12 display controls and keyboard/mouse/PC gamepad input.
Captured Quest workload profiles and local matched replay comparisons are available;
see [benchmark setup and measurement limits](BENCHMARKS.md).
Verified Original Xbox collections are supported on both targets, including matched
Xbox replay comparisons. The [vehicle editor](tools/vehicle-editor/README.md#quest-and-pcvr-targets)
authors wheels, vehicle HUD/dashboard panels, radio/music and speedometer placement
using shared templates and separate Quest/PCVR personal overrides, with transfer
previews, backups and live reload acknowledgement.

Android positional sound effects use the full listener-relative 3D position
for distance attenuation. Gain stays full inside the supplied minimum range,
falls quadratically to silence at the maximum range, and updates as sources
move. The existing effects/fade controls and 2D audio paths remain in effect.

Quest swinging-door motion and aircraft destruction sequences follow elapsed
game time with their original 30 FPS reference behavior. Pickup partitions,
population/world cleanup, garage checks, siren yielding and distant train/plane
updates use a separate 30 Hz gameplay clock that respects pause and slow motion.
Crossed schedule phases are processed when a frame spans multiple ticks.

Current source-kit version: **v0.8.5 — release candidate**.

**The Quest build is ready for public testing and playing, with finishable main missions.** Please
report mission progression issues, VR comfort problems and compatibility bugs
in the Flat2VR community. See [the changelog](CHANGELOG.md#085--release-candidate)
for changes since 0.8.0; earlier release changes remain documented below it.

The port started from **Vice City VR v0.5.6**. **v0.6.0** marks the port of that
foundation from GTA Vice City to GTA III. **v0.7.0** covers the features and fixes
added to Liberty City VR after the 0.6.0 port baseline, including full-body IK
and weapon calibration improvements, physical scopes, custom music and the
vehicle radio board, safer autosaves, and rendering, audio and gameplay fixes.
**v0.8.0** introduced the cross-platform manager, bidirectional save transfer,
headset playlist management, tracked spatial audio, the vehicle placement editor
and calibrated defaults, Classic/Original Xbox models, dual wielding, immersive
speedometers, train/camera/mission fixes, and expanded graphics/profiling tools.
**v0.8.5** adds local Original Xbox ISO import, tracked Xbox body/camera improvements,
hands-up surrender, shared native/VR settings, curated Upstate Liberty management
and engine integration, Neo materials/particles/audio, rendering debug views and
sniper overlay fixes. It also establishes the planned Windows PCVR scope and
updates the official repository URL. PCVR remains future work.
See the [gameplay guide](overlay/docs/QUEST_GAMEPLAY.md),
[spatial audio notes](overlay/docs/SPATIAL_AUDIO.md),
[model guide](overlay/docs/QUEST_MODELSETS.md),
[vehicle editor guide](tools/vehicle-editor/README.md) and
[capture guide](overlay/docs/PERFORMANCE_CAPTURE.md) for details.
Existing graphics and calibration preferences are preserved. See
[the changelog](CHANGELOG.md) for the release history and
[performance notes](PERFORMANCE.md) for the inherited rendering optimizations.

It intentionally contains **no APK, complete re3 source tree, original game
files, saves, logs or original Xbox assets**. Every user builds their
own APK and supplies data from a legally owned PC copy of GTA III.

## Updating an existing installation

Choose **Update** in the manager. It fetches official main, requires a clean
fast-forward, checks the version floor, reuses cached tools, rebuilds the updated
manager and replaces only the APK. Local changes, divergent history, asset-update
requirements and incompatible signatures stop the update. Use **Dry run** for
local checks, **Build only** to skip deployment, and **Release** with your private
`LIBERTYVR_RELEASE_SIGNING_PROPERTIES` when updating a release-signed application.
The workplace in **Settings** selects the tool/dependency cache.

Mission autosaves retain their separate `autosave.b` and `autosave.b.bak` files;
manual slots, calibration, retail data and optional model overlays stay in place.
## What is included

| Path | Contents |
|---|---|
| `overlay/` | Current Android/OpenXR, VR integration and tooling files; common/platform separation planned |
| `patches/` | Changes applied to a user-supplied re3 checkout |
| `librw/` | MIT-licensed librw fork with the Quest Vulkan backend |
| `tools/` | Cross-platform .NET manager, embedded auxiliary tools and native fixtures |

Modified upstream re3 files are distributed as patches, not as a second copy
of the re3 repository. The only bundled runtime art is the credited MIT-licensed VR
hand asset under `overlay/gamefiles/models/vrhands/`.

## Custom music and radio board

Choose **Manage music** in the manager to browse headset tracks, inspect metadata,
import your own audio and drag tracks into playlist order. **Perform** saves the
order; **Cancel** discards pending changes. The adjustable
vehicle-mounted radio board provides station, volume and custom-track controls.
See [Custom radio](overlay/docs/CUSTOM_RADIO.md) for formats, limits, calibration,
placement sharing, and the ADB/ffprobe prerequisites.

## Desktop vehicle placement editor

Choose **Vehicle editor** in the manager to build, test and package the standalone non-VR editor.
It reads your local GTA III models, edits virtual wheel and dashboard-panel
placements per vehicle/modelset, and exports or transfers template and personal
packs to a Quest 3. See the [editor guide](tools/vehicle-editor/README.md) for
dependencies, import/deployment, and preview limitations.

## Required external source

The exact patch base comes from the public upstream re3 project:

<https://github.com/mrxenginner/re3>

The tested public `master` commit is
`b050e1cc63b1869e6b514ab2f1a00ef4969e0c84`. The assembly action never modifies
that checkout: they copy it to a new directory, apply all patches in
`patches/` in filename order, and add the port overlay and bundled librw fork.
No private repository is required or referenced.

Complete prerequisites and copy-paste commands are in
[BUILDING.md](BUILDING.md).

Use the manager's **Build and install** page for the guided build workflow on Windows and Linux.

If the OPTIONS menus show entries like `FET_GFX missing` or `FED_AAS missing`
instead of labels, the installation predates the wizard copying the port's
replacement text and frontend files.
The manager's **Repair game files** page installs those onto an
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
- Hands-up surrender: while wanted and on foot, hold both tracked hands at
  head height or higher, within 65 cm of each other, for three seconds. A law
  enforcement officer attacking Claude within 15 metres and with clear sight
  triggers the normal Busted sequence. Lowering either hand, losing tracking,
  opening a menu or leaving combat resets the hold.
- The Dodo uses powered fixed-wing flight on Quest. Hold the right trigger to
  build runway speed, then gently pull the left stick back to take off. Use
  left/right to bank, forward/back to pitch, and the left trigger to reduce
  power for landing. Car motion/physical-wheel modes do not affect aircraft.
- In-headset controls, calibration, categorized cheats, traffic, ragdoll,
  graphics and model-set menus.
- Native Main/Pause menu Options and the in-game VR overlay share categorized settings
  implementation, including audio volume, subtitles, language and player skins.
  See [Quest settings](overlay/docs/QUEST_SETTINGS.md).
- Player cheats include Flight and Noclip. In Flight, move with the left stick,
  hold A to rise, B to descend, and the right index trigger (R2) for a speed boost.
  Noclip alone retains roads and terrain; combining it with Flight bypasses
  all collision. Leaving the map returns the player to the Town Hall forecourt
  on Staunton Island and turns both modes off. Traversal is suspended in
  vehicles, cutscenes and scripted control sequences.
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
- Classic or Original Xbox asset profiles. Original Xbox loads its complete world,
  vegetation, vehicle, pedestrian and weapon assets together. Changes require a
  full app restart.

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
for Classic/Original Xbox asset selection.
The [asset guide](overlay/docs/QUEST_MODELSETS.md) also describes the current
local preparation from a user-provided original Xbox US 1.0 ISO,
including conversion requirements and renderer/cutscene compatibility limits.

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

## Tools and third-party dependencies

The [tools inventory](tools/README.md) covers compilation, signing, publishing,
ADB deployment, FFmpeg/FFprobe audio conversion, native Xbox ISO extraction,
editor dependencies and optional authoring/diagnostic utilities. See
[NOTICE.md](NOTICE.md) and the [manager notices](tools/manager/NOTICE.md) for
licenses and locked transitive dependencies.

Liberty City VR builds **GTA III/re3**. Vice City VR and reVC references in the
credits describe the original VR foundation. The old Vice City save converter,
script installers, HD-pack downloader and incompatible test fixtures have been
removed; active build compatibility aliases remain where required.

## Credits and Acknowledgments

- **GeT_RiCh** — Original author (based on his Discord account) of the [vice-city-vr-quest](https://github.com/dubrovskiy-yevhen-stakelogic/vice-city-vr-quest) VR port for the major implementation made for GTA Vice City using the reVC engine as a contribution for VR support.
- **whachuvr** — For the full body IK implementation fork on top of GeT_RiCh's repository ([whachuvr/vice-city-vr-quest](https://github.com/whachuvr/vice-city-vr-quest)).
- **re3 and reVC teams** — Huge thanks for their amazing effort and groundbreaking reverse-engineering work.
- **Gemini 4 Argon and Gemini 3.8 Flash** — Huge thanks for porting GeT_RiCh and whachuvr's work from reVC / Vice City to re3 / GTA 3.
- Their collective work offered a huge contribution for a solid starting point for the GTA III VR for standalone Quest porting efforts.
- **OpenAI Codex** — Assisted with ongoing Liberty City VR development,
  including implementation, debugging, regression testing and documentation.

## Planned development with Codex

Codex is planned to take over the ongoing coding-assistance role for Liberty
City VR across its Quest and planned PCVR targets, with priorities and changes
directed and reviewed by the project maintainer. Planned work includes:

- Fixing reported gameplay, audio, save-system and VR interaction issues.
- Refining tracked controls, vehicle interactions and cutscene comfort.
- Improving Quest rendering performance and stability, including Vulkan resource
  handling and visual correctness.
- Expanding runtime/controller coverage and performance validation of the
  experimental Windows OpenXR/Vulkan game, manager integration and captured Quest
  workload profile; see [platform scope](PLATFORMS.md).
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
