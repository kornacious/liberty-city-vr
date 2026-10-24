# VR traffic pop-in

## Identified cause

Liberty City's VR renderer builds an omnidirectional entity list, but
`CCarCtrl::GenerateOneRandomCar` still used the original mono-camera population
rules. Depending on movement and frame parity, it requested either a roughly
120 m forward spawn or a 40 m side/rear spawn. The final acceptance test allowed
offscreen vehicles within 50 m while accepting visible vehicles only within the
110–130 m ring. A car outside that mono frustum can be visible in the headset,
or become visible immediately after turning the head.

Cleanup had the same mismatch: an offscreen distance threshold could shrink to
50 m, and the old stalled-traffic cleanup could delete eligible vehicles beyond
25 m after five seconds. These population rules are independent of the optional
physics director and vehicle visual budget.

## Change

Patch 21 uses a common distance policy for VR random traffic. Every spawn attempt
uses the distant ring, with final validation at 110–130 m from the player and at
least 90 m from the camera. The generation multiplier cannot shrink these VR
distances below their base values; larger multipliers still scale them.
Direction selection, path validation, collision checks, density limits and
mission ownership rules retain their existing behavior.

Both ordinary offscreen cleanup and stalled-traffic cleanup protect vehicles
within 130 m, scaled by the same policy. Extended-range vehicles retain their
ordinary cleanup extension. The non-VR paths retain the original rules.

Vehicle geometry/LOD callbacks and the experimental directors are unchanged.
This fixes an identified population mismatch; it does not establish that all
reported appearances were caused by population rather than model rendering.

## Validation — 2026-10-21, local execution

- Fresh assembly against the pinned clean re3 source succeeded.
- Full Android `assembleDebug --offline` succeeded (37 tasks).
- New traffic regression passed: near/far boundaries, camera separation,
  generation multipliers 0.5/1/1.5, all 360 directions, and policy integration in
  generation and both cleanup paths.
- Current-source regression run: 42 passed, 2 failed, 8 reference-only fixtures
  excluded. `graphics-menu-layout` fails because an unused `QuestMenuPageColour`
  function warning is treated as an error; `ragdoll-menu` cannot extract that
  function. Both failures match the prior baseline; the tested menu source hash
  is identical. They are not passes.
- Verified the personal Quest 3 device reports `Quest 3`; installing this task's APK
  with `adb install -r` returned `Success`. No game assets or
  saves were transferred or erased.

Build, assembly and regression logs are in the ignored `.cache` directory with
the `traffic-visibility` prefix. The APK is under
`.cache/traffic-visibility-verified/android/app/build/outputs/apk/debug/`.

Headset appearance, population balance and performance have not been measured.
Check busy streets on foot and while driving, looking sideways and behind,
with the optional directors both enabled and disabled. Nearby cars should no
longer be created or removed merely because they are outside the mono frustum.
