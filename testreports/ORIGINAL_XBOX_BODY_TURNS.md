# Original Xbox physical turns and body reference resets

2026-11-06, local Windows implementation. No retail game-data access or reimport
was needed. Existing personal fit values, roomscale movement and saves are kept.

The torso/mid references were captured in the HMD-facing visual basis although
the native upper-body matrices had not yet been transformed into that basis.
Resetting Body IK while physically turned 180 degrees therefore captured the
opposite-facing torso as its new neutral. Shoulder position capture could also
retain roomscale follow displacement from the first enable frame.

For Original Xbox, anatomy is now captured in the native entity basis and only
reconstructed in the visual basis when applied. Shoulder capture excludes the
already-applied roomscale follow translation. Classic keeps its capture policy.

The body head target also now subtracts the calibrated eye socket in physical
HMD-facing axes from the actual tracked eye. The offset previously remained in
the original camera-facing axes as the torso turned, which could leave the neck
ahead of the eye at 180 degrees. The correction moves the presentation skeleton,
never the camera or gameplay root. Existing eye-fit and body-yaw values remain.

When looking steeply down/up, body heading uses the projected headset right
axis instead of falling back to entity yaw as forward approaches vertical. This
retains physical facing when inspecting the torso, including past vertical.

Validation:

- 4,033 real production reference/capture checks: independent native/HMD yaws,
  first enable at 0/90/180/270 degrees, subsequent full turns, mid-turn cache
  resets, looking straight down, roomscale follow and Classic preservation.
- 40,316 real production posture checks, including full physical rotations with
  fixed forward/side/height eye fit, reset and continuous cases, and steep head
  pitch on both sides of vertical. The tracked eye remains unchanged.
- 22,335 native-head/camera, 142,976 held weapon, 2,443 seat/train/backend, 12,030
  calibration, 1,966 crouch, 505 roomscale/arrest and 1,536 visibility checks pass.
  Total: 228,140 native checks.
- Full ARM64 Quest debug workspace APK built successfully and Manager verified
  and updated the personal Quest 3 device. Existing compiler warnings remain.

Headset visual acceptance is pending. Test physical 90/180/270/360-degree turns,
look down at each facing, recenter at 180 degrees, toggle Body IK off/on there,
then repeat with roomscale travel, physical/button crouch and aiming. Verify
vehicle/train and knockdown recovery still retain their existing ownership.
Unrelated workspace edits are not included in this change.
