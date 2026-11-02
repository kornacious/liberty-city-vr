# Quest weapon calibration

Open Settings > Weapons > Weapon Calibration while holding the weapon to edit.
The heading identifies the weapon and the selected edit hand. Six submenus
separate aim offset, aim rotation, weapon offset, weapon rotation, support
offset, and support rotation. Each submenu contains Edit Hand, Step, X/Y/Z,
and Back. Support grip style and the per-weapon laser switch stay on the root.

- Left stick selects a row. L2 subtracts; R2 or A adds. Hold a trigger to repeat
  numeric changes. A or R2 also opens submenus and activates action rows.
- Y toggles Normal (0.5 cm/degrees) and Turbo (10 cm/degrees). The Step row
  shows the current mode and also toggles it. The mode carries between
  calibration submenus; it affects numeric values only.
- Rotations wrap across -180/+180 degrees in either direction, including turbo
  steps. Distances retain their existing limits.
- Save As Default stores a personal checkpoint for this weapon and edit hand.
  Ordinary edits continue to auto-save separately.
- Restore Default restores that checkpoint. Without one, it restores the
  built-in profile for the active weapon model set.
- Copy Values To Left/Right Hand copies all 19 calibration values and their
  support frame version exactly, without mirroring. It auto-saves the other
  hand's working calibration and leaves both personal checkpoints intact.
- Restore Factory Values restores the built-in profile for the selected
  weapon/hand without deleting its personal checkpoint.
- B or L3 returns from an axis submenu to its category on the calibration root;
  from the root it returns to Weapons.

Personal checkpoints live in `vr_settings.ini`, under
`VRWeaponDefault_<id>_<name>` or `VRWeaponModernDefault_<id>_<name>`.
Left-hand keys use the existing `Left` prefix. Classic and Xbox share the
generic weapon profiles and checkpoint namespace. Modern is separate.
The laser remains a per-weapon setting and is not included in a hand checkpoint.
The Weapons page laser switch disables all weapon lasers, including saved ON
overrides. Enable it to use the individual weapon choices again.
Holster Loadout saves **Front Grab Distance**: FAR (27 cm, the original reach),
MEDIUM (20 cm), and CLOSE (14 cm). This applies to waist and chest holsters.
Shoulder/back holsters always use FAR (27 cm), regardless of this setting.
Distance is measured from the controller grip to the holster point. Previously
saved VERY CLOSE or PRECISE settings load as CLOSE.
This controls drawing from holsters; returning and catching weapons retain
their existing distances.

The centre chest holster cycles through Molotov, grenade and EMPTY; EMPTY is
saved and leaves that holster unused after restarting.

Pistol and SMG each have a saved **Dual Wield** switch, off by default. The
Weapons page edits the right-hand weapon (or the available held/current weapon);
Weapon Calibration lets you choose the hand to edit. The switch shows
UNAVAILABLE for other weapons. Enable it, then assign the same sidearm to one
left and one right point in Holster Loadout. Disabling it keeps the first
assignment and saves the duplicate as EMPTY, releasing the second copy if held.
Each copy keeps its own magazine, reload timer and firing cooldown through
hand transfers and holstering. Both use the inventory ammunition total; rounds
loaded in the other gun cannot be used to reload this one. A new second copy
starts empty and uses the normal timed or physical reload.

Grip style now has a per-hand key, with compatibility fallback for older files.
Installing this update does not replace existing INI calibrations or saves.

The factory profiles now include the headset-authored checkpoints captured on
2026-10-21: 22 independently saved hand profiles across 12 weapons. The exact
values, grip styles, and support-frame versions are recorded in
`src/vr/QuestWeaponFactoryDefaults.h`. Classic/Xbox fresh defaults and Restore
Factory Values apply these profiles after the older fallback calculations.
Modern keeps its existing factory profiles. Grenade and detonator had only
left-hand checkpoints; their right-hand factory values remain unchanged at the
user's request. Current working calibrations and personal checkpoints continue
to take precedence and are not replaced during installation.

Validation: 429 production grip/calibration checks cover fine/turbo edits,
rotation wrapping, distance limits, checkpoint persistence, hand/model-set
isolation, copy, and factory fallback. The menu suite passes 2,362 checks,
including the existing cheat controls and the new calibration row mapping,
repeat gating, Back navigation, and text/row bounds for the longest weapon
heading. The debug APK builds successfully. These automated checks do not
replace a headset usability check of the new controls.

Factory-profile validation passes 2,186 grip/calibration checks. Every field
from each of the 22 checkpoint profiles is checked through both initial factory
loading and Restore Factory Values for Classic and Xbox. The imported table
was also compared against the local saved checkpoints without unit conversion
or mirroring. Tests retain coverage for the geometry fallback, Modern profiles,
and the two intentionally unchanged right-hand profiles.
