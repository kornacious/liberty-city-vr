# Original Xbox eye fit and knockdown attachment

2026-11-06, local Windows implementation and validation. No retail asset access,
reimport or game-data transfer was needed.

The native animation head joint was used as the eye midpoint without anatomical
eye clearance, and roomscale displacement remained active during knockdowns.
The comfort basis also overrode downed head orientation. This could expose the
neck and let the viewpoint move away from Claude while he was on the ground.

The Xbox camera now adds an adjustable eye fit: initially 6 cm forward, 8 cm up
and 0 cm right. These are starting values for visual fitting, not a claim of
measured retail eye coordinates. Body yaw retains its existing -6 degree value
but is adjustable. Locomotion > XBOX HEAD / BODY FIT exposes these values, plus
knockdown movement lock and head-bone/level view. Changes apply immediately and
persist across restart; personal save/restore defaults remain separate from
factory reset. Classic does not consume these calibration settings.

During Xbox knockdown/get-up, physical translation is locked to the native head
by default using the existing train/backend tracking-origin mechanism. Native
head orientation is followed in HEAD BONE mode; headset rotation remains active.
Walking animation rotation stays excluded. Releasing a position lock captures
the current physical reference to avoid replaying downed movement on recovery.
The existing Locomotion knockdown row shares the submenu's Xbox view setting.

Validation:

- 22,234 production native-head/final-camera checks passed, including bone-axis
  eye offsets, downed orientation and level recovery.
- 2,443 production seat/train/XR/backend checks passed, including a moving-head
  knockdown/get-up sequence, opt-out and Classic preservation.
- 12,030 production calibration checks passed: real menu actions, INI load/save,
  signed offsets, bounded edits, restart persistence, separate default restore,
  factory reset, recenter and back actions.
- 27,117 body posture, 142,976 held weapon and 1,536 visibility checks passed.
- Full ARM64 Quest debug APK build passed. Existing shader precision, format and
  integer-pointer cast warnings remain.
- Manager verified and updated the personal Quest 3 device successfully with
  the final shared-menu-setting build. Existing game data and saves were kept.

Visual headset acceptance is pending. Fit camera height/forward/right while
upright and looking down, verify crouch/lean/aiming, test falls and get-up while
physically moving, and confirm driving/train seating. Use SAVE AS MY DEFAULT
once satisfied and report the visible values for a future factory-fit update.
