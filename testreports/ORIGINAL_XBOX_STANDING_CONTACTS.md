# Original Xbox standing foot targets after physical turns

2026-11-07, local Windows implementation. No retail data access or reimport.

Anatomical stance centering was applied only during button crouch. Standing
foot candidates still came from the original entity-centered animation even
after the pelvis had been corrected beneath the tracked head. New contacts after
a physical half-turn could therefore settle ahead of the corrected body.

Stationary grounded standing now uses the same anatomical stance origin as
crouch before contact retention and inferred stepping. Native joystick walking/
running retains its animated candidates. Existing contact retention and ground
checks remain active; this does not force both planted feet to slide continuously
with every head movement. Knee direction, car-entry camera, gameplay roomscale,
personal fit, Classic and vehicle/train handling are unchanged.

Validation:

- 47,098 production posture checks passed. Stance midpoint checks now cover both
  standing and crouching at 24 physical headings. A continuous half-turn retains
  contacts across frames while checking anatomical origin proximity and exact
  tracked-head placement.
- 5,761 reference, 22,346 head/camera, 142,976 held weapon, 2,463 seat/train,
  12,030 fit, 1,966 crouch, 505 roomscale/arrest and 1,536 visibility checks passed.
  Total: 236,681 native checks.
- Full ARM64 Quest debug APK built successfully and Manager verified/updated
  the personal Quest 3 device. Existing compiler warnings remain; saves, game
  data and fit values are preserved.

Headset visual acceptance is pending. Recenter upright, turn slowly through
180 degrees, pause and inspect foot landing positions, then repeat with roomscale
steps and button crouch. Keep fit values unchanged for the first comparison.
