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
Grip style now has a per-hand key, with compatibility fallback for older files.
Installing this update does not replace existing INI calibrations or saves.

Validation: 429 production grip/calibration checks cover fine/turbo edits,
rotation wrapping, distance limits, checkpoint persistence, hand/model-set
isolation, copy, and factory fallback. The menu suite passes 2,362 checks,
including the existing cheat controls and the new calibration row mapping,
repeat gating, Back navigation, and text/row bounds for the longest weapon
heading. The debug APK builds successfully. These automated checks do not
replace a headset usability check of the new controls.
