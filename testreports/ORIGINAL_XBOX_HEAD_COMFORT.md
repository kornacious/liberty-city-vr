# Original Xbox camera comfort correction

Date: 2026-11-06. Execution: local Windows; no retail game data transfer or reimport.

The direct native head anchor submitted animated bone orientation to the camera,
including its resting tilt and idle/walk/run nods. It also bypassed WALKING HEAD
BOB: the setting getter previously had no camera consumer.

The Original Xbox first-person path now derives orientation from the locomotion
or vehicle frame, leaving head rotation to tracked headset input. On foot the
horizon uses world up. Driving retains carrier orientation and the bike horizon
lock policy; head-relative movement retains gameplay camera heading.

With walking head bob disabled, routine idle/walk/run and standing train camera
positions retain a bone-derived entity-local reference. Root/carriage motion
remains immediate. The reference commits after simulation and resets when the
player, model, carrier or toggle changes. Crouch, jump, vehicle transitions and
knockdowns retain live native head positions; recovery preserves the standing
reference. Enabling bob restores live positional animation immediately. Classic
and third-person camera paths remain unchanged.

Validation:

- 22,231 native head and production final-camera checks passed, including toggle
  changes, animated pitch/roll, moving roots, standing train boarding, transitions,
  knockdowns, crouch/jump recovery, carrier orientation and Classic bypass.
- 142,976 held weapon frame checks passed.
- 2,074 vehicle seat/train/XR/backend checks passed.
- 1,536 body visibility checks passed.
- Full Quest debug APK build succeeded. Existing shader precision and time format
  warnings remain.
- Manager verified and updated the personal Quest 3 device successfully, preserving
  retail assets and saves. No profile rebuild was required.

Visual headset acceptance is pending: verify level idle view, walking/sprinting
with the overlay toggle off and on, car entry/knockdown recovery, and immersive
train boarding/standing while the carriage moves. Host tests and installation
do not establish that visual acceptance.
