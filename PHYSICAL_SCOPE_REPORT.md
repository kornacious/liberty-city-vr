# Physical scope rendering — 2026-10-20

`3D PHYSICAL` now renders a magnified scene onto the rifle's rear optic while
the surrounding headset view keeps its normal field of view. `2D OVERLAY`
retains its existing 2.5x main-view zoom, and `OFF` requests no optic pass.

The previous implementation drew an opaque lens backing and a reticle, and
applied zoom to both main eye projections even in physical mode. It did not
produce a separate image through the lens.

## Rendering

- A 512 x 512 offscreen target replays this frame's world draw buffers,
  materials, transforms and skinning offsets. Atomic bounding spheres are
  tested against the optic frustum before replay. Held weapons, hands,
  lasers and wrist UI are excluded.
- Both multiview layers receive the same optic projection; layer zero is
  copied into the sampled lens raster. The resulting mono optical image is
  placed on world geometry and therefore appears at the lens position in
  each eye. This avoids a second stereo scene submission.
- The optical camera follows the authoritative firing ray. Its field of
  view accounts for the lens size and distance to the nearest aligned eye,
  giving the selected magnification (initially 2.5x) relative to the lens's
  apparent angular size.
- A cached geometry fit locates a compact rear-facing cap above the bore,
  preferring the rear of equal-height caps. The fit uses the actual triangle
  plane, allowing a small angle between the cap and calibrated firing ray.
  The existing lens placement is a fallback when a replacement mesh has no
  suitable cap. An inset elliptical mask covers the original opaque cap.
- The original scene's colour/depth attachments are stored only on frames
  requesting a physical optic. After rendering the scope, a compatible pass
  loads them and draws the lens and reticle with depth testing. This uses the
  current image, without introducing a previous-frame optic texture.
- A separate per-frame scene UBO protects the main-eye projections from
  scope-camera changes. Graphics descriptors, viewport and scissor are
  rebound after the multiview pass boundary. Texture sampling is ordered
  after the render/copy writes, and touched game render state is restored.

The graphics-state rebind follows the [Vulkan multiview state rules](https://docs.vulkan.org/refpages/latest/refpages/source/VkRenderPassMultiviewCreateInfo.html).

## Validation

`tools/tests/test-physical-scope.ps1` compiles production scope projection and
cap-fitting headers, plus extracted production draw capture/replay and zoom
selection functions. Generated geometry exercises cap selection and slightly
angled caps. Additional checks cover room-scale transforms, rifle roll,
frustum rejection, copied draw state, skinning offsets, hand/UI exclusions,
deferred lens geometry, and switching between physical/overlay/off modes.

The final current-source regression run passed **41 suites with zero
failures**. Eight reference-only fixtures remained excluded by the runner;
they are not counted as passes. The focused scope fixture also passed after
the final culling and cap-plane refinements.

The arm64 debug APK compiled successfully with the local Android toolchain.
It was installed successfully with `adb -s REDACTED_QUEST_SERIAL install -r` after
querying and verifying the device model as `Quest 3`. No uninstall, game-data
transfer or save clearing was performed.

Build and test artifacts are local under `.cache/physical-scope-final/`,
`.cache/physical-scope-final-regressions/`, and
`.cache/physical-scope-tests-releasecheck/`.

## Remaining headset checks

Lens placement across the user's actual model sets, optical appearance,
alignment comfort, MSAA/SGSR behaviour and frame cost need an in-headset aiming
test. Installation and compilation do not establish those results. The scope
shares the main scene's streamed objects and draw distance; it does not request
additional distant world streaming. Its extra render work is limited to active
physical aiming, but a headset performance measurement is still needed.

## Right-thumbstick zoom — 2026-10-24

While aiming with `3D PHYSICAL`, hold the right controller's thumbstick up to
zoom in or down to zoom out. Magnification changes continuously from **1x to
8x**, starts at **2.5x**, and remains at the selected value when the rifle is
lowered or the scope mode changes during the current game session. It is not
written to disk.

The vertical stick has a 0.25 deadzone and proportional speed up to two zoom
units per second. The update uses frame elapsed time, capped at 100 ms to avoid
a jump after a stall. It runs after scope activation is updated, is blocked
by menus/unavailable gameplay/knockdown, and consumes only the vertical legacy
camera axis while physical aiming is active. Horizontal stick turning and
the existing `2D OVERLAY` zoom behaviour are preserved. The lens projection
accepts the wider field of view needed by a large, close optic at 1x.

The physical-scope fixture additionally exercises the production input handler
and zoom calculation, covering up/down direction, proportional input,
deadzone, limits, stalled/invalid elapsed time, 30/72/90/120 FPS equivalence,
input consumption, blocked input and retention outside physical aiming.

The focused scope tests and full arm64 debug APK build passed. The broader
current-source run passed 47 suites and failed six: `cheat-menu`,
`graphics-menu-layout`, `quest-melee`, `held-weapon-frame`, `ragdoll-menu`, and
`vehicle-seat-recenter`. All six failures were reproduced against an unchanged
HEAD snapshot with the same missing fixture declarations, unused-function
warning, extraction failure or outdated anchor signature. They are pre-existing
fixture failures and were not counted as passes. Eight reference-only suites
remain excluded. Logs are under `.cache/scope-zoom-validation-20261024/` and
`.cache/scope-zoom-baseline-checks-20261024/`.

The resulting APK at
`.cache/scope-zoom-apk-20261024/android/app/build/outputs/apk/debug/app-debug.apk`
was installed successfully with `install -r` on verified Quest 3 serial
`personal Quest 3 device`, preserving app data. The user confirmed the previous physical
scope's appearance and behaviour; the new thumbstick speed and range still
need their in-headset check.
