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

A fixed hash table tracks up to 512 model/class/phase combinations per sampled
frame. Capacity overflow increments `model_overflow_draws`; phase totals still
include every command. Rankings are incomplete when this is nonzero. Context
stack overflow is separately reported and temporarily attributes draws to
unknown, restoring the enclosing owner on unwind.

No per-draw clocks, GPU waits, extra timestamp markers, render-pass splits,
or render-state changes are introduced. Counters allocate no memory per draw.
An immutable model snapshot is allocated once per newly delivered sampled
result and shared by repeated observations and the bounded async writer.
Queue/batch storage is on the heap; the queue is released when recording stops.
Unsampled/off frames do not clear the model table or look up model names.

## Analysis and validation

`tools/analyze-perf-capture.py` accepts schemas 1-6. For schema 6 it reads the
companion automatically and adds `draw_profile` with phase averages, model
rankings by draws/triangles, and the same slow five-second route windows.
Model averages include absent frames as zero by dividing by all selected
samples, rather than only appearances. It filters using source gameplay state,
matches GPU/frame identity, rejects duplicate/incomplete/inconsistent source
groups, reconciles model/phase totals with overflow, and reports missing files,
source gaps, unscoped coverage and loss counts. Phase totals must agree with
the existing GPU draw-command counts.

Validated locally without original assets:

- Fresh pinned-base assembly and Android arm64 debug build.
- Production GPU hooks and portable counters: list/strip counts, split reasons,
  nested restoration, name sanitization, bounded model/context overflow,
  disabled paths, nonblocking readback and session isolation.
- Actual CSV writer: immutable snapshot lifetime, precision, one companion
  export per sampled result, schema 6 metadata and split/fade counts.
- Writer-to-analyzer round trip, matching/reconciliation, source filtering,
  overflow and missing-companion behavior; legacy schema tests remain passing.
- Java companion filename/canonical-path restrictions and road visibility
  regression tests remain passing.
- Task APK installed with `install -r` on the personal Quest 3 device. Saves,
  game data and demos preserved; read-only diagnostics export responds.

The next two replays validate real asset attribution, capacity and overhead.
