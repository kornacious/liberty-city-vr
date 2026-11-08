# Changelog

The port started from Vice City VR 0.5.6. Liberty City VR 0.6.0 marks the
port from GTA Vice City to GTA III. Later entries describe Liberty City VR releases.

## Unreleased — Windows OpenXR/Vulkan game integration

- Fixed a major discrete-GPU texture loading/streaming bottleneck: alpha-weighted
  mip generation no longer reads uncached Vulkan staging memory. When the actual
  upload allocation lacks HOST_CACHED, filtering uses temporary CPU memory and
  copies the finished chain into staging once. Cached allocations retain their
  existing path, and texture filtering/alpha behavior is preserved.
- Added a synthetic texture upload benchmark with GPU sampling/readback of
  alpha-weighted mip colors in both eyes. An initial matched local test of 80
  256x256 RGBA uploads fell from 2,876 ms to 78 ms CPU time on the RX 6700 XT;
  this measures that upload workload, not overall game or Quest performance.
  The user confirmed much faster loading and streaming across the tested Windows
  transitions. All seven Windows checks passed; the shared Quest APK also built
  and updated successfully on the personal Quest 3 device.
- Fixed washed-out Windows menus/gameplay and the incorrect color-space handoff
  contributing to dark-area banding. Windows now submits sRGB swapchains with
  mutable UNORM attachment views, preserving re3's display-encoded shading and
  blending while identifying the image correctly to the compositor. The CPU-uploaded
  overlay uses the same sRGB interpretation. Android retains its existing format policy.
  The user confirmed normal brightness and reduced banding through VirtualDesktopXR.
- Added a Vulkan GPU regression test that reads back nine encoded gray levels
  in both eye layers through the mutable sRGB/UNORM path, alongside stereo geometry
  checks. Windows now requires an advertised sRGB format instead of silently using
  the incorrect linear UNORM handoff.
- Added an experimental Windows x64 game executable using the shared re3 loop,
  OpenXR session/actions, tracked VR gameplay, wrist/theatre menus and renderer.
  Shared game-source definitions and Windows portability adapters preserve the
  Android lifecycle and platform services for Quest.
- Added manager CLI source assembly from clean pinned re3 and isolated Windows
  data preparation, rejecting existing/overlapping destinations and symbolic links.
  Original game files and saves stay untouched; desktop saves and generated audio
  caches use a separate user directory.
- Split native effects output from the shared spatial/HRTF mixer. Quest retains
  AAudio; Windows uses event-driven 48 kHz stereo WASAPI with MMCSS scheduling
  and aggregate buffer/mixing diagnostics, replacing the initial waveOut output
  after reports of crackling engines and sirens. The user confirmed clean playback
  with music preserved; the follow-up run recorded zero empty output buffers.
- Added Media Foundation music streams with seek/pause/loop/volume and duration
  handling. Fixed silent retail ADPCM music with local ACM decoding into a reusable
  PCM cache, preserving original files.
- Added Windows file/streaming and atomic-save adapters, optional injected RenderDoc
  support and local crash stack diagnostics. Fixed an exit crash by releasing
  frontend textures before closing the renderer.
- Added synthetic PCM/IMA playback, cache and duration checks and manager
  data-isolation coverage. Documented the experimental desktop game workflow.

- Added a Windows x64 CMake diagnostic executable using the existing librw
  Vulkan backend and GLSL/SPIR-V shaders. Extracted shared shader/backend build
  definitions from Android rather than creating a separate desktop renderer.
- Replaced backend Android-only logging and POSIX timing with a portable
  boundary that retains Android logcat and uses monotonic clocks on both targets.
  Extracted stereo projection and pose math for use by Quest and Windows.
- Added runtime-selected Vulkan GPU/device/session negotiation through
  `XR_KHR_vulkan_enable2`, explicit Vulkan 1.1/stereo multiview checks, supported
  feature negotiation without requiring ASTC, and array swapchain rendering.
- Added a tracked stereo triangle scene, basic controller grip/select bindings,
  session/focus handling, cleanup, bounded frame-count controls, and separate
  Vulkan-only, offscreen renderer and OpenXR negotiation probes.
- Added Windows host checks for pose inverses, asymmetric FOV, Vulkan clip/depth
  conventions, API ranges, graphics queues, optional features and CLI errors.
- Fixed invisible diagnostic geometry by initializing the backend's no-camera
  world-to-play transform. The offscreen GPU test now reads back both eye layers
  and rejects missing triangle pixels or incorrect stereo disparity, rather than
  treating successful command submission as proof of rendered geometry.
- Added Windows build/validation documentation and pinned, SHA256-checked
  Khronos OpenXR loader source fallback. Original game data is not required.
- Interactive manager target selection, desktop packaging and the Quest
  compatibility profile remain planned; Windows game support is experimental.
  The diagnostic deliberately serializes GPU submissions and is not a benchmark.

## 0.8.5 — Release candidate

Quest source-build release candidate. Changes since the 0.8.0 version commit
`ad84866`, including the project identity, repository and platform-scope update.
Windows PCVR is a planned target; this release does not include a PCVR game build.

### Project scope, repository and version

- Broadened the project identity to **Liberty City VR**, covering the implemented
  standalone Quest 3 target and planned Windows x64 PCVR using OpenXR and Vulkan.
  Keep shared re3 patches, VR gameplay, librw rendering and shaders in one repository.
- Renamed the official repository to `kornacious/liberty-city-vr`. Updated the
  manager's official update/clone URL and documentation examples. Existing local
  checkout names, application IDs, storage paths and signing identities remain valid.
- Added `PLATFORMS.md` with target status, platform separation, Windows bring-up,
  future manager target selection and a planned desktop Quest compatibility profile.
  Desktop estimates and synthetic timings are not measured Quest performance.
- Updated scope instructions, README, build/release guidance, manager and Quest
  documentation to distinguish host tools, implemented Quest support and planned PCVR.
- Set Android `versionName` and native About/menu text to **0.8.5**, Android
  `versionCode` to **805**, and current release references to 0.8.5. Historical
  release numbers and third-party dependency versions retain their original meaning.

### Original Xbox assets, body and camera

- Replaced the downloaded Xbox HD pack importer with local Original Xbox retail
  ISO conversion. Added native XDVDFS extraction and Xbox model/texture conversion;
  removed the retired HD-pack manifest, texture compressor and associated tests.
  Original game assets remain local and are not bundled or downloaded.
- Added Original Xbox audio and particles to restart-based Asset Source selection,
  plus Vulkan Neo material effects and optional rain droplets. Fixed explosion
  particle frame selection and pedestrian head/body rim lighting.
- Fixed Xbox Claude skin-hierarchy camera eligibility and head-bone axes. Attach
  the first-person view to the native head, correct tilt and eye placement, honor
  walking head bob and keep knockdown views attached to the head.
- Added tracked crouching, leaning and physical stepping for Xbox body IK. Align
  the body to the camera/head target, synchronize room-scale gameplay movement,
  plant standing feet, and correct pelvis balance, crouched stance and knee/foot
  targets. Stabilize body facing through physical turns and IK resets, and follow
  native car-entry facing.
- Corrected Xbox train window-facing passenger views and head following. Keep
  exit views facing outward and preserve facing through boarding/exit handoffs.
- Fixed jump launch direction after physical headset turns and physical-turn
  alignment during jumps and sniper locomotion.

### Gameplay, settings and menus

- Added tracked hands-up surrender into the native police arrest flow and a
  saved option controlling visibility of holstered weapons.
- Unified native Main/Pause Options and the VR overlay around shared categorized
  Quest settings. Restored original native menu typography, audio controls,
  radio previews and menu feedback; settings remain available through both routes.
- Moved Simulation Replay controls into live-game Graphics > Optimizations.
- Explain save slots from a different game configuration rather than presenting
  an unexplained incompatible slot. Existing saves and configuration are preserved.

### Upstate Liberty and manager

- Added curated Upstate Liberty v1.1.2 management: local archive validation or
  pinned download, install/status, enable, disable and delete, with verified
  staging, rollback, runtime capability gating and save preservation.
- Added the native Upstate engine adapter with expanded engine capacities,
  package/runtime validation and isolated mod saves. Windows ASI binaries are
  excluded from headset packages; map assets remain locally managed.
- Preserve the selected Original Xbox asset preference while Upstate is active
  and support Xbox assets beneath the Upstate overlay.
- Recorded local Upstate/Xbox gameplay acceptance in the relevant guides;
  this does not establish Windows PCVR or universal mission compatibility.
- Fixed manager updates to require and fetch official `main`, reject legacy
  `master`, and preserve local changes. Updated the active version-floor fixture
  for version code 805.

### Rendering, diagnostics and maintenance

- Adapted retained developer tools and culling diagnostics to Liberty's renderer.
  Removed obsolete inherited test runners/fixtures targeting missing modules,
  reVC-only behavior or private historical snapshots; documented coverage limits.
- Added live visualization controls for mesh/wireframe views, hidden-edge
  removal, diagnostic shading, xray/depth-tested mesh overdraw, category visibility,
  collision bounds/shapes and velocity vectors. Reset restores normal rendering;
  diagnostics do not change saved graphics preferences or simulation.
- Added live geometry/draw counters to the full profiler and host coverage for
  visualization topology, Vulkan pipeline isolation, deferred edge rendering
  and graphics-menu layouts. These diagnostics are not hardware invocation counts.
- Fixed sniper overlay coverage, per-eye alignment and depth, with host
  regression coverage for mask geometry, stereo projection and depth behavior.
- Clarified project licensing and toolchain attribution, and retired inherited
  Vice City workflows including the incompatible save converter. Historical
  credits and compatibility identifiers retain their context.

## 0.8.0 — Release candidate

Ready for public testing and playing, with finishable main missions. This is a
release candidate; feedback on mission progression, VR comfort and compatibility
is welcome. Distribution remains a source build requiring legally owned local
GTA III PC data.

Changes since the 0.7.0 version commit
`171c58921b5ddb9b442cc595edda703e08122bd4` through `2aa5753`
(the last gameplay change before this version bump).

### Missions, controls and VR gameplay

- Fixed garage camera overrides and mission delivery exit clearance, including
  room-scale eye position and reopening when the player returns during closure.
- Made Ammu-Nation entry and exit seamless: retain normal VR turning and bypass
  fades, staged teleports and control freezes while preserving purchases.
  Bypass ambient Belleville Park bathroom cameras and fixed stair camera zones.
- Fixed original-model cutscene actor camera selection and restored R3 cinematic
  switching. Refined train passenger eyes, neck clearance and boarding turns;
  retain the passenger body without its head, passenger IK and immersive HUD.
- Corrected crouch body/head alignment and recovery, directional locomotion
  animations, recenter heading and forward braking slides. Fixed ungripped hand
  flicker in moving vehicles and hide the body when tracked hands are disabled.
- Added optional pistol/SMG dual wielding with independent magazines. Restored
  rocket launch speed and require both hands to fire the launcher. Fixed weapon
  laser disabling, throwable overlays and NPC reactions to silent VR aiming.
- Scale physical melee strength by stroke speed and distance; allow blunt strikes
  with held guns. Added adjustable holster grab distance and independent front
  holster adjustment while keeping shoulder reach fixed.
- Enabled full fixed-wing flight physics for the Dodo. Added digital and analog
  immersive car speedometers with shared placement, immersive car-window
  visibility and separate Vehicles settings pages. Fixed emergency siren double
  taps, third-person steering animation, radar road masks and pedestrian step-down.
- Added Flight and Noclip cheats, Town Hall recovery, weather/time and mission
  countdown controls, paginated vehicle spawning and driver-seat selection.
  Improved cheat labels and toggle states; traversal boost uses the right
  thumbstick with a higher boosted speed and trigger boost control.
- Adapted ragdolls and visual vehicle deformation to GTA III native contacts,
  original and skinned NPC rigs and pre-impact motion. Preserve evasion/getup
  animations and allow NPC recovery beside cars when current geometry is clear.
  Pedestrian body contacts no longer damage vehicles.
- Centered startup branding and mapped Quest B to menu Back.

### Audio, models and placement tools

- Added tracked-listener spatial audio with measured HRTFs for world localization.
- Consolidated Classic and Modern (Xbox HD) model selection around a pinned GTA III
  Xbox Version HD re3 pack. Load matching world data and Claude skin, restore
  shared texture fallback, repair HD wheel textures and retain opaque vehicle
  body depth while blending glass. Improved installer SDK discovery, native
  warning handling, preparation prompts and progress reporting.
- Added a standalone vehicle placement editor with Classic/Modern profiles,
  cameras, render/radio previews, saved preferences, placement transfer and
  acknowledged live synchronization. Bundle calibrated per-model placements
  in the APK, separate from personal overrides.
- Added optional local radio conversion to Quest-native AAC-LC/M4A in the manager;
  validate replacements before removing headset WAVs. Keep FFprobe diagnostics
  out of radio metadata JSON.

### Manager, music and saves

- Consolidated build/install, update, model installation, repair, editor building
  and auxiliary tools/tests in the Windows/Linux .NET 10 manager. Use MANAGE.bat
  or MANAGE.sh; the old per-action root launchers are removed.
- Added native bidirectional PC/Quest save transfer with multiple slots, dedicated
  Autosave, backups and read-back verification.
- Added headset music management with metadata captions, ordered playlists,
  range/multi-file selection, group reordering and deletion. Imported tracks
  append after the current playlist order.
- Sanitize device diagnostics and keep private device identifiers out of menus,
  settings and shared logs. Migrated repository privacy hooks and checks to the
  native manager.

### Rendering, performance and diagnostics

- Added 75% and 90% render scales, world LOD quality modes and a graphics
  optimization submenu. LOD fade transitions, stereo world occlusion,
  backface culling, opaque ordering, mesh batching and static instancing include
  guarded benchmark/experimental controls. Existing graphics preferences remain.
- Reduced redundant Vulkan frame/pipeline and static-world state work. Added
  proven-opaque shader selection, empty alpha-pass rejection, road culling
  outside both tracked views and guarded native-resolution postprocessing.
- Fixed boat-window depth hiding water and distant-building sun/flare occlusion;
  restore smooth sun glow and flare visibility transitions.
- Added automobile simulation recording/replay, ghost mode, route assistance,
  forward-view benchmarks and automatic captures. Expanded raw Quest performance
  capture, player/vehicle context, sampled geometry/material ownership, draw counts
  and whole-render-pass GPU timing. Added replay-timed RenderDoc captures and
  scoped read-only local capture export; minimal profiling shows replay time.

## 0.7.0

Features and fixes added after the 0.6.0 GTA III port baseline.

### GTA III integration, controls and reliability

- Added full-body IK and articulated GTA III-style hands, including support for
  non-skinned PC models, room-scale tracking, vehicle entry and cutscene cameras.
- Improved weapon grips, bore-centered firing and aiming, factory calibration
  profiles and calibration controls. Added a saved pistol trigger mode and
  restored native firearm cadence.
- Added physical scope magnification, eyepiece alignment and right-thumbstick zoom.
- Added saved laser or fingertip touch interaction for the vehicle radio board,
  and corrected station retuning and custom music uploads.
- Corrected left-hand steering-wheel orientation and tracked hand continuity.
- Added fine and turbo HUD alignment steps and fixed wrist wanted-star clipping.
- Added transactional mission autosaves with backup recovery while preserving
  all eight manual save slots.
- Fixed texture/archive loading crashes, archive directory capacity, save-load
  streaming hangs and Vulkan texture descriptor lifetime.
- Added Android 3D audio distance attenuation, low-rate resampling fixes and
  stereo IMA ADPCM playback for GTA III radio and ambient streams.
- Normalized timers, vehicle controls, physics settling, doors, destruction and
  gameplay schedules against elapsed game time.
- Neutralized tracked controller input after OpenXR focus loss.
- Corrected world sprite facing and texture stability during head roll.
- Fixed startup at refresh rates other than 72 Hz and declared supported Quest
  devices for 80, 90 and 120 Hz refresh-rate options.
- Corrected Xbox asset initialization, collisions and companion wheel textures.
- Added opt-in vehicle capture/input replay and restored host regression coverage.

### Additional features and fixes

- Fixed planes disappearing when turning away from the original camera view in
  VR. Their big-building visibility path now uses the shared VR visibility
  policy before the existing stereo culling; mono/theater behavior is preserved.

- Fixed mission meter labels (including damage and spook) inheriting unrelated
  font settings when the clock is hidden. Labels now establish their own font
  and right alignment to remain beside the meter as its fill changes.

- Added a head-anchored knockdown/get-up view with gentle automatic turning,
  floor/surface protection and smooth recovery. Tracked hands, weapons and
  aiming aids hide until head-height/tilt recovery completes. Knockdowns return
  held weapons to their holsters and require a fresh grip to draw them again.
  Recovery follows voluntary turning; the final yaw correction cannot keep
  hands hidden or resume upper-body IK before the head pose is ready.
  Locomotion's Knockdown View defaults to Level Horizon; Immersive Tilt adds
  bounded pitch and roll while retaining physical head tracking.

- Added a custom soundtrack station with background indexing, incremental local
  playback, previous/next, pause and persisted song positions. Added validated
  Windows/Linux import tools using staged uploads and headset SHA-256 checks.
- Added a vehicle-mounted radio board with pointer/trigger controls while holding
  X, global/per-model placement, save/cancel/reset, and shareable layout exports.
  Bundled model defaults remain separate from personal overrides.

- Added a minimal performance overlay with FPS above frame time, fitted to the
  upper right of both eyes' visible area. Minimal mode avoids full profiling overhead.

- Fixed mission selection falsely reporting "MISSION NOT SAFE RIGHT NOW" for
  background main-script threads, and preserved those threads during switches.
- Kept VR random traffic on the distant spawn ring in every direction and
  protected nearby traffic from mono-camera offscreen cleanup, independently
  of the experimental director settings.
- Made every inventory weapon independently selectable in the VR holster loadout,
  including the rocket launcher, grenade, AK-47 and detonator. The weapon cheat
  now includes the detonator; the chest center can select grenade or Molotov.
- Kept one EMPTY holster choice and displayed assigned weapon names even when
  that weapon is not owned.
- Made the wasted transition fade to pure white across both complete eye views,
  preserving its fade-out and respawn fade-in timing.
- Unified VR startup, loading and splash fades on the cyan artwork, fitted at
  its native aspect ratio without cropping, with black bars in unused space.
- Fixed loading artwork appearing independently in each eye and skewing with
  head movement. The fade now uses one shared stereo plane at the theater
  screen's size and distance, independent of the HUD safe-area scale.
- Made the cyan loading artwork and its black surround fade as one layer,
  revealing the scene uniformly without double-darkening beneath the image.
- Added the optional Xbox assets profile to the model downloader and asset menu.

## 0.6.0

Ported the established Vice City VR 0.5.6 foundation from GTA Vice City to
GTA III/re3 for Meta Quest. This is the Liberty City VR port baseline; the
features below were inherited from that foundation.

- Restored vehicle drive-by gun controls: left grip + B left, right grip + B right.
- Added explosion reactions for existing ragdolls and bounded late activation of nearby corpses.

- Ragdolls and vehicle deformation are disabled by default.
- Fixed Modern vehicle impact selection and strong dents being reduced by mesh protection.
- Removed developer quick-start and physics diagnostics from player builds.

### Ragdolls and vehicle contact

- Added articulated falls and corpse reactions with anatomical hip, knee and
  spine limits, limb self-contact and consistent leg orientation.
- Added bullet and car impacts, swept contacts against nearby walls and objects,
  and contact friction that permits bodies to ride on a bonnet.
- Added a RAGDOLL submenu for body weight, vehicle braking, grip and bullet
  reaction. Braking can be increased up to 2000%.
- Default body weight is now 200%, alongside 200% braking, 150% grip and 100%
  bullet reaction. Saved settings remain in effect.
- Retained corpse poses when many bodies are present and separated pose storage
  from the bounded simulation scheduler.
- Corrected culling and bullet bounds when a body moves away from its original
  pedestrian position. Carried bodies now follow the moving car consistently
  between simulation updates, without the previous back-and-forth jitter.

### Vehicle deformation

- Added optional local dents to vehicle body geometry. Bumpers, grilles, chassis,
  bonnet and adjoining panels can respond to the same impact.
- Added saved controls for strength, affected radius, maximum dent depth and
  minimum impact threshold, with a reset to defaults.
- Corrected impacts between both cars and preserved the contact's local frame
  while a car moves or turns before its queued deformation finishes.
- Added bounded processing independent of whether a car is currently visible;
  empty or unsuccessful jobs no longer exhaust available car records.
- Preserved transparent and identifiable interior, engine and wheel surfaces,
  texture coordinates, shared source geometry and the cumulative dent limit.
- Deformation is a visual approximation. Repeated strong hits can open seams
  between separately authored panels; it is not a structural vehicle solver.

### Controls, gameplay and menus

- Police react to a visible tracked gun aimed at them through the native wanted
  system. Holstered guns and targets behind walls are excluded.
- Restored the special police helicopter's native bullet-damage path for tracked
  instant-hit weapons, preserving obstruction and weapon damage rules.
- Corrected the first deliberate physical melee swing after drawing a weapon.
- Run Without Limits also bypasses weapon-specific sprint restrictions.
- Organized cheats into submenus and added Never Wanted with a separate police
  override. Disabling it restores normal wanted behavior.
- Fixed car entry stopping after the door opens when movement was held during
  the approach. Returning the movement control to neutral arms explicit cancel.
- Centered the viewpoint once the actual vehicle seat and a valid tracked head
  pose are ready, while preserving subsequent leaning and seat calibration.
- Improved graphics-menu spacing and separated the last setting, Back and footer.
- Added a direct Vehicle shortcut to the main VR menu.

### Source release

- Added `UPDATE.bat` / `UPDATE.sh` to update source, reuse build-tool caches and
  install a new APK without repeatedly downloading the source kit. Local edits,
  saved data and optional asset overlays are preserved.

- Retained the Vice City VR 0.5.5.1 rendering, mip-worker and settings-cache
  optimizations carried forward in its 0.5.6 foundation.
- Updated source-only build documentation, portable test inputs and release
  inventory checks. The culling inspector remains in source behind the explicit
  developer-tools build option; normal player builds omit its diagnostic path.
- Builds contain application code only. Players supply their own game data;
  no game archive, save, signing key or generated Modern model set is included.

Host regressions cover production physics, contacts, menus, state transitions
and real locally supplied vehicle meshes. Clean assembly and APK compilation
are separate checks from headset appearance and frame-time validation.
