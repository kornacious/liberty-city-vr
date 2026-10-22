# Changelog

## 0.5.6

- Added a head-anchored knockdown/get-up view with gentle automatic turning,
  floor/surface protection and smooth recovery. Tracked hands, weapons and
  aiming aids hide until recovery completes, preserving held weapon slots.
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
- Added the optional Xbox assets profile to the model downloader and asset menu.
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

- Retained the 0.5.5.1 rendering, mip-worker and settings-cache optimizations.
- Updated source-only build documentation, portable test inputs and release
  inventory checks. The culling inspector remains in source behind the explicit
  developer-tools build option; normal player builds omit its diagnostic path.
- Builds contain application code only. Players supply their own game data;
  no game archive, save, signing key or generated Modern model set is included.

Host regressions cover production physics, contacts, menus, state transitions
and real locally supplied vehicle meshes. Clean assembly and APK compilation
are separate checks from headset appearance and frame-time validation.
