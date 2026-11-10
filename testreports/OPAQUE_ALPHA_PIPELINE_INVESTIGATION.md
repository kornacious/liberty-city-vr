# Stage 2: captured game depth state and alpha specialization

Investigated locally on 2026-11-10. This stage inspected existing captures,
their structured Vulkan metadata and saved replay profiling data. It did not
change game rendering code, settings, assets or saves, and did not start a new
game capture or GPU replay session. Stage-1 diagnostic files remain separate.

## Main finding

**Opaque World Fast Path was OFF throughout both road capture runs.**
The capture sessions' `opaque_world_alpha_fast_path_at_start` values are `0`.
Every frame row also has `opaque_world_alpha_fast_path_enabled=0`: 2,542 rows
for RoadAutoCapture and 2,560 for RoadAutoCaptureProfilingMode, including each
run's RenderDoc-active frame.

The two renamed local captures were matched to their original headset replay
reports by identical SHA256 content, rather than merely by filename/time.
Their actual trigger times were 22.502909 and 22.486241 simulation seconds;
both targeted 22.486241 seconds. They are closely matched, separate frames.

## Captured pipeline evidence

| Metric | RoadAutoCapture | RoadAutoCaptureProfilingMode |
|---|---:|---:|
| Scene draws | 3,233 | 3,209 |
| Depth-tested, depth-writing, non-blended scene draws | 2,389 | 2,377 |
| Scene draws with alpha-test specialization enabled | 3,233 | 3,209 |
| Scene draws using an alpha-test-disabled specialization | 0 | 0 |

All eight used scene pipelines supply specialization constant ID 0 as `1`.
That ID is `RW_ALPHA_TEST` in the renderer. The ninth used pipeline is the
separate postprocessing pipeline. The dominant scene pipeline accounts for
2,383 and 2,371 draws respectively; the additional six non-blended depth writers
use another vertex variant with the same fragment specialization.

The matching high-Clocks draws (12648/12654 and 12531/12537) use that dominant
pipeline. Their captured state is depth test ON, depth writes ON, LESS_OR_EQUAL,
blending OFF, stencil OFF, alpha-to-coverage OFF, and no face culling. Their
material alpha scalar is 1 and their positive alpha reference is 3/255. This
does **not** establish opacity of their texture/vertex alpha.

The fragment module matches the generated `rw_world_noeffects_frag.h` binary
from the game build byte for byte. Its default constant ID 0 is zero, but the
captured pipelines explicitly override it to one. Its SPIR-V contains the two
fragment-kill instructions corresponding to the positive/negative alpha-test
branches. Reading the module's default alone would therefore misclassify these
draws. No shader binary or original texture/mesh data was exported to the report.

Each capture's recorded command stream was correlated against the previously
collected replay metrics: all 3,234 / 3,210 draw and instance counts match in
order. Captured pipeline bindings, specialization values and narrow push-constant
control scalars were decoded locally. Resource buffer/texture/mesh contents
were not exported. Full derived metadata remains in ignored local cache files.

## Earlier draws and pass state

The scene uses a two-view multiview pass, 1512x1584 per view, 1x MSAA and
D24_UNORM_S8_UINT depth. Depth load operation is CLEAR with clear depth 1.
Scene pipeline comparisons consistently use LESS_OR_EQUAL. There are no
recorded mid-pass clear commands, dynamic depth/stencil state changes or
secondary-command-buffer execution commands in these captures.

The five draws preceding the first ordinary depth writer do not write depth.
The first depth writer already has `RW_ALPHA_TEST=1` and reported LRZ state `1`
(test enabled, writes disabled), before the later depth-writing blended draws.
This does not support blaming an earlier depth-writing blended draw for the
initial observation. It does not rule out every attachment/driver interaction.

## Source correlation and next validation

`useOpaqueWorldPipeline()` returns false immediately when
`getOpaqueWorldAlphaFastPathEnabled()` is false. Eligible draws otherwise select
`ALPHAALWAYS`, which becomes specialization constant ID 0 = 0. Thus the saved OFF
configuration explains why the intended opaque bypass was not selected here.
The captures do not demonstrate that the enabled fast path is broken.

Next, enable **Graphics > Optimizations > Opaque World Fast Path** and repeat
the same road replay/capture. Check that eligible opaque draws select constant
ID 0 = 0, and compare their LRZ write-enable state and shaded fragments.
Keep alpha testing on cutouts/fading draws through the existing eligibility
rules. Do not remove alpha tests globally or force all depth writers opaque.

Use an unhooked matched replay for any performance acceptance. Capture Clocks
remain attribution evidence: the existing normal-mode capture has only one
actual counter sample and the profiling-mode capture has noisy draw samples.
This investigation establishes a configuration explanation and a specific
next test; it does not yet establish restored LRZ writes or a game FPS gain.

## Follow-up: Opaque World Fast Path enabled

`RoadAutoCaptureOpaqueWorldFastPath.rdc` was replayed successfully on the personal
Quest 3 device. Its bytes matched the saved headset capture by SHA256. The
matched session flag is ON and all 2,488 profile rows, including the captured
frame, record `opaque_world_alpha_fast_path_enabled=1`.

The capture has 3,220 draws, 116,593 submitted triangles and two render passes.
All draw/instance counts were again matched against the recorded Vulkan command
stream. The main opaque pipeline now supplies constant ID 0 = 0 and is used by
2,363 draws. The remaining scene variants retain alpha testing. The existing
fast-path selection is therefore functioning; global removal of alpha testing
is neither necessary nor justified.

### LRZ write-enable transition

- The first 25 opaque draws, events 52 through 133, report raw LRZ state `3`
  (test and write enabled).
- Event 137 uses the masked-material-style positive alpha split: alpha reference
  128/255, depth writes ON, blending ON, alpha-test specialization ON. It reports
  raw LRZ state `1` (test enabled, write disabled).
- Event 140 is its negative-reference partner, with depth writes OFF.
- Subsequent draws using the same alpha-test-disabled opaque pipeline report
  state `1`. None of the remaining 2,338 opaque draws regain write-enable state
  in this render pass.

The state transition coincides with the first depth-writing blended draw. This
is now the leading investigation target. The capture establishes the sequence,
not a controlled causal isolation of blending versus alpha kill or every other
aspect of that draw. A follow-up control should introduce this combination into
the synthetic scene before proposing changes to masked rendering or draw order.
Preserve cutout/blend semantics; do not force the depth-writing split opaque.

Counter collection returned all 15 counters with consistent attribution and no
unverified draws. Five samples were collected, 11,011 outliers removed, maximum
reported coefficient of variation 2. Per-draw Clocks remain noisy attribution
evidence, not ordinary frame timings. `LRZ Prims Killed` remains zero.

### Unhooked playback comparison limitation

The new unhooked ON run and the earlier unhooked OFF run completed the same
30-second demo with matching route, assets and source identities, ghost,
route assistance and fixed-forward view. They differ in actual render extent:

| Metric | Earlier OFF run | New ON run |
|---|---:|---:|
| Per-eye extent | 1512x1584 | 1680x1760 |
| Refresh rate | 120 Hz | 120 Hz |
| Vulkan GPU median | 6.701 ms | 7.246 ms |
| Vulkan GPU p95 | 13.492 ms | 13.943 ms |
| App frame interval median | 8.443 ms | 8.483 ms |

The new extent contains 23.46% more pixels. These single sequential runs do not
isolate the option's performance effect; neither an FPS gain nor a regression
can be assigned to the toggle from them. Keep the actual resolution identical
for the next unhooked A/B comparison.

The managed replay was ended. No game code or configuration was changed during
analysis. Derived metadata, counter health and exported performance CSVs remain
local in ignored `.cache/renderdoc-analysis` directories.

The follow-up [eight-case transition control](DEPTH_STATE_TRANSITION_CONTROL.md)
now reproduces the persistent write-disable state with blending plus depth
writes, including a shader with no discard. Discard without blending recovers
on the next opaque draw in that control. This narrows the next experiment to
submission ordering and the depth-writing blended split, while preserving
masked rendering semantics. Background fragment execution remains zero in the
synthetic control, so the flag alone must not be treated as failed depth rejection.
