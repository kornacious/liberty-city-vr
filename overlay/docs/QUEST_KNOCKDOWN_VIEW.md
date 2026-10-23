# Knockdown view

On foot, falling, staggering, diving away and getting up temporarily use the
animated player's head as the camera target. Tracked hands, held weapons,
lasers, holster highlights and throwable previews hide through recovery.
Held weapons return to their holsters without losing inventory or ammunition;
draw them again after getting up. Holding grip through the fall cannot redraw
a gun automatically: release grip and grab the holster again.
Native and tracked attack input is suspended during incapacitation.
The animation's own body and arms remain visible, including with Body IK off.

In **Settings > Locomotion > Knockdown View**, choose:

- **Level Horizon** (default): follows head position and gently turns toward
  torso facing, without imposed pitch or roll.
- **Immersive Tilt**: adds up to 25 degrees of pitch and 20 degrees of roll,
  each changing at no more than 30 degrees per second.

Both modes retain physical headset tracking. During the fall, imposed yaw is
limited to 60 degrees from the entry direction and changes at no more than 45 degrees per
second. Heading recovery begins during get-up and follows voluntary turns
immediately while easing out the remaining imposed rotation. Hands and body IK
resume together once head height and tilt have recovered; they never wait for
the final yaw correction. Position smoothing uses a 100 ms time constant during
the animation and 180 ms during recovery. Collision checks protect the camera anchor from
the floor and surfaces between it and the animated head; physical leaning
can still move the actual eyes beyond that anchor.

The option persists as `[VR] KnockdownCameraTilt=0` or `1` in
`vr_settings.ini`. Death, cutscenes and vehicle cameras retain their existing
ownership. Changing player, teleporting or leaving the gameplay context clears
the knockdown offsets.

Run `tools/tests/test-vr-knockdown.ps1` for production smoothing, state and
tracking-reference checks. In-headset checks should cover a car impact,
sideways fall, face-up/down fall, get-up, repeated impacts, both view options,
and transitions into death or a vehicle. Comfort and visual clipping require
an actual play test.
