# Held weapon jitter during locomotion

## Confirmed source defect

Player body IK runs in `CPed::PreRender`, before the final camera anchor update
in `DoRWStuffStartOfFrame_Horizon`. Its controller refresh converted the current
OpenXR sample using the earlier, pre-movement anchor. The camera then moved to
the post-movement player position. Refreshing raw controller matrices at that
later point did not refresh the already-solved wrists that own the gun transform
when body IK is enabled.

An older compensation advanced unlocked wrist targets between the previous and
current visual body frames. That body frame includes presentation adjustments
and does not necessarily move identically to the camera anchor. Two-hand locked
targets skipped this compensation entirely. Both cases could disagree with the
frame used by the eyes, through different paths. This defect affects the body IK
weapon ownership path; it is not evidence of a gun collision or force instability.

The uncompensated translation error is approximately movement speed multiplied
by simulation step duration. At an illustrative 6 m/s, a full step is 20 cm at
30 FPS, 10 cm at 60 FPS and 6.7 cm at 90 FPS. These are calculated examples,
not measurements from the headset. Actual speed, timer scaling/clipping and
existing compensation affect the result. Changing frame times changes the error;
standing still removes its translational component. Turning can also introduce
angular differences. This explains why locomotion and unstable FPS expose it.

## Repair

Before either on-foot skeleton solver runs, publish the current player camera
anchor. Use the preparation phase (`postPhysics=false`) so this extra call does
not repeat post-physics seat recenter or RenderWare camera synchronization.
The existing controller refresh then converts both hands against that anchor.
Skip body-carrier advancement for these fresh on-foot targets in both skeleton
paths, preserving the rigid calibration between wrist and gun. Vehicle neck
anchors can depend on the body solve and retain their existing sequence.

This is an update-order repair, not additional smoothing: held weapons should
respond directly to tracking without a new filter delay or altered gun forces.

## Validation and limits

The native host regression extracts the actual body IK entry point, floating
pose conversion, carrier blocks and rigid weapon reconstruction. The old source
fails because the solver sees an obsolete anchor. The fixed source passes
51,769 checks across nine rates (15–144 FPS), both skeleton paths, locked and
unlocked hand modes, alternating frame durations, acceleration, running,
stopping, turning, teleports and tracking loss. Inactive IK, cutscene and ragdoll
paths must not refresh the anchor. Skeleton storage is adapted; the full arm
solver and two-hand constraint are not simulated by this test.

An independent negative check kept the new anchor preparation but restored
on-foot carrier advancement; it failed the exactly-once movement assertion.
The assembled current-source run passed all 42 supported suites (eight
reference-only fixtures excluded), and the Android debug APK built successfully.
ADB reported no connected devices, so deployment and visual validation were
unavailable in this session. No original game data was needed for these checks.

The headset follow-up is to draw a pistol and rifle, run/strafe/turn with one
and both hands, and compare body IK on/off at steady and fluctuating frame
rates. Check grip engagement/release and crouch transitions too. Automated
results establish the timing defect and repair contract, not perceptual absence
of all possible jitter. Tracking noise, rendering/compositor pacing and vehicle
presentation need separate diagnosis if symptoms remain.
