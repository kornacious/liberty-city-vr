# Per-pass Vulkan GPU profiling

Local implementation for Liberty City VR Quest. Source/build/host validation
requires no original game data. Original assets and diagnostic captures stay
on the user's computer/headset.

## Running the next benchmark

Replay the same completed 120-second demo with Auto Profile Playback enabled.
Minimal and full display modes both record the new GPU metrics automatically;
the HUD and replay controls remain the same. Let playback finish so profiling
closes its files. Keep assets, graphics settings, refresh rate, ghost/tracking
mode and head direction consistent. Two comparable runs help establish normal
variation before making a rendering optimization.

This build corrects measurement attribution; it does not optimize the renderer.
Use a baseline from this instrumented build for subsequent A/B work. Old schema 1–3 captures
remain readable, but have no per-pass data.

## What the pass buckets mean

Schema 5 measures **complete render passes from outside their boundaries**.
The four initial schema 4 replay captures showed approximately 0.0006 ms in
roads/world/fading despite hundreds of draws, while most elapsed time appeared
in `other`. These in-render-pass timestamps do not attribute scene execution
usefully on the Quest tiler. Do not interpret them as cheap scene rendering or
an expensive unidentified `other` shader.

| GPU-timed bucket | Work bracketed |
| --- | --- |
| scene | Entire native multiview scene render pass: sky, roads, world, water, fading, weather, effects and game UI; includes attachment clear, MSAA resolve and store |
| wrist | Wrist target rendering, outside all its render-pass boundaries |
| scope | Physical scope rendering/replay, outside its render-pass boundaries |
| resolve_postprocess | Remaining scene-image preparation, reflection reduction, optional SGSR conversion and final post pass, excluding nested scope work; scene MSAA resolve belongs to `scene` |
| other | Command-stream intervals outside these timed groups |

Sky, roads, reflections, world, water, fading, weather, effects and UI retain
actual draw-command counts and their existing CPU timings. Their separate GPU
milliseconds are **unavailable (`-1`, timing mask clear)**. The timing mask does
not control draw-count availability. Draw buckets are exclusive logical
submission phases; `scene` draw counts are only scene draws outside a named
logical phase, not a duplicate of all scene draws. `other` draws are commands
outside every named draw scope.

These GPU intervals are approximate elapsed costs, not exclusive shader times.
The separate OpenXR debug/menu/recording quad and runtime compositor are outside
this command buffer. Draw counts count actual `vkCmdDraw`/`vkCmdDrawIndexed`
commands, including split materials and scope replays, without multiplying a
multiview command by the number of eyes.

The locally queried personal Quest 3 device did not advertise
`VK_QCOM_elapsed_timer_query`. This implementation uses the already supported
ordinary timestamp queries, with no requirement for that extension. Khronos
[documents the in-render-pass timestamp limitation on tiled GPUs](https://docs.vulkan.org/features/latest/features/proposals/VK_QCOM_elapsed_timer_query.html).
Fine GPU phase comparisons require separate targeted experiments or supported
hardware profiling; splitting the scene render pass solely for measurement
would change tiling/attachment costs and is deliberately avoided.

## Sampling and GPU safety

- Total-frame timestamps retain the existing every-frame behavior. Per-pass
  markers and draw accounting sample one frame per eight-frame block. The
  selected phase rotates between blocks to avoid consistently measuring only
  one phase of periodic rendering work.
- A bounded pool reserves two query indices per marker, including markers
  outside render passes. Vulkan multiview timestamps consume one slot per
  active view. The first lane supplies the interval; secondary lanes are not
  added together or treated as independent frames.
- Markers use bottom-of-pipe boundaries **outside native render passes**,
  partitioning the frame's existing top-to-bottom timestamp span. Logical
  scene brackets change draw attribution without emitting timestamp commands.
  Repeated/nested timed intervals accumulate into their buckets. Scope cost is
  excluded from its enclosing postprocessing bucket. Render passes and draw
  ordering are preserved;
  profiling introduces no render-pass splits or new pipeline barriers.
- Query results are read only when the normal frame-slot reuse has already
  observed its submission fence. Readback requests availability and never uses
  `VK_QUERY_RESULT_WAIT_BIT`, a profiler fence wait, or a queue-idle drain.
  Unwritten spare mono lanes may produce `VK_NOT_READY`; only availability of
  the actual primary boundary queries is required.
- Query overflow, unbalanced scopes, unavailable boundaries and unsupported
  timestamps are explicit unavailable results. Missing timings are `-1`, not
  zero. A zero duration is legitimate only for a valid measured interval.
- Sampling stops when profiling is off. Enable/disable epochs reject pending
  samples from a previous capture, even when CPU frame IDs restart at one.
  Full/minimal switching keeps the existing capture/epoch.

Timestamp commands themselves impose GPU execution dependencies. On a tiled,
pipelined GPU they can perturb workload/order or redistribute observed time.
Sampling reduces probe cost but does not make it free. A high bucket is evidence
for a targeted follow-up, not proof of a particular shader/overdraw cause.

The [Vulkan timestamp command rules](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdWriteTimestamp.html)
define multiview slot consumption; the [official timestamp sample](https://docs.vulkan.org/samples/latest/samples/api/timestamp_queries/README.html)
explains timestamp stages, pipeline overlap and execution dependencies.

## Capture schema 5 and analysis

The existing `.frames.csv` / `.session.csv` export endpoints and file names
remain unchanged. The original schema 4 columns keep their order; `gpu_pass_scene_ms` and
`gpu_pass_scene_draw_calls` are appended. Schema 5 session metadata records
`gpu_pass_scope=outside_render_pass_bottom_of_pipe_intervals`, the timed buckets,
scene resolve ownership and independent logical draw attribution:

- `gpu_pass_frame_id`, `gpu_pass_start_monotonic_ns`: originating CPU capture
  frame identity, not the CPU row in which the GPU result was read back.
- `gpu_pass_status`: 0 none, 1 complete, 2 overflow, 3 unbalanced, 4 unavailable.
- `gpu_pass_valid_mask`, `gpu_pass_frame_ms`: measured pass presence and total
  GPU span from that same sampled source frame.
- `gpu_pass_<bucket>_ms`, `gpu_pass_<bucket>_draw_calls`: per-bucket intervals
  and actual draw-command counts.

Session metadata records sampling interval, timestamp scope, status meanings
and tail behavior. The latest completed sampled result can repeat in several
CPU rows; this is intentional and must not inflate the sample count.

`tools/analyze-perf-capture.py` supports schemas 1-5. Its `gpu_pass_profile`
summary deduplicates source IDs, verifies source monotonic timestamps, filters
using source-frame gameplay state, and reports unavailable/unmatched samples.
It summarizes timings using the timing mask and draw counts independently,
labels schema 4 attribution as unreliable, and ranks slow five-second windows
using capture-relative time from the originating frame. The replay run report
supplies the offset from capture start to simulation playback time. A result delivered during a
paused readback row can still describe a valid earlier gameplay frame.

Pending GPU samples at capture shutdown are not drained with extra waits. The
last slots can therefore be absent; the analyzer reports unobserved expected
source frames. Dropped CPU source rows are excluded rather than guessed.

## Validation

- Android arm64 debug compilation passes from a fresh source assembly.
- The final APK is installed with `install -r` on the personal Quest 3 device;
  game data, saves and recordings are preserved, and diagnostics export responds.
- Production backend host tests cover multiview query bounds, nonblocking
  readback with unwritten spare lanes, nested/repeated passes, draw attribution,
  timer wrapping, query/stack overflow, unbalanced scopes, deferred scene
  execution, draw-only in-pass brackets, exclusive nested timed groups, disabled/unsampled
  paths and capture-session isolation.
- Writer tests verify schema 5 metadata, pass names, source identity, timing
  precision, draw counts, unknown values, bounded queue/drop accounting and
  shutdown drain. Analyzer tests cover schemas 1-5, delayed deduplication,
  source-state filtering, identity mismatch, unavailable passes and tail gaps.
- Existing pipeline and descriptor/frame-slot regressions pass.

Host Vulkan calls are mocked. Device query distribution and instrumentation
overhead require the next replay capture; no on-headset per-pass improvement or
GPU cost attribution is claimed by the build/host tests alone.

Focused reproduction:

```powershell
./tools/tests/test-gpu-pass-profile.ps1 -OutDir <new-output-directory>
./tools/tests/test-perf-capture.ps1 -OutDir <new-output-directory>
python tools/tests/test-perf-analysis.py
```
