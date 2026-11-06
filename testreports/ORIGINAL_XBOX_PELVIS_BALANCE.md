# Original Xbox pelvis and button-crouch stance balance

2026-11-07, local Windows implementation. No retail data access or reimport
required. Existing camera fit, gameplay roomscale movement and saves are kept.

The previous solver constrained the head endpoint but let the original animated
pelvis and spine geometry choose the rest of the pose. Physical turns could
therefore leave the belly forward even when the head/facing were correct. Button
crouch retained the authored pose's lateral/forward stance displacement.

Xbox upper-body calibration now also captures pelvis-to-head, pelvis-to-foot
center and neutral spine joint offsets in anatomical body space. The solve
reconstructs those vectors in the current physical body heading, anchors the
pelvis beneath the tracked head, and retains the physical lean contribution.
Spine lengths and initialization come from the anatomy reference rather than
displaced/compressed pose samples. Existing head reach handling remains active,
with a lower pelvis-height bound derived from the foot plane and leg length.

During button crouch, native foot contact candidates are centered around the
anatomical stance before contact retention/stepping and leg IK. The authored
forward/right displacement no longer determines where the feet should settle.
Knee bending and ground contacts remain active. The native crouch pose remains
the source for leg dimensions and knee bend direction, while the balanced spine
reference removes its unwanted whole-body displacement.

Only Original Xbox on-foot posture consumes this reference. Classic, vehicle/
train ownership, knockdown/arrest cameras, personal fit settings and gameplay
roomscale synchronization are unchanged. Missing reference data retains the
existing fallback path; IK reset clears the reference with the other anatomy.

Validation:

- 5,761 production capture/reference checks passed, including anatomical pelvis,
  head and stance capture independent of native/HMD heading and reset angle.
- 40,532 production posture checks passed, including 24 physical turn headings
  with pelvis/hip placement beneath the head, and forward-bent crouch poses with
  exaggerated right/forward displacement. Crouched foot center, neutral spine
  dimensions and exact head placement are checked independently.
- 22,335 head/camera, 142,976 held weapon, 2,443 seat/train/backend, 12,030 fit,
  1,966 crouch, 505 roomscale/arrest and 1,536 visibility checks passed.
  Total: 230,084 native checks.
- Full ARM64 Quest debug APK built successfully and Manager verified/updated the
  personal Quest 3 device. Existing compiler warnings remain.

Headset visual acceptance is pending. Check belly/pelvis position while turning
through 360 degrees, looking down, recentering and toggling IK. Repeat button
crouch facing forward/sideways/backwards, then check physical leaning/crouching,
alternating roomscale steps, native walking and car/train/knockdown transitions.
