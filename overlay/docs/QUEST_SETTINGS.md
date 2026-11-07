# Shared Quest settings

Main menu / Pause menu → Options and the VR settings shortcut open the same
category catalog, page state, painters and input handlers. The standard menu
keeps ownership of pause and returns to its Settings entry when the shared menu
closes; the gameplay shortcut closes back to play. Held opening/closing inputs
are consumed so they cannot change a setting or immediately leave the parent.

`src/skel/android/QuestSettingsMenu.h` defines category order and destinations.
`vrdebug.cpp` supplies the common pages and controller interaction.
`src/core/QuestGameSettings.cpp` edits inherited engine preferences directly and
saves through `CMenuManager::SaveSettings`; it does not duplicate preferences in
`vr_settings.ini`. The in-car radio board uses the same music-volume and station
handlers. Existing VR-specific persistence remains authoritative for its own
settings, including traffic density and Vulkan PS2 alpha rendering.

The shared menu adds:

- Audio and Radio: music/SFX volume, radio station, board/custom-music setup and
  audio-volume defaults. Defaults affect volumes only.
- Language and Subtitles: installed game languages and subtitle visibility.
  The settings interface itself uses English labels.
- Appearance: asset profile and player skin. Profile changes require restart;
  skin changes apply immediately to a loaded player, or at the next game load.
- Graphics: the inherited draw-distance preference, subject to the existing VR
  minimum-distance and world-LOD policies.
- Locomotion: cinema-screen borders. Stereo cutscenes continue to omit borders.

Desktop keyboard/mouse configuration, window mode, resolution, VSync, desktop
frame limiting, motion-blur trails, provider/speaker selection, reverb and free
camera are absent from the Quest settings path. Legacy 2D brightness dimming is
also omitted: it is not a stereo brightness adjustment. Temporal AA remains
disabled internally and its non-actionable placeholder row is removed.

The old PC settings pages and their reset actions remain available to desktop
builds, but are unreachable through the Quest Options page. In particular, the
PC graphics reset cannot disable the game's PS2 alpha capability or lower full
island residency during a Quest session. The Vulkan toggle is the single
user-facing alpha control.

Regression adapter: `tools/tests/test-shared-settings.ps1` through the manager.
It extracts the production backend and frontend bridge for host tests covering
shared values, bounds, persistence calls, missing language/skin resources,
radio retuning and startup/pause navigation with input release handling.
