# Stage 1: Quest depth-rejection and LRZ reporting control

Measured locally on 2026-11-10 using the personal Quest 3 device and RenderDoc
Meta Fork v207. The separate `com.libertyvr.depthprobe` APK uses generated
geometry only. No game renderer code, game data, settings or saves were changed.
Stage 2 (game pipeline/shader-specialization inspection) was not performed.

## Controlled workload

One offscreen multiview render pass: 1024x1024 per view, two views, 1x MSAA,
RGBA8 colour, D32_SFLOAT depth cleared to 1, LESS_OR_EQUAL comparisons. Every
case submits 17 draws / 8,194 triangles, counted once per multiview command.
The same 16 background grids are drawn far to near. A foreground quad is either
offscreen, onscreen with depth writes, or onscreen without depth writes.

Both pipelines disable blending, culling, stencil and alpha-to-coverage. The
compiled fragment SPIR-V was checked to contain no fragment kill/demote, depth
output, sample-mask output, image writes or atomics. It uses procedural colour,
with no textures or storage resources. Nine interior pixels per view verify
the expected red/blue result in each case.

## Measured counters

All three generated captures replayed successfully in the same draw-call
profiling mode. All 15 requested counters were available, with consistent
trace attribution, no mismatched events and no unverified draws.

| Case | Background fragments shaded | Foreground fragments shaded | LRZ write-enabled draws | LRZ primitives killed |
|---|---:|---:|---:|---:|
| Open / foreground offscreen | 2,097,150 | 0 | 17 / 17 | 0 |
| Foreground occludes background | 0 | 2,097,150 | 17 / 17 | 0 |
| Foreground depth writes OFF | 2,097,150 | 2,097,150 | 16 / 17 | 0 |

The background is shaded in the open and no-depth-write cases and completely
rejected behind the depth-writing foreground. Ordinary unhooked runs and the
captured run passed all pixel checks in both views.

Raw LRZ state is `3` (test and write enabled) for the positive-control draws.
Only the foreground draw in the no-depth-write control changes to `1` (test
enabled, write disabled). The tool therefore distinguishes LRZ write-enable
state on this headset. This boolean describes enabled state, not a count of
actual LRZ memory writes.

Open and no-depth-write counter collection stopped adaptively after 3 samples;
occluded collected 5. Maximum reported coefficients of variation were 0.0449,
0.0570 and 0.00230 respectively. The occluded collection removed 74 outliers;
the other two removed none. Background fragment counts were the decisive
control measurement, not a ranking of noisy individual draw times.

## Supporting GPU timestamps

Timestamp queries bracket the complete native render pass outside multiview;
CPU readback follows the measured interval. Four warm-up frames precede eight
measured frames per case. The captured frame is excluded from the median.
One unhooked run measured medians of 1.416 ms (open), 1.166 ms (occluded) and
1.689 ms (foreground depth writes OFF). These short sequential samples use
ordinary device clock policy and are not a performance benchmark or evidence
of a game FPS improvement. Injected timings are kept separately and must not
be compared directly against unhooked timings.

## Conclusion and limits

The simple control demonstrates effective background depth rejection and a
working LRZ write-enable reporting path. The game captures' write-disabled
state is therefore not explained by the reporting tool always returning zero.
The cause in the game remains untested: attachments, shader variants, draw
ordering and other state differ from this deliberately simple workload.

`LRZ Prims Killed` stays zero even in this successful control. Its zero value
alone must not be used as evidence that LRZ/depth rejection is ineffective.
The open background's fragment count is approximately one view-sized layer per
eye despite sixteen submitted layers; do not expect all sixteen layers to be
shaded when hardware hidden-surface rejection is working.

The diagnostic APK remains installed and its process is stopped. All managed
replays were ended. Captures, raw timestamps and sanitized full counters remain
in ignored local `.cache/depth-control-results` and `.cache/renderdoc-analysis`
directories. No capture binaries are committed or uploaded.

See [build, run and capture instructions](../tools/depth-rejection-probe/README.md).
