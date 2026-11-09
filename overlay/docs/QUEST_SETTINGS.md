# Shared Quest settings

Main menu / Pause menu → Options and the VR settings shortcut open the same
category catalog, page state and input handlers. Frontend Options renders those
shared rows through GTA III's native fonts, textured background and selection
bar on the game's cinema screen. The gameplay shortcut retains the VR overlay.
Native pages paginate automatically as the selection moves. Main categories are
centered without OPEN labels or an item-position counter. Entry typography uses
the original GTA III heading font and frontend scales. Submenu rows use compact
spacing and balanced left/right insets. The standard menu
keeps ownership of pause and returns to its Settings entry when the shared menu
closes; the gameplay shortcut closes back to play. Held opening/closing inputs
are consumed so they cannot change a setting or immediately leave the parent.

With both hands empty, hold both grips and press Y to open or close Simulation
Replay, or B to open or close Cheats. Both shortcuts work on foot and in vehicles.
Index triggers are optional; holding them while opening Simulation Replay does
not activate a replay action.

`src/skel/android/QuestSettingsMenu.h` defines category order and destinations.
`vrdebug.cpp` supplies the common page definitions and controller interaction.
`QuestNativeSettings` collects their rows and renders native label/value columns;
it owns no copies of the settings or action handlers.

Frontend settings hide weapon/hand and head/body calibration, HUD placement,
vehicle seat/wheel calibration, radio and speedometer placement, recenter,
holster loadout, Dual Wield, About, cheats, spawning, missions, replay, profiler and renderer
diagnostics. Drawing, navigation and activation use the same availability rules.
Persistent preferences such as HUD visibility, speedometer style/units, weapon
behaviour, traffic and graphics quality remain editable before loading a game.
`src/core/QuestGameSettings.cpp` edits inherited engine preferences directly and
saves through `CMenuManager::SaveSettings`; it does not duplicate preferences in
`vr_settings.ini`. The in-car radio board uses the same music-volume and station
handlers. Existing VR-specific persistence remains authoritative for its own
settings, including traffic density and Vulkan PS2 alpha rendering.

The shared menu adds:

- Audio and Radio: native segmented music/SFX volume sliders, station playback
  previews with the original radio logo strip and selected-station highlight,
  board/custom-music setup and audio-volume defaults. Previews follow
  station changes and stop when leaving Audio or selecting Radio Off. The
  gameplay overlay retains its normal radio behaviour. Defaults affect volumes only.
- Language and Subtitles: installed game languages and subtitle visibility.
  The settings interface itself uses English labels.
- Appearance: asset profile and player skin. Profile changes require restart;
  skin changes apply immediately to a loaded player, or at the next game load.
- Graphics: the inherited draw-distance preference, subject to the existing VR
  minimum-distance and world-LOD policies.
- Graphics → Optimizations → Simulation Replay: performance recording and
  profiling during live gameplay. Hidden in startup and frontend settings,
  where the world is unloaded or paused. Back returns to Optimizations.
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

Shared Settings input emits the original navigation, setting-change, new-page
and Back sounds once per input pulse or transition. The frontend Options hook
retains the original pre-game `DMAudio.Service` call through
`QuestGameSettings::ServiceFrontendAudioFrame`. Preview and feedback APIs queue
requests; that service is required to turn them into audible playback. Loaded
games keep their existing audio service path, avoiding a duplicate update.

Regression adapter: `tools/tests/test-shared-settings.ps1` through the manager.
It extracts the production backend and frontend bridge for host tests covering
shared values, bounds, persistence calls, missing language/skin resources,
radio retuning and startup/pause navigation with input release handling.
