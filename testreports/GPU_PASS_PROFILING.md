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

This build adds measurements; it does not optimize the renderer. Use a baseline
from this instrumented build for subsequent A/B work. Old schema 1–3 captures
remain readable, but have no per-pass data.

## What the pass buckets mean

| Bucket | Work bracketed |
| --- | --- |
| sky | Clouds and horizon |
| roads | `CRenderer::RenderRoads` |
| reflections | Corona reflection rendering |
| world | `RenderEverythingBarRoads`, including whatever materials that engine entry point submits |
| water | Boats and water |
| fading | `RenderFadingInEntities`; this is not every possible transparent draw |
| weather | Rain streaks |
| effects | The existing RenderEffects CPU bracket |
| ui | Game HUD/frontend rendering inside the renderer |
| scope | The existing physical-scope target rendering/replay |
| wrist | The existing wrist target rendering |
| resolve_postprocess | Scene resolve, reflection reduction, optional SGSR conversion and final post pass, excluding nested scope work |
| other | Clears, barriers, unbracketed draws and gaps between named passes |

These are command-stream intervals, not exclusive shader execution costs.
The separate OpenXR debug/menu/recording quad and the runtime compositor are
outside this render command buffer. Draw counts count actual `vkCmdDraw` or
`vkCmdDrawIndexed` commands, including split alpha/material passes and scope
replays, without multiplying multiview commands by the number of eyes.

## Sampling and GPU safety

- Total-frame timestamps retain the existing every-frame behavior. Per-pass
  markers and draw accounting sample one frame per eight-frame block. The
  selected phase rotates between blocks to avoid consistently measuring only
  one phase of periodic rendering work.
- A bounded pool reserves two query indices per marker, including markers
  outside render passes. Vulkan multiview timestamps consume one slot per
  active view. The first lane supplies the interval; secondary lanes are not
  added together or treated as independent frames.
- Markers use bottom-of-pipe boundaries, partitioning the frame's existing
  top-to-bottom timestamp span. Repeated/nested pass intervals accumulate into
  their buckets. The renderer's render passes and draw ordering are preserved;
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

## Capture schema 4 and analysis

The existing `.frames.csv` / `.session.csv` export endpoints and file names
remain unchanged. New columns are appended after schema 3's begin diagnostics:

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

`tools/analyze-perf-capture.py` supports schemas 1–4. Its `gpu_pass_profile`
summary deduplicates source IDs, verifies source monotonic timestamps, filters
using source-frame gameplay state, and reports unavailable/unmatched samples.
It summarizes each bucket's timings/draws and ranks slow five-second windows
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
  timer wrapping, query/stack overflow, unbalanced scopes, disabled/unsampled
  paths and capture-session isolation.
- Writer tests verify schema 4 metadata, pass names, source identity, timing
  precision, draw counts, unknown values, bounded queue/drop accounting and
  shutdown drain. Analyzer tests cover schemas 1–4, delayed deduplication,
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
