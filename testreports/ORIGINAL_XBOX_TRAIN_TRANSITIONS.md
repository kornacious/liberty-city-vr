# Original Xbox train transition continuity

Date: 2026-11-07. Implemented and validated locally on Windows. No original
game data was required for implementation or transferred.

The native `PedSetOutTrainCB` callback changed Claude's heading after the exit
animation, undoing the outward-facing VR exit view. A new Android VR patch calls
`FinishQuestXboxTrainExit` after native platform placement and passenger removal.
For the Xbox first-person player, it derives the outward direction from the
platform position relative to the carriage, assigns Claude's current/destination
heading, and seeds the ordinary follow-camera position, direction and yaw.
Pending camera rotation is cleared; physical headset calibration is preserved.
Classic, NPC and cinematic train exits retain their existing handling.

Xbox boarding now follows the native head's authored turn with a level horizon.
The riding camera starts in its final carriage-relative outward direction,
without the second 300 ms turn or a new physical yaw latch. Classic retains its
existing boarding blend.

During Xbox entry/exit states, the native animation owns the connected arms and
hands. Tracked hand rendering is suppressed through the shared hand policy and
returns after the transition. This avoids displaying free-floating VR hands when
the skinned transition deliberately bypasses arm IK.

Validation:

- `tools/tests/test-train-transitions.ps1`: 22,746 head/camera checks and 3,467
  seat/train/XR/backend checks passed using extracted production code. Includes
  native exit hook placement, both carriage sides across full yaw rotation,
  player/camera/movement heading agreement, single boarding turn, physical yaw
  continuity, native/tracked hand ownership and Classic/NPC/inactive VR exclusions.
- Remaining weapon, visibility, body reference/posture, roomscale, calibration and
  crouch regressions passed: 238,085 native checks in total.
- Full Quest APK build succeeded, including application of patch 77. Existing
  unrelated shader precision and screenshot format warnings remain.
- Update installation succeeded on the personal Quest 3 device, preserving game
  data and saves. In-headset visual verification awaits user testing.
