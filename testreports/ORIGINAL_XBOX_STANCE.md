# Original Xbox backwards lean alignment and standing foot contacts

2026-11-06. Local Windows work; no retail game-data access or reimport required.

The previous solver clamped an unreachable spine endpoint toward the pelvis.
This preserved segment lengths but left the rendered head behind the camera's
physical lean, especially backwards. Its head baseline also came from cached
body stabilization rather than the actual comfort camera anchor. Constant eye
offset adjustment could not correct that changing positional error.

The camera now publishes its anatomical head target before applying user eye
fit. Body IK combines it with physical tracking displacement and solves to that
exact endpoint. Reach and finite-iteration residuals are resolved at the pelvis;
spine lengths, the camera and gameplay root remain unchanged. Stale, excessively
distant targets are rejected. The solved/rendered head never feeds the target.

Stationary feet now retain ground positions and orientations independently of
HMD/body yaw and native idle sway. Two short vertical probes check static ground
and supply contact normals. Native animation provides ankle clearance. Contacts
release for walking/running, airborne states, large turns or root movement,
teleports, missing/steep ground, ownership gaps and leg reach limits. No moving
platform entity reference is stored. Native locomotion gait remains in control
while moving; procedural walking/stepping and moving-platform planting remain
outside this change. Classic, vehicle/train and knockdown ownership is unchanged.
Existing Xbox eye fit values and personal defaults are preserved.

Validation:

- 31,453 body posture checks passed, including backwards/sideways exact head
  endpoints at four rates, fixed spine lengths, stationary contact/orientation
  persistence, lost ground, ownership gaps, teleports, native gait release and
  real production branch transforms with independent camera/ground inputs.
- 22,234 native-head/final-camera checks passed.
- 2,443 seat/train/XR/backend, 12,030 calibration, 142,976 held weapon and 1,536
  body visibility checks passed.
- Full ARM64 Quest debug APK built successfully. Existing shader precision,
  format and integer-pointer cast warnings remain.
- Manager verified and updated the personal Quest 3 device successfully without
  erasing saves, settings or the existing Original Xbox profile.

Headset visual acceptance remains pending. Recenter upright; try forward, side
and backwards leans without editing fit, physical crouch plus lean, physical
backward steps, and stick-driven backwards walking. Check standing feet on flat
ground and slopes, then verify car/train and knockdown/get-up behavior. Persistent
standing contacts release at anatomical reach limits; they do not promise fixed
feet for every roomscale displacement.
