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
  giving 2.5x magnification relative to the lens's apparent angular size.
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
