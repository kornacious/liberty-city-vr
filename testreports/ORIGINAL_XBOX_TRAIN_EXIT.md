# Original Xbox train exit camera

Date: 2026-11-07. Local Windows implementation and validation; no retail game
data was required or transferred.

The train exit state disables the riding camera before the exit animation ends.
The Xbox head override consequently selected the ordinary player/game camera
heading, which can point into the carriage while Claude faces outside.

The Xbox override now derives the outward heading from the live head's occupied
side of the carriage and the carriage right axis throughout `PED_EXIT_TRAIN`.
The native head remains the position anchor and eye calibration uses that outward
basis. Passenger heading calibration is preserved, without another recenter.
The Xbox position lock persists even when occupancy clears during exit, then
releases when exit finishes. Classic tracking and normal on-foot heading retain
their existing paths.

Validation:

- 237,138 native regression checks passed across head attachment, seat/train/XR,
  weapons, visibility, body reference/posture, roomscale, calibration and crouch.
- New checks cover both carriage sides over a full yaw rotation with an inward
  gameplay heading, outward eye fit, exit translation lock after occupancy clears,
  heading calibration continuity, on-foot restoration and Classic policy.
- Full Quest APK build succeeded; update installation succeeded on the personal
  Quest 3 device. Existing game data and saves were preserved.
- Visual train-exit verification in the headset remains pending user testing.
