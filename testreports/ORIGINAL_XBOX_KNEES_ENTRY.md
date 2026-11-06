# Original Xbox knee direction and animation-led car entry

2026-11-07, local Windows implementation. No retail data access or conversion
required. Existing fit values, roomscale root synchronization and saves remain.

The leg solver projected its knee bend direction from the native animation even
after moving the hip/pelvis. With nearly straight native legs or shifted contacts,
that projection could reverse or swing sideways during physical turns. Tracked
posture now constructs the knee plane from anatomical forward projected across
the current hip-to-foot direction, with an up-axis fallback for a horizontal leg.
Native dimensions and reachable foot contacts remain unchanged. The non-tracked
fallback retains its original knee-plane source.

Original Xbox car entry previously used the regular level on-foot camera basis,
which could disagree with the authored head while opening a door or settling
into the seat. OPEN_DOOR, CARJACK, ENTER_CAR and STEAL_CAR now use native head
position/orientation until driving takes over. Entry establishes a fresh physical
heading once, so starting while turned 180 degrees does not look backwards down
the animation. Position tracking is locked throughout these entry phases, while
physical looking remains additive. Heading is not re-latched every animation
phase or frame. Driving and cancelled entry release the lock through the existing
tracking-origin handoff. Classic, train boarding and exit behavior are unchanged.

Validation:

- 47,012 production posture checks passed. New coverage includes reversed and
  nearly straight native knee poles, full physical yaw, forward/backward contacts,
  anatomical forward knee bending, preserved limb lengths and head/foot targets.
- 22,346 native-head/final-camera checks passed, including all four car-entry
  phases and driving/on-foot basis handoff.
- 2,463 seat/train/backend checks passed, including 180-degree entry heading,
  phase-to-phase latch stability, entry position lock, driving release, cancel
  release and a fresh heading for later entry.
- 5,761 reference, 142,976 held weapon, 12,030 calibration, 1,966 crouch, 505
  roomscale/arrest and 1,536 visibility checks passed. Total: 236,595 native checks.
- Full ARM64 Quest debug APK built successfully and Manager verified/updated the
  personal Quest 3 device. Existing compiler warnings remain.

Visual headset acceptance is pending. Inspect knees while turning physically,
walking roomscale and button/physical crouching. Enter cars facing forward and
while physically turned around; inspect door opening, carjacking, entry and
settling, then confirm normal driving and cancellation/exit behavior.
