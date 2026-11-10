# Quest depth-rejection control

Stage 1 validates depth rejection and the LRZ reporting path with generated
geometry, independently of the game's materials, culling and shader variants.
It is a separate debug APK, `com.libertyvr.depthprobe`, and loads no game data.
It does not require OpenXR, a save or controller input. Game pipelines are not
modified. The diagnostic remains installed; its process is stopped by the runner.

The offscreen render pass uses 1024x1024 per view, two-view Vulkan multiview,
1x MSAA, RGBA8 colour and D32 depth. Depth is cleared to 1 every frame and uses
LESS_OR_EQUAL. Blending, stencil, face culling and alpha-to-coverage are disabled.
The compiled fragment shader contains no discard, depth output, atomics, storage
writes or sample mask. This is a controlled workload, not a reproduction of the
game's complete attachment/shader configuration or performance.

Every case submits the same 17 draws and 8,194 triangles, counted once for the
multiview command stream. The first draw is a two-triangle foreground quad:

| Case | Foreground | Foreground depth writes | Expected result |
|---|---|---|---|
| open | Clipped offscreen | On | Blue background |
| occluded | Covers both views at depth 0.1 | On | Red foreground; background rejected |
| no_depth_write | Covers both views at depth 0.1 | Off | Blue background overwrites foreground |

The 16 background layers are identical 16x16 grids, drawn far to near at depths
0.85 through 0.55. Their shader uses nonconstant arithmetic to make execution
measurable. Draw labels distinguish foreground from background in RenderDoc.
Timestamp queries bracket the entire render pass from outside, before readback.
Readback checks nine interior pixels per eye. Four warm-up and eight measured
frames precede the optional captured frame; capture time is not in that median.

## Build and run locally

Requires the existing Android SDK, NDK 27.2.12479018, build-tools 34.0.0,
android-35 platform, JDK 21, CMake and Ninja. Outputs and the local diagnostic
signing key must stay outside version control. Reuse the output directory/key
for updates; the runner never uninstalls an app to resolve a signing mismatch.

```powershell
./tools/depth-rejection-probe/build.ps1 -Sdk SDK_PATH -JavaHome JDK_PATH -OutDir .cache/depth-control -CMake CMAKE_PATH -Ninja NINJA_PATH
python tools/depth-rejection-probe/run.py --sdk SDK_PATH --apk .cache/depth-control/depth-control.apk --output .cache/depth-control-results/normal
```

The runner privately discovers a single authorized Quest 3, verifies its model,
installs only this diagnostic and stops only its package. All ADB output and
device identifiers remain local. Zero/multiple eligible targets stop the run.

For capture, use Meta's Python environment and add
`--renderdoc "C:/Program Files/RenderDocForMetaQuest"` with a different output
directory. Disconnect the GUI's Quest replay context first. The runner launches
only this synthetic APK through the installed Meta tooling. It does not root,
reboot or uninstall anything. The app passively obtains the injected API and
captures one complete submission per case, restoring the previous capture path
template. It still runs and writes timestamps when RenderDoc is absent.

Replay the three exported `.rdc` files on the personal Quest 3 device in the same
draw-call profiling mode. Collect LRZ state, Fragments Shaded and Clocks using
the existing renderdoc-meta workflow. Sum **background** fragments separately
from foreground. Preserve trace health, actual sample counts and variance.

LRZ write-enable state is a boolean, not a count of actual depth writes.
`LRZ Prims Killed == 0` alone does not establish ineffective depth rejection.
Do not compare injected GPU medians directly with unhooked medians: clock policy
and capture overhead differ. Short sequential samples are controls, not a robust
performance benchmark. See [measured stage-1 results](../../testreports/DEPTH_REJECTION_CONTROL.md).

## Blend/write/discard transition control

Add `--transition` to run the eight-case follow-up suite, using a separate
output directory. Add `--renderdoc` as above to export eight synthetic captures.
The runner sets an app-private suite flag and verifies the returned suite;
running without `--transition` restores the original three-case baseline.

This suite uses D24_UNORM_S8_UINT with depth clear 1, depth store DONT_CARE,
stencil load/store DONT_CARE and stencil testing disabled, matching those
aspects of the game's captured scene pass. Resolution, multiview, sampling,
depth comparison and generated geometry stay fixed across cases.

Each case submits 19 draws and 8,198 triangles in this order:

1. An opaque seed quad at depth 0.95, with depth writes enabled.
2. A trigger quad at depth 0.9, independently varying blending, depth writes
   and fragment discard: all eight on/off combinations.
3. An opaque foreground quad at depth 0.1, using the original no-discard shader
   and depth-writing pipeline again.
4. The same 16 background grids as the baseline, behind the foreground.

Trigger alpha is 0.75. Blending uses SRC_ALPHA/ONE_MINUS_SRC_ALPHA. The discard
variant kills half the pixels in a checker pattern. The other variant is
compiled from the same source with discard physically absent from SPIR-V;
this is not merely a runtime-disabled branch. Every case should end with red
foreground in both eyes. Pixel checks and timestamps follow the baseline
procedure, with the optional captured frame excluded from measured samples.

Capture names encode the trigger: `transition_b{blend}_w{write}_k{discard}`.
Compare LRZ write-enable state on the labeled seed, trigger, subsequent opaque
quad and background separately. A zero background fragment count establishes
depth rejection in this control, but does not identify which hardware depth
stage rejected it. In particular, write-disable state is not equivalent to
disabled depth testing.

See [transition measurements](../../testreports/DEPTH_STATE_TRANSITION_CONTROL.md)
for the factorial result and its limits. This control does not reproduce the
game's textures, meshes, alpha splits, draw ordering or complete shader workload.
