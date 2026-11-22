# Liberty City VR — source build kit

> **0.9.5 Release Candidate is a source-only release for Quest 3 and Windows PCVR. Linux PCVR is experimental.**
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
> your local GTA III PC installation or supported Original Xbox US 1.0 disc image. Existing app data and saves
> are preserved; an incompatible Quest signing key stops APK installation.
>
> See [the manager guide](tools/manager/README.md) for configuration,
> prerequisites, embedded auxiliary runtimes, logs and standalone publishing.

> [!TIP]
> **[Join the Flat2VR Discord!][flat2vr-discord]** Development updates, player
> feedback, testing, and discussion of the mod take place there.

[flat2vr-discord]: https://discord.com/channels/747967102895390741/1574137259706548284

Liberty City VR brings GTA III to VR using re3, OpenXR and the bundled librw
Vulkan renderer. The project covers **standalone Meta Quest 3**, **Windows PCVR**
and **experimental Linux PCVR** in one source repository:
[github.com/kornacious/liberty-city-vr](https://github.com/kornacious/liberty-city-vr).

| Target | Status in 0.9.5 Release Candidate |
| --- | --- |
| Quest 3 / Android ARM64 | Release candidate with finishable main missions. |
| Windows PCVR / x64 / OpenXR + Vulkan | Release candidate. The maintainer completed the main story, all payphone missions and the off-road missions with Original Xbox assets and Upstate Liberty enabled after the progression fixes. |
| Linux PCVR / x64 / OpenXR + Vulkan | Experimental. Earlier native CachyOS/WiVRn testing covered build, launch, PC/Xbox audio, first mission and saving. A later Linux playtest confirmed the crowded-scene audio fix. The mission fixes await a native retest. See [validation scope](PLATFORMS.md). |

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
All three targets can use a complete PC or Original Xbox base. Install either or both, then
select **Appearance > Asset Source** and restart to switch. The Xbox base supplies its
own scripts, collision, animations, models, textures and audio. The [vehicle editor](tools/vehicle-editor/README.md#quest-and-pcvr-targets)
authors wheels, vehicle HUD/dashboard panels, radio/music and speedometer placement
using shared templates and separate Quest/PCVR personal overrides, with transfer
previews, backups and live reload acknowledgement.

Android positional sound effects use the full listener-relative 3D position
for distance attenuation. Gain stays full inside the supplied minimum range,
falls quadratically to silence at the maximum range, and updates as sources
move. The existing effects/fade controls and 2D audio paths remain in effect.

Swinging-door motion and aircraft destruction sequences follow elapsed
game time with their original 30 FPS reference behavior. Pickup partitions,
population/world cleanup, garage checks, siren yielding and distant train/plane
updates use a separate 30 Hz gameplay clock that respects pause and slow motion.
Crossed schedule phases are processed when a frame spans multiple ticks.

**New in 0.9.5 RC:** mission selection and progression fixes, an optional immersive
fall view, and a desktop renderer debug viewer. PCVR now keeps saves in the
persistent user directory. Reimport existing Xbox assets to get the corrected
head texture coordinates for Maria, Curly Bob and Catalina.

Please report mission progression issues, VR comfort problems and compatibility bugs
in the [Flat2VR Discord][flat2vr-discord]. See
[the changelog](CHANGELOG.md#095--release-candidate) for changes since 0.9.0.

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
updates the official repository URL. PCVR was planned at that release.
**v0.9.0** adds buildable Windows and Linux PCVR games, sharing SDL3 window/input/
audio output and FFmpeg media decoding for maintainability alongside the existing
shared game, OpenXR integration and Vulkan renderer. It adds portable versioned
PCVR installations, independent PC/Original Xbox asset variants, PCVR Upstate
Liberty, matched replay benchmarks, vehicle authoring improvements and gameplay,
audio, rendering and manager fixes through `6f13a4b`. See the
[0.9.0 release notes](CHANGELOG.md#090--release-candidate) for the complete changes.
See the [gameplay guide](overlay/docs/QUEST_GAMEPLAY.md),
[spatial audio notes](overlay/docs/SPATIAL_AUDIO.md),
[model guide](overlay/docs/QUEST_MODELSETS.md),
[vehicle editor guide](tools/vehicle-editor/README.md) and
[capture guide](overlay/docs/PERFORMANCE_CAPTURE.md) for details.
Existing graphics and calibration preferences are preserved. See
[the changelog](CHANGELOG.md) for the release history and
[performance notes](PERFORMANCE.md) for the inherited rendering optimizations.

It intentionally contains **no APK, complete re3 source tree, original game
files, saves, logs or original Xbox assets**. Build the application for your target and supply one complete local asset base.

## Updating an existing installation

For Windows or Linux PCVR, run **Build and install** again for the same target. Leave
both asset variants unchecked for an application-only update. To refresh converted
Xbox models, select **Install Original Xbox variant** and reimport the ISO.

For Quest, choose **Update** in the manager. It fetches official main, requires a clean
fast-forward, checks the version floor, reuses cached tools, rebuilds the updated
manager and replaces only the APK. Local changes, divergent history, asset-update
requirements and incompatible signatures stop the update. Use **Dry run** for
local checks, **Build only** to skip deployment, and **Release** with your private
`LIBERTYVR_RELEASE_SIGNING_PROPERTIES` when updating a release-signed application.
The workplace in **Settings** selects the tool/dependency cache.

Quest mission autosaves use `autosave.b` with a separate backup. PCVR uses
`GTA3sfautosave.b` in the persistent user directory, alongside the manual slots.
Upstate Liberty keeps its own save folder.

## What is included

| Path | Contents |
|---|---|
| `overlay/` | Shared VR/OpenXR integration, Android and SDL3/FFmpeg desktop services, and tooling |
| `patches/` | One consolidated diff applied to the pinned user-supplied re3 checkout |
| `librw/` | MIT-licensed librw fork with the shared Quest/PCVR Vulkan backend |
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
packs to Quest or a local PCVR installation. See the [editor guide](tools/vehicle-editor/README.md) for
dependencies, import/deployment, and preview limitations.

## Required external source

The exact patch base comes from the public upstream re3 project:

<https://github.com/mrxenginner/re3>

The tested public `master` commit is
`b050e1cc63b1869e6b514ab2f1a00ef4969e0c84`. The assembly action never modifies
that checkout: it copies it to a new directory, applies the consolidated
`patches/01-re3-liberty-vr.patch`, and adds the port overlay and bundled librw fork.
The single patch contains shared and platform-guarded changes for every target;
see [patch maintenance](patches/README.md).
No private repository is required or referenced.

Complete prerequisites and copy-paste commands are in
[BUILDING.md](BUILDING.md).

Use the manager's **Build and install** page for the guided build workflow on Windows and Linux.

If Options shows missing labels, reinstall the affected PC or Xbox variant through
**Build and install**. For a legacy Quest installation with PC files directly under
`gamedata`, **Repair game files** refreshes the replacement text and frontend assets.

The source-only maintainer checks are in [RELEASING.md](RELEASING.md).

## VR gameplay and rendering on Quest and PCVR
- Room-scale movement, physical weapons, two-hand support, holsters, scoped
  aiming and physical steering for cars and boats.
- Remappable face buttons, grips and stick clicks; a swapped-hands layout;
  per-weapon and per-seat calibration.
- Hands-up surrender: while wanted and on foot, hold both tracked hands at
  head height or higher, within 65 cm of each other, for three seconds. A law
  enforcement officer attacking Claude within 15 metres and with clear sight
  triggers the normal Busted sequence. Lowering either hand, losing tracking,
  opening a menu or leaving combat resets the hold.
- The Dodo uses powered fixed-wing flight. Hold the right trigger to
  build runway speed, then gently pull the left stick back to take off. Use
  left/right to bank, forward/back to pitch, and the left trigger to reduce
  power for landing. Car motion/physical-wheel modes do not affect aircraft.
- In-headset controls, calibration, categorized cheats, traffic, ragdoll,
  graphics and asset source selection.
- Native Main/Pause menu Options and the in-game VR overlay share categorized settings
  implementation, including audio volume, subtitles, language and player skins.
  See [VR settings](overlay/docs/QUEST_SETTINGS.md).
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
- PC (Classic) or Original Xbox as the active asset source. A full restart applies
  the selection to game data, visuals and audio together.

Quest defaults to 100% render scale and requests sustained CPU/GPU performance hints.
Ragdolls and vehicle deformation default to OFF; saved preferences take precedence.
Normal frontend and save loading are used on startup. Experimental temporal AA,
SGSR and runtime-only foveation remain disabled.

The Quest runtime and storage layout are documented in
[QUEST_PORT.md](overlay/docs/QUEST_PORT.md). See
[QUEST_GAMEPLAY.md](overlay/docs/QUEST_GAMEPLAY.md) for gameplay changes,
[QUEST_RAGDOLL.md](overlay/docs/QUEST_RAGDOLL.md) for ragdoll behavior and limits,
[QUEST_VEHICLE_DEFORMATION.md](overlay/docs/QUEST_VEHICLE_DEFORMATION.md) for
body deformation, and [QUEST_MODELSETS.md](overlay/docs/QUEST_MODELSETS.md)
for Classic/Original Xbox asset selection.
The [asset guide](overlay/docs/QUEST_MODELSETS.md) also describes the current
local preparation from a user-provided original Xbox US 1.0 ISO,
including conversion requirements and renderer/cutscene compatibility limits.

## Game data locations

On Quest, the manager installs the selected base under
`/sdcard/Android/data/com.libertyvr.quest/files/gamedata/variants/`. On PCVR,
the configured data directory contains `variants/pc` or `variants/xbox`, and the
user directory holds saves across application and asset updates. See the
[build guide](BUILDING.md#build-and-install) for installation and the
[asset guide](overlay/docs/QUEST_MODELSETS.md) for startup selection and repair.

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
City VR across its Quest and Windows/Linux PCVR targets, with priorities and changes
directed and reviewed by the project maintainer. Planned work includes:

- Fixing reported gameplay, audio, save-system and VR interaction issues.
- Refining tracked controls, vehicle interactions and cutscene comfort.
- Improving Quest rendering performance and stability, including Vulkan resource
  handling and visual correctness.
- Expanding runtime/controller coverage and performance validation of the
  Windows and Linux OpenXR/Vulkan games, manager integration and captured Quest
  workload profile; see [platform scope](PLATFORMS.md).
- Maintaining regression coverage, source assembly, build/update tooling and
  contributor documentation as features evolve.

These are future work areas, not completed features or a release schedule.
Implementation and validation will happen through maintainer-directed development
sessions; headset behavior and performance still require local device testing.

## Distribution boundary

Do not upload assembled source trees, APKs, original game files, saves, prepared
asset variants or signing keys to this repository. Publish only this
build kit. Each player must obtain re3 and the original game separately and
perform the documented build locally.

## No affiliation

This is an unofficial fan project. It is not affiliated with or endorsed by
the publishers or developers of GTA III, Meta, Khronos, Qualcomm or
NVIDIA. All trademarks belong to their owners.
