# Original Xbox tracked crouch and lean

2026-11-06, local Windows execution. Retail model metadata was inspected locally;
no game assets were downloaded, transferred, reimported or included in the change.

The previous on-foot solver carried the entire body with horizontal headset
displacement, restored upright torso orientations and solved controller arms.
That left no expressive physical lean and little procedural crouch handling.

The new Original Xbox skinned-player layer reserves up to 28 cm of roomscale
displacement for leaning. It shares that movement between pelvis and spine,
uses the recentered headset's vertical displacement for physical crouch, solves
the spine with preserved segment lengths, and bends each leg toward its original
animated foot contact. Complete descendant branches move with each joint, so
shoulders follow the torso before the existing arm/weapon solve. Foot orientation
is restored from the native pose. This retains per-frame animation contacts;
persistent terrain-aware foot planting remains future work.

Physical and authored crouch lowering use the greater reduction rather than
adding both. Enabling IK during a button crouch retains native crouch height
until a standing reference is available. Additional lowering is capped at 65 cm
and by leg length. The solve
is stateless, independent of frame duration, bounds extreme displacement and
rejects invalid/unsupported anatomy. Recenter while upright for a neutral pose.

The camera, native-head snapshot and gameplay root/collision remain authoritative
and unchanged. The feature follows the existing Body IK toggle. Classic,
vehicles (including train passengers), jumps, transitions, cutscenes and
knockdown/ragdoll ownership retain existing behavior.

Validation:

- 27,117 posture checks passed, including real production branch transforms,
  shoulder propagation, segment lengths, foot contacts/orientations, combined
  crouching, extreme/invalid input, eight sample rates, roomscale carry, world
  yaw/translation invariance, excluded owners and camera/root isolation.
- 22,231 native-head/final-camera checks passed.
- 142,976 held weapon checks passed.
- 2,074 seat/train/XR/backend checks passed.
- 1,536 body visibility checks passed.
- Local converted Xbox player metadata confirms the required waist/mid/torso/head
  and bilateral leg chains. No asset bytes or extracted metadata are committed.
- Full ARM64 Quest debug APK build passed, including the final crouch-start
  calibration guard. Existing shader precision, format and integer-pointer cast
  warnings remain. An initial engine macro name collision was corrected.
- Manager verified and updated the personal Quest 3 device successfully. Retail
  data, saves and the existing Original Xbox profile were preserved.

Visual acceptance requires headset testing: upright/recenter, slow shallow/deep
physical crouching, forward/side leans, walking while crouched, physical plus
button crouch, aiming/reaching, return to neutral, and existing car/train/fall
handling. Headset/hand-only tracking estimates the lower body; it cannot measure
the player's actual hips, knees or feet.
