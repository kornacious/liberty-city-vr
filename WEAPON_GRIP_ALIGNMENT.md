# GTA III weapon grips

The Classic and Xbox sets use the local GTA III `models/generic/weapons.dff`.
The inspected Xbox archive contains no weapon replacements. GTA III's
rifle/pistol meshes have X along the barrel and Z along the top; the grenade
and bottle also use Z for height. This mesh convention is distinct from the
calibrated controller/hand basis. The established base rotation must remain:
removing its 90-degree X component rolled every weapon sideways in the headset.
The 2026-10-21 follow-up screenshots confirmed that regression.

`QuestWeaponGrip.h` records a primary grip centre for every firearm and
throwable, including the detonator. The default builder retains the established
controller-to-hand roll and translates each grip into the closed palm. It
calculates translation after rotating the grip, so correcting the roll does not
move the physical contact point. The initial palm target is 5.5 cm forward and
4.5 cm below the controller origin, using the existing wrist calibration.
Positions retain the settings system's half-cm resolution. Primary grip data
and the native model-bound support sockets remain separate from the hand basis.

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
install -r` on the personal Quest 3 device. The broader source regression
run passed 20 suites before stopping at the unchanged graphics-menu fixture:
MSVC C4505 (`QuestMenuPageColour` unused) is treated as an error there. The
remaining suites in that runner did not run; this is not a full-suite pass.

Follow-up roll correction: 341 grip checks pass, now exercising the production
model-matrix builder for both hands and three hand orientations. They check
mesh top relative to the calibrated hand, barrel direction, and palm contact.
Restoring the previous bad roll and its matching translation as a negative
control fails specifically at the hand-relative mesh-top assertion. The initial
121 checks validated positions and missed this orientation error. The corrected
APK also builds successfully; final finger contact still needs headset feedback.

The follow-up APK installed successfully with adb install -r on the personal
Quest 3 device, preserving existing application data.

Rocket launcher muzzle correction (2026-10-21): the original approximate native
muzzle `(-0.070, 0.040, 0.750)` was about 7 cm radially off the tube centreline.
The measured front opening centre is `(-0.070460401, 0.114642583, 0.783905983)`.
The rear centre is `(0.251384735, -0.029805297, -0.247366756)`; their normalized
difference gives the bore direction `(-0.295286557, 0.132528077, 0.946172372)`.
The previous direction omitted the tube's native Y component.

Classic/Xbox aiming now transforms this point and direction through the current
rendered weapon matrix. The laser and projectile launch use that same aiming
path. Modern retains its existing muzzle profile. Saved hand profiles and the
support-socket frame remain unchanged so this correction does not move the
headset-authored grips.

Validation passes 2,304 grip/calibration checks, including muzzle-centre and
bore-line assertions for both hands, Classic/Xbox, and six rigid poses per
combination. Restoring the previous approximate muzzle in a host-test negative
control fails the opening-centre assertion. Mesh measurements remain in the
ignored local cache; no game assets are included in the source change.
