# Sampled geometry, material-pass and model profiling

## Recording

Replay the same 120-second demo twice with Auto Profile Playback enabled.
Minimal and full profiler displays both record this data. Keep graphics,
Classic assets, head direction, replay/ghost options and actual output
**1512 x 1584** consistent. Allow each playback to finish normally.

Each schema 6 capture produces three matching files:

- `.frames.csv`: existing CPU, runtime GPU and whole-render-pass timing data;
  its columns are unchanged from schema 5.
- `.session.csv`: schema, units, model capacity, capture completion, row/drop
  accounting and companion sample/row counts.
- `.draws.csv`: phase totals and model-owner counters, written once per
  completed sampled GPU result. Copy all three files together when exporting
  manually. The existing read-only ADB capture provider now accepts this
  companion filename; it still exposes no arbitrary files or mutation.

This is an instrumentation build, not another rendering optimization. Its
overhead must be assessed with the next device captures; host tests do not
establish a performance gain.

## Counters

Counters use the same rotating one-per-eight-frame sample as GPU timing. Every
companion row includes the originating frame ID and monotonic start timestamp,
not the later CPU readback frame. Named logical phases retain their meanings
from [GPU profiling](GPU_PASS_PROFILING.md).

| Counter | Meaning |
| --- | --- |
| draws | Actual submitted draw commands, matching the frame CSV |
| elements | Submitted index count, or vertex count for non-indexed commands; not unique vertices |
| triangles | Submitted triangle primitives, including repeated split passes |
| geometry_known_draws | Commands with a supported known topology/count |
| mesh_passes | Issued first-pass atomic mesh/material commands; not unique materials |
| extra_split_draws | Additional issued PS2 or vehicle alpha split commands |
| alpha_requested_draws | Mesh classification requests alpha based on texture, vertex or material data |
| texture_alpha_draws / vertex_alpha_draws / material_alpha_draws | Separate alpha input reasons |
| alpha_test_draws / depth_write_draws | Pipeline state selected for the issued atomic mesh command |
| ps2_split_draws / vehicle_split_draws | Commands participating in each split policy, including both halves |
| distance_fade_draws | Commands submitted while their owning entity has distance fading enabled |
| skinned_draws | Atomic mesh commands using the skin/bone path |

Reason flags overlap. They describe command features, not pixel coverage,
overdraw, exclusive GPU duration, or proof of unnecessary rendering. Immediate
and postprocessing draws contribute geometry counts but not atomic material
reason flags. Scope replay commands currently retain unknown geometry and
owner; `geometry_known_draws` makes that limitation explicit.

The [uniform-alpha fast path](UNIFORM_ALPHA_FAST_PATH.md) can retain only the
contributing half of a PS2 split. Its sole submitted half counts as the primary
mesh pass; PS2 policy flags still apply to it. Use `extra_split_draws` for actual
additional submissions rather than dividing `ps2_split_draws` by two.

The [opaque-world pipeline](OPAQUE_WORLD_ALPHA.md) additionally selects the
existing shader variant without alpha testing for proven opaque unsplit world
meshes. `alpha_test_draws` counts the selected variant rather than the restored
engine state, allowing its coverage to be checked against total draw counts.

Triangles are not multiplied by eye count. List counts use index/vertex count
divided by three; strips/fans use count minus two and include degenerates.
Submitted triangles are not rasterized/visible triangles or unique geometry.

## Model attribution and bounds

Model rows identify owner ID, sanitized name, entity class and logical phase.
Classes are building, vehicle, pedestrian, object and unknown (0-4). Entity
scopes cover ordinary submissions, nested occupants, delayed vehicle alpha
atomics and the direct distance-fade callback. Descendant attachments drawn
within an entity scope belong to that owner; these are not per-atomic asset
identifiers. Unscoped commands use model ID -1 and name `unattributed`.

The original 512-entry model/class/phase table truncated detail in the dense
35-40s and 65-75s sections. That fixed limit is removed. Each frame slot reserves
2,048 entries before its first sample, doubles storage and rehashes when needed,
and retains the allocation for subsequent samples. This is an initial reserve,
not another fixed attribution limit. The table stays at most half full.

Growth is transactional: a table-allocation failure preserves tracked owners
and complete phase counters. Omitted model draws remain explicit in
`model_overflow_draws`; `model_allocation_failures` reports failed reserves.
A failed reserve is not retried for each new owner during the same sample.
Existing owners can still accumulate counters. Context stack overflow is
separately reported and temporarily attributes draws to unknown, restoring the
enclosing owner on unwind.

Schema 6 is retained, with four optional columns appended to `.draws.csv`:
`model_buckets`, `model_capacity`, `model_table_growths` and
`model_allocation_failures`. They describe the source sample and repeat on its
rows; do not sum them per row. Growth includes initial allocation. Session
metadata records `draw_model_capacity=dynamic`, the initial reserve and growth
policy. The analyzer accepts older schema 6 files without these columns.

No per-draw clocks, GPU waits, extra timestamp markers, render-pass splits or
render-state changes are introduced. Normal draws use reserved storage; a draw
can trigger allocation only when the table needs to expand. Unsampled/off
frames do not clear the model table or look up model names. Completed GPU
readback creates one immutable snapshot containing only occupied model rows;
readbacks and the async writer share it, avoiding a full table copy on every
CPU frame. Snapshot ownership survives frame-slot clearing and growth.
Queue/batch storage is on the heap; the queue is released when recording stops.

## Analysis and validation

`tools/analyze-perf-capture.py` accepts schemas 1-6. For schema 6 it reads the
companion automatically and adds `draw_profile` with phase averages, model
rankings by draws/triangles, and the same slow five-second route windows.
Model averages include absent frames as zero by dividing by all selected
samples, rather than only appearances. It filters using source gameplay state,
matches GPU/frame identity, rejects duplicate/incomplete/inconsistent source
groups, reconciles model/phase totals with overflow, and reports missing files,
source gaps, unscoped coverage, peak model/capacity use, growth events and loss
counts. Truncation is explicit globally, by phase and in slow windows. Phase totals must agree with
the existing GPU draw-command counts.

Validated locally without original assets:

- Fresh pinned-base assembly and Android arm64 debug build.
- Production GPU hooks and portable counters: list/strip counts, split reasons,
  nested restoration, name sanitization, automatic growth, storage reuse,
  retained-owner behavior after reserve failure, context overflow,
  disabled paths, nonblocking readback and session isolation.
- Actual CSV writer: immutable snapshot lifetime, precision, one companion
  export per sampled result, schema 6 metadata and split/fade counts.
- Writer-to-analyzer round trip, matching/reconciliation, source filtering,
  overflow and missing-companion behavior; legacy schema tests remain passing.
- Java companion filename/canonical-path restrictions and road visibility
  regression tests remain passing.
- Task APK installed with `install -r` on the personal Quest 3 device. Saves,
  game data and demos preserved; read-only diagnostics export responds.

The growth regression records 6,915 model/phase entries across three phases,
forces multiple rehashes, verifies all counters and later updates, then resets
and reuses storage. The production writer/analyzer round trip exports all 6,915
entries with zero missing/omitted draws, peak capacity 8,192 and three initial/
growth events. Previous immutable snapshots remain valid after recorder reset.
The original schema 6 captures remain readable and explicitly truncated.

The task APK with this fix was built and installed with `install -r` on the
personal Quest 3 device, preserving saves, original data and demos. The next two
replays validate real asset attribution and overhead at 1512 x 1584, 120 Hz,
consistent head direction and fixed in-game/SideQuest performance settings.
