# Quest gameplay

## 0.9.0 release candidate

Ready for public testing and playing, with finishable main missions. Report
mission progression, comfort and compatibility issues with the mission name,
Classic/Modern model choice and steps to reproduce. Keep saves and game data
local and omit personal device identifiers from reports.

Quest garages retain the player camera and require both the player body and
tracked eyes to clear the delivery doorway. Ammu-Nation shops preserve normal
VR turning and seamless entry/exit while their purchase logic keeps running.
Ambient bathroom and stair cameras no longer take over the player view. Train
passenger views retain the body, tracked hands and immersive HUD, with adjusted
eye/neck clearance. R3 switches cinematic views.

Vehicles settings include immersive car windows and digital/analog speedometers
with shared placement. The Dodo uses full fixed-wing flight physics. Optional
pistol/SMG dual wielding uses separate magazines; the rocket launcher requires
both hands. Holster grab distance is adjustable independently of front holster
placement. See the [weapon guide](QUEST_WEAPON_CALIBRATION.md) for calibration.

Flight/Noclip, weather/time, mission countdown and paginated vehicle spawn
controls are in the cheat menus. See the
[changelog](../../CHANGELOG.md#085--release-candidate) for the release changes.

## Controls and behavior

The in-headset menus group cheats by purpose. Back returns to the parent
category. Never Wanted is under WANTED / POLICE; it clears current pursuit and
blocks new wanted levels while enabled. The override is separate from native
mission/garage flags and is not saved: it defaults to OFF after app restart.
Scripted enemies are not removed.

Tracked weapon aim uses the held barrel pose, a limited range and view cone,
and an obstruction check before dispatching the original pedestrian or police
reaction. Holstered guns, invalid tracking and restricted game states do not
produce that reaction. Police checks are staggered over frames.

The tracked instant-hit path also invokes the original special police
helicopter damage test. World geometry and intervening vehicles still block
the shot; weapon damage rules remain native. Rockets use their own path.

Physical melee follows the full calibrated blade, including the first deliberate
swing after drawing. Contact latching, player-motion compensation and tracking
gap rejection prevent a continuous contact or a tracking jump from repeatedly
counting as a new swing. Fists, the bat and held guns scale blunt damage and
knockback with speed at contact and distance travelled during the stroke. Slow
brushes do negligible damage; moderate strokes deal reduced damage. Full native
damage requires roughly 3 m/s over 30 cm for a fist, or 4 m/s over 55 cm for a
bat or gun. Damage never exceeds native strength. A slowed follow-through loses
strength rather than retaining the swing's peak. Guns use their calibrated
grip-to-muzzle reach and bat-like blunt damage without spending ammunition.
Native pedestrian and car melee targets and obstruction checks still apply.
Run Without Limits bypasses the usual weapon-specific
sprint restrictions while enabled.

Vehicle entry distinguishes movement held during the approach from a new cancel
input after the control returns to neutral. Door locks, speed and vehicle state
checks still apply. Seat centering waits for the actual seat and valid current
head tracking, then centers once. Leaning later does not retrigger it.

The main VR menu has a direct Vehicle shortcut. Ragdoll and vehicle deformation
settings have their own pages, with per-setting explanations and saved tuning.
The culling inspector is available only when building with developer tools;
ordinary graphics culling choices remain available in player builds.

See [QUEST_RAGDOLL.md](QUEST_RAGDOLL.md) and
[QUEST_VEHICLE_DEFORMATION.md](QUEST_VEHICLE_DEFORMATION.md) for contact physics
and optional visual vehicle damage.
