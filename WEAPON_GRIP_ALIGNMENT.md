# GTA III weapon grips

The Classic and Xbox sets use the local GTA III `models/generic/weapons.dff`.
The inspected Xbox archive contains no weapon replacements. The previous
defaults came from the desktop port and used its Y-up placement and support
frame. GTA III's rifle/pistol meshes have X along the barrel and Z along the
top. The grenade and bottle also use Z for height. The inherited extra
90-degree X rotation and the universal left-only position correction therefore
do not belong to these meshes.

`QuestWeaponGrip.h` records a primary grip centre for every firearm and
throwable, including the detonator. The default builder cancels that extra
rotation and translates each grip into the closed palm. The initial palm
target is 5.5 cm forward and 4.5 cm below the controller origin, using the
existing wrist calibration. Positions retain the settings system's half-cm
resolution. The bat and unarmed profiles retain their existing behaviour.

The launcher is a separate case: its tube runs diagonally along native Z.
Its 107.5-degree local pitch, muzzle direction, muzzle origin, and support
frame now describe the same tube. The other guns retain their existing
mesh-derived muzzle positions and directions.

Support defaults use version 6: lateral/barrel/top sockets built from GTA
III's native axes, with wrists beside the pump or foregrip rather than at
the muzzle. Left-primary sockets reflect laterally, with mirrored wrist
rotations. Both the calculated frame and current rendered contact frame use
the same axis conversion. Modern profiles retain version 5. Saved v5 and
older support calibrations retain their original frame and migration rules;
saving a calibration preserves its frame version. Explicit INI calibration
values continue to override defaults.

Validation: `test-weapon-grip.ps1 -StageRoot <assembled source>` tests the
production defaults, both hands, Classic/Xbox equivalence, palm placement,
support mirroring, old frame compatibility, and launcher aim alignment.
The current-source regression runner includes this suite. The existing
held-weapon frame suite checks hand/weapon rigidity across frame rates.

These are geometry-based starting profiles, not headset-verified finger
contact measurements. The supplied screenshots identify the AK, pistol,
and grenade problems; final wrist orientation, finger penetration, and
comfort for the complete arsenal still require headset feedback. No game
meshes, textures, or extracted vertex data are included in the change.

Local validation (2026-10-21): 121 grip checks and 52,268 held-frame checks
passed. The final debug APK built successfully and installed with `adb
install -r` on verified Quest 3 `personal Quest 3 device`. The broader source regression
run passed 20 suites before stopping at the unchanged graphics-menu fixture:
MSVC C4505 (`QuestMenuPageColour` unused) is treated as an error there. The
remaining suites in that runner did not run; this is not a full-suite pass.
