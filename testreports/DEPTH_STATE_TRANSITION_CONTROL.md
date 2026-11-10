# Quest LRZ state transition control

Local investigation on 2026-11-10, using the personal Quest 3 device and
RenderDoc Meta Fork v207. This extends the separate generated-geometry
[depth-rejection diagnostic](../tools/depth-rejection-probe/README.md).
The game renderer, game assets and saves were not changed.

## Question and control

The enabled Opaque World Fast Path capture first loses LRZ write-enable state
at event 137, a blended, depth-writing, alpha-tested draw. Later opaque draws
retain the same write-disabled state. The capture alone could not isolate
which aspect caused the transition; see the
[game capture investigation](OPAQUE_ALPHA_PIPELINE_INVESTIGATION.md).

The new `--transition` suite varies trigger blending, depth writes and shader
discard independently in eight cases. Before the trigger, an opaque seed quad
writes depth. After it, an opaque foreground quad uses the same original
no-discard, nonblended, depth-writing pipeline. Sixteen background grids follow.
Each case has 19 draws and 8,198 submitted triangles, counted once for multiview.

All cases use 1024x1024 per eye, two-view multiview, 1x MSAA, RGBA8 colour,
D24_UNORM_S8_UINT depth/stencil, depth clear 1 and LESS_OR_EQUAL. Depth store is
DONT_CARE; stencil load/store are DONT_CARE and stencil testing is disabled.
These match the relevant attachment aspects of the game capture, while
textures, meshes, alpha splitting and the complete scene workload remain absent.
Trigger blending uses SRC_ALPHA/ONE_MINUS_SRC_ALPHA with surviving alpha 0.75.
The discard shader kills a checker pattern; the paired no-discard shader
physically contains no OpKill instruction.

The recorded Vulkan command stream was independently checked for all eight
captures: 19 draws/8,198 triangles, trigger blend/depth-write flags, depth-test
comparison, disabled stencil and the selected fragment module's OpKill presence.
Recorded draw/instance counts were matched in order to replay counter records.

## Measured transition

Raw LRZ state `3` means test and write enabled; `1` means test enabled and
write disabled. These are enable flags, not counts of depth writes. The seed
reports `3` in every case. Every one of the 16 background draws agrees with
the subsequent opaque quad's state.

| Trigger blend | Trigger depth writes | Trigger discard | Trigger LRZ | Subsequent opaque/background LRZ |
|---|---|---|---:|---:|
| Off | Off | Off | 1 | 3 |
| Off | On | Off | 3 | 3 |
| On | Off | Off | 1 | 3 |
| On | On | Off | 1 | 1 |
| Off | Off | On | 1 | 3 |
| Off | On | On | 1 | 3 |
| On | Off | On | 1 | 3 |
| On | On | On | 1 | 1 |

For this workload and device/driver, **blending combined with depth writes is
sufficient to reproduce the persistent LRZ write-disable transition**, even
when the trigger shader contains no discard. Discard without blending can
disable writes on the trigger itself, but the subsequent opaque draw recovers.
Blending without depth writes also recovers. This provides a controlled
reproduction of the state sequence observed at game event 137; it does not
establish every possible condition that can affect LRZ on this GPU.

Background Fragments Shaded sums to **zero in all eight cases**, including the
two that retain state `1`. LRZ Prims Killed is also zero throughout. Therefore
write-disable state does not establish that hidden surfaces reach the fragment
shader or that depth testing has failed. This test cannot assign the rejection
to a particular hardware depth stage.

Per-draw background Clocks sum to approximately 212,285–212,722 for the six
recovering cases and 433,762–434,499 for the two persistent cases. Whole-case
sums are approximately 391,577–394,496 versus 803,971–806,567. These are isolated
replay attribution counters, not normal pass duration. Different hidden-surface
processing also changes seed/trigger fragment execution: both report zero in
the recovering cases and approximately 2.10 million each in the persistent
cases. Do not translate these ratios into a game FPS prediction.

All eight counter runs returned 15/15 requested counters and consistent draw
attribution with no mismatched or unverified events. Adaptive collection
requested five samples and stopped after three in every case. No outliers
were removed; maximum reported CV per case ranged from 0.004467 to 0.0496793.
The tool reported no crash, GPU hang, ANR or thermal throttling. Each managed
replay ended successfully.

## Implication for the next game experiment

The next candidate is a controlled opaque-first submission experiment: assess
whether eligible opaque draws can run before the first depth-writing blended
split, allowing them to populate LRZ first. Preserve masked/blended semantics
and audit ordering dependencies before moving draws. Do not simply disable
blending or depth writes on event 137; those states affect the intended image.

Any implementation should be optional and validated with the same demo,
resolution and settings, both-eye visual comparisons, a new hardware capture
and unhooked repeated A/B runs. Measure game GPU duration and fragments as well
as LRZ state: this control shows a state cause, not the size of its game cost.

## Validation and evidence

The Android ARM64 diagnostic compiled with native warnings treated as errors;
APK signing verification passed. Java emitted obsolete Java 8 source/target
warnings. The APK was installed as the separate `com.libertyvr.depthprobe`
package on the verified personal Quest 3 device.

All eight cases passed nine interior pixel checks in each eye both without
RenderDoc and during the injected capture run. All eight synthetic captures
were exported locally. The original three-case baseline also passed on the
updated APK without injection. The runner's Python syntax check passed.

Readback ends red in every transition case. Timestamp queries bracket the
whole render pass before readback; four warm-up and eight measured frames
precede the optional captured frame. These short sequential runs are functional
controls, not an estimate of game performance or a stable performance benchmark.

Raw synthetic captures/results remain in ignored `.cache/depth-transition-results`.
Sanitized replay counters, pipeline metadata and derived summaries remain in
ignored `.cache/renderdoc-analysis/Transition*` and `transition-summary.json`.
No capture or resource data belongs in version control.
