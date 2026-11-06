# Original Xbox train window facing and passenger head alignment

2026-11-07, local Windows implementation. No retail data access or reimport
required. Existing fit values, on-foot posture and roomscale behavior are kept.

The train path already computed a 300 ms turn toward the boarding-side windows,
but the final Xbox camera override discarded it in favor of the player entity's
forward axis. That axis can differ from the skinned passenger's facing. The
standing comfort latch also retained the first passenger head position while
native animation continued into the standing/idle pose.

Xbox boarding completion now establishes a fresh physical heading once, after
post-physics lineup. The existing window turn remains authoritative through the
final Xbox basis override. While riding, camera position follows the live native
head and calibrated eye offset in the outward-facing carriage basis, rather than
a frozen first-passenger pose. This prevents settling/idle animation from leaving
the view behind the body. Headset looking remains additive; physical position
uses the existing train lock. Classic, train exit, car entry/driving and the
accepted on-foot body/foot placement are unchanged.

Validation:

- 22,527 native-head/final-camera checks passed. New coverage verifies outward
  heading independent of entity forward, live settling/idle head position, and
  eye-fit alignment while the carriage translates and turns.
- 2,588 production seat/train/backend checks passed. Xbox completion captures
  physical heading after lineup, faces outside without physical turning, retains
  carriage-relative direction in motion, avoids repeated heading resets while
  looking around, and releases passenger tracking policy on exit. Existing
  Classic, car-entry, seat and train behaviors also pass.
- 5,761 reference, 47,098 posture, 505 roomscale/arrest, 142,976 held weapon,
  1,536 visibility, 12,030 calibration and 1,966 crouch checks passed.
  Total: 236,987 native checks.
- Full ARM64 Quest debug APK built successfully and Manager verified/updated the
  personal Quest 3 device. Existing compiler warnings remain; assets, saves and
  personal calibration are preserved.

Headset visual acceptance is pending. Board from each available door while
holding a natural forward physical facing, wait for the window turn, inspect
neck/shoulder alignment, then look around while stationary and while the train
moves. Check train exit and a representative on-foot/car transition afterwards.
