# Original Xbox gameplay roomscale, stepping, fixed crouch and arrest view

2026-11-06, local Windows implementation. No original game data access, reimport
or transfer was required. Existing profile, personal fit settings and saves remain.

The previous roomscale layer moved the presentation skeleton while leaving the
player entity at its original position. NPC targeting and gameplay collision
therefore disagreed with the visible body. Standing contacts also lacked an
alternating stepping phase. Button crouch followed authored horizontal head
motion and continued adding physical height changes; arrest states did not own
the immersive knockdown camera and could enter the director/theater view.

Changes:

- Before simulation, collision-check physical horizontal displacement in 4 cm
  substeps and move the actual Xbox player entity on static ground. Keep a 10 cm
  head-lean allowance, cap a transfer at 25 cm, stop at obstacles/missing ground,
  update collision sectors and animated bones, and preserve native velocity.
  Consume accepted horizontal displacement in the tracking origin so the eye
  does not move twice. Duplicate samples in a game frame are ignored.
- Infer alternating roomscale steps from foot/root displacement. Each 220 ms
  swing has up to 6.5 cm clearance and retains the opposite support contact.
  Same-frame solver calls cannot advance a step twice. Native joystick gait
  retains ownership while walking/running; moving-platform steps are not added.
- Xbox button crouch now toggles a fixed 45 cm vertical lowering of the standing
  head reference. It does not inherit authored horizontal head displacement.
  Other input does not cancel it, and its native timer is refreshed. A backend
  height-only tracking lock ignores physical crouch/stand height until release,
  while preserving horizontal movement and tracked looking.
- On-foot PED_ARRESTED, PED_HANDS_UP and WBSTATE_BUSTED use the same native-head
  camera options as knockdown/get-up. Busted letterboxing cannot force the
  director/theater camera, while formal cutscene ownership remains unchanged.
- Classic camera/crouch/roomscale behavior and existing vehicle/train ownership
  are preserved. Body IK controls inferred body steps, not gameplay root sync.

Validation:

- 505 production roomscale/root/backend/crouch/arrest checks: root and eye
  continuity at multiple basis yaws, collision substeps, ground loss, duplicate
  frame calls, owner/input/paused gates, height lock/release, arrest/busted and
  theater/formal-cutscene policy.
- 37,980 body posture checks, including alternating step phases at four rates
  and visible lift/support using both real production leg chains.
- 22,335 native-head/final-camera checks, including fixed crouch horizontal
  stability across changing authored head poses and standing recovery.
- 1,966 crouch checks covering Classic behavior and the Xbox toggle/timer.
- 142,976 held weapon, 2,443 seat/train/XR/backend, 12,030 calibration and 1,536
  visibility checks passed. Total: 221,771 native checks.
- Manager regressions: 79/79 passed, including the updated exported backend test.
- Full ARM64 Quest debug APK built and updated on the verified personal Quest 3
  device. Existing shader precision, format and integer-pointer warnings remain.
  An initial missing crouch API declaration was corrected before successful builds.

Visual headset acceptance remains pending. Verify NPC melee/gun targeting after
walking physically, capsule behavior near walls/steps, alternating foot motion,
button crouch while physically crouching/standing, toggle recovery, and arrest
alongside car/shotgun knockdowns. Physical steps are inferred from headset/root
motion rather than measured feet. Static-ground root transfers and footsteps do
not implement moving-platform locomotion.
