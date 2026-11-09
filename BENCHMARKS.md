# Quest compatibility profile and matched replay benchmarks

The Windows game can apply a rendering workload captured on an actual Quest 3.
It uses the shared game/shaders, captured eye-buffer dimensions, MSAA, graphics
and model-set selections, anisotropy and optional feature restrictions. Matched
replay also uses the captured stereo FOV, separation and cant. This is a settings
and workload compatibility profile. Desktop measurements remain measurements of
the PC; the profile does not emulate Adreno execution, memory, thermal behavior,
the standalone compositor or a Quest frame rate. No artificial slowdown is applied.

The Vulkan baseline remains 1.1 with stereo multiview. The backend now consumes
the features actually enabled on the adopted device, including compression and
anisotropy, instead of assuming every advertised physical feature was enabled.
Unavailable required anisotropy/fragment-atomic features or MSAA fail startup.
Exact profile eye extents must fit and allocate; there is no silent lower-resolution
fallback. Refresh rate is requested through existing runtime support and measured
in captures; a different actual rate fails comparison. Windows retains its correct
sRGB compositor handoff rather than copying Android's presentation format policy.
Mobile ASTC/ETC2 availability can differ on PC and is reported separately; the
current librw asset path uses BC uploads or its existing decoded fallback.

## Capture and use a profile

1. Build/install both targets from the same source-kit contents. The manager embeds
   a SHA256 identity of native/shader/build inputs, platform source and patches,
   including the pinned re3 base identity. Documentation edits do not change it;
   game-data contents are fingerprinted separately.
2. On the standalone personal Quest 3 device, enter gameplay and start a profiler
   capture after rendering has started. Each capture writes a numeric
   `perf-captures/<capture>.quest-profile` beside its frames/session CSVs. Export
   is valid only on Quest 3, after stereo geometry and measured refresh are known.
   Display rate comes from the runtime's actual display-rate query; application
   pacing/reprojection periods are not used as display refresh. Settings are
   allowlisted; no serials, tracking origins or personal calibration
   files are included. Effective MSAA and output dimensions are recorded.
3. Collect recent diagnostics locally into a **new** directory:

   ```text
   dotnet run --project tools/manager -c Release -- --benchmark-collect C:/LCVRBuild/benchmarks/quest-001
   ```

   This selects the verified personal Quest 3 device through the manager's privacy
   boundary and the app's read-only diagnostics provider (compatible with scoped
   storage), and copies at most 12 recent allowlisted files from each of
   `perf-captures` and `simulation-replays`, verifying size and SHA256. It never
   copies game assets or saves. If several eligible headsets are connected, keep
   only the intended one connected for this CLI operation. Collect after quitting
   or stopping the capture so the files are complete.
4. In **Launch PCVR**, select the collected `.quest-profile` file. Blank selects
   native PCVR. Changing profiles requires restarting the game. CLI callers may
   set `Options.PcvrQuestProfile`, or launch the executable with
   `--quest-profile FILE` alongside the existing data/user arguments. The desktop
   ImGui panel shows the current profile. Tracked gameplay remains available.

Profile settings override reads in memory and do not replace saved graphics
preferences. A graphics change attempted during a profile session marks it dirty;
restart before taking a matched measurement. Profiles from newer settings or
invalid/unknown fields are rejected or disqualified rather than silently matched.

## Run the same workload on both targets

Use the in-game VR menu's **Optimizations → Simulation replay** on each target. Record
a route of at least 10 seconds (30 seconds is a useful starting point), then stop
and collect it. Copy the same generated `.sim.csv` locally into the PCVR user
directory's `simulation-replays` folder without replacing another tape. Tapes
contain controls/reference positions, not game assets. Playback spawns the recorded
car and seats the player if needed, or reuses the matching occupied car. It repairs
and restores the recorded starting pose, velocity and drivetrain state even if the
car is still moving. Handling and active model categories must still match.

Set **Playback count** from **1** to **10** before selecting Play. Each pass has its
own unprofiled preparation, performance capture and `.run.csv` report, with
`repeat_index` and `repeat_count` in the report and capture context. The previous
capture closes before the next reset; the profiler's prior ON preference resumes
only after the batch ends. Stop, playback errors or report-write failures cancel
the remaining passes. Ambient world state is not reset between passes.

Enable **Matched workload** on Quest. Loading a Quest profile enables it for PCVR.
Matched workload defaults on for new sessions; the menu can select a live-world
run explicitly. V3 tapes include numeric handling parameters. Matching permits
only `1e-6 + 1e-5 × magnitude` rounding differences, with exact serialized flags;
material handling changes are rejected. Legacy V1/V2 tapes keep their exact-hash
validation. Playing one on its original matching setup creates a new V3 copy and
preserves the original, allowing cross-target numeric validation without recording
the drive again. Run reports retain both the reference and execution fingerprints.
Use playback ghost, route assistance, benchmark forward view and auto profile;
leave RenderDoc capture off. Matched playback requires those settings. Preparation
fingerprints local `data`, `models`, `anim`, `txd` and `modelsets` contents and the
tape before profiling. Fingerprinting can take time and warms the file cache;
these are warm replay benchmarks, not cold-loading benchmarks. FNV-1a-64
fingerprints detect workload differences; they are not security digests.

Replay holds noon/sunny conditions each simulation step and restores the previous
clock/weather on completion, stop or abort. It retains the existing unprofiled
preparation. On Windows, desktop mirror GPU presentation is suppressed while the
matched run prepares/plays, then resumes. Render poses use the captured stereo
geometry and a fixed forward view; untouched tracked poses remain with the OpenXR
compositor. Stop the run if the synthetic view is uncomfortable.

Run the route at least three times per target. Keep starting saves, relevant
mods/settings, runtime refresh and audio setup consistent. Ambient RNG, traffic,
particles and audio are not restored by the replay. Route assistance corrects
the vehicle path; it does not make the whole game deterministic. Warm caches,
thermal state and host/runtime scheduling still affect results.

## Compare local reports

If asset fingerprints differ, diagnose them locally before collecting another pair:

```text
dotnet run --project tools/manager -c Release -- --benchmark-inventory C:/LCVRBuild/benchmarks/inventory-001
dotnet run --project tools/manager -c Release -- --benchmark-align-quest C:/LCVRBuild/benchmarks/aligned-001
```

Both commands require local ADB access to the authorized Quest 3 and the manager's
prepared PCVR data configuration. Append a local manager `REQUEST.json` argument
to either command to use that request's Settings (for example, a separate Original
Xbox installation) without changing global preferences. Inventory compares path, size and SHA256 metadata
without transferring asset contents. Alignment creates a **new** data/user copy,
reuses verified matching PC files and reads only missing/different files from the
headset through a root/shell-authorized read-only provider. Its scope is `data`,
`models`, `anim`, `txd` and `modelsets`; PC-only files in those roots are excluded.
Original game data and existing saves remain unchanged. The copied PCVR user folder
retains saves and routes but excludes old performance captures. Every asset copy is
size/SHA256 verified; interrupted setups retain `INCOMPLETE.txt` and must not be used.
All assets and inventories must stay on the local computer, outside version control.

Use the completed `benchmark.json` data/user paths in a local launch request's
`Settings.PcvrDataDirectory` and `Settings.PcvrUserDirectory`, together with the
fresh Quest profile in `Options.PcvrQuestProfile`. Global manager settings are not
changed. Alignment matches the fingerprinted asset scope; retain consistent audio,
starting saves and gameplay settings separately.

Keep each target's collected `simulation-replays` and `perf-captures` directories
adjacent. PCVR already writes that structure under its isolated user directory.
Then run:

```text
dotnet run --project tools/manager -c Release -- --benchmark-compare C:/LCVRBuild/benchmarks/quest-001/simulation-replays/QUEST.run.csv C:/LCVRBuild/windows-user/simulation-replays/PC.run.csv
```

Replace the report names with the generated names. JSON goes to stdout; exit 0
means the workload checks matched, exit 2 means rejected matching, and exit 1
means an invalid/incomplete input or another execution error. Individual measured
summaries remain available with mismatch reasons. The comparer checks completion,
source/route/assets/model/handling identity, profile, actual rendering state and
refresh, fixed view, no pauses/cutscenes/RenderDoc, and closed error-free captures.
Legacy reports cannot qualify as matched. Different asset contents deliberately
fail comparison even when the model-set names are identical.

Profile snapshots are checked against their recorded identities before numeric
comparison. Display-rate quantization below 0.001 Hz is tolerated; different
refresh targets are rejected. Recorder-row losses are reported separately from
settings or view changes and attributed to the affected target.

The report excludes the first measured second in addition to preparation, requires
at least 30 valid frames, and reports CPU-local time and frame interval mean,
p50, p95 and p99. GPU summaries use unique delayed **source frame** samples within
the measurement interval. Repeated latest GPU values are deduplicated; unavailable
GPU timing stays unavailable. Frame interval includes runtime pacing and is not
GPU execution time. Do not interpret cross-device ratios as emulated Quest speed.
Matching display refresh and workload settings does not imply matching achieved
application FPS. Runtime submission, driver pacing or missed frame deadlines can
reduce application rate; inspect the measured frame intervals independently.
Windows logs bounded `PCVR pacing` aggregates for OpenXR wait/begin/end, local
frame work, predicted period and display Hz when investigating such differences.
Record external streaming settings and headset optimizer overrides alongside PCVR
results. During validation, a SideQuest optimizer profile capped Virtual Desktop
on Quest 3 at 90 FPS even though OpenXR reported 120 Hz. The user corrected the
optimizer profile and confirmed sustained 120 FPS gameplay; three matched captures
averaged 8.334–8.338 ms intervals. HEVC/H.264 codec selection was not the cause.
Check headset-side per-app FPS limits as well as Virtual Desktop's streaming rate.
A desktop Quest profile cannot override those external limits.
For Original Xbox measurements, install the same verified collection on both
targets and record a route with Original Xbox active; Classic routes retain their
asset-selection checks. Match vehicle placement templates and personal layout
fields too. Rendering profiles do not automatically copy those configuration files.
Use an isolated benchmark data copy when matching layouts so normal Quest/PCVR
personal overrides remain independent.
Validate optimizations on actual Quest captures and compare repeated runs on each
device independently. Never upload original game data with reports.

## Local validation — 2026-11-09

Three completed runs per target passed the workload comparer with no dropped
recorder rows. They used the same source identity, portable Kuruma route, aligned
asset scope and captured 1512x1584-per-eye, 1x MSAA, 120 Hz profile. Values below
are the range of the three run means after warmup exclusion.

| Execution target | Frame interval (ms) | Local CPU (ms) | Sampled GPU (ms) |
| --- | ---: | ---: | ---: |
| Personal Quest 3 device, standalone | 9.375–9.486 | 5.795–6.071 | 7.495–7.587 |
| Windows PCVR, RX 6700 XT | 8.334–8.338 | 2.538–2.600 | 0.726–0.738 |

PCVR gameplay at 120 FPS was confirmed after correcting the headset optimizer
override described above. These observations establish an exercised comparison
workflow, not equivalent GPU performance or deterministic simulation. The measured
Windows build precedes the additional pacing loggers; the preserved reports are
not relabeled as measurements of subsequent diagnostic builds. See
[validation details](tools/manager/VALIDATION.md).

### Original Xbox validation

Three new runs per target also passed with Original Xbox active for all five
asset categories, matching source/route/assets/profile and zero dropped rows.
The same 1512x1584-per-eye, 1x MSAA, 120 Hz profile was used. Vehicle layouts were
copied into a separate benchmark directory; normal target overrides were preserved.

| Execution target | Frame interval (ms) | Local CPU (ms) | Sampled GPU (ms) |
| --- | ---: | ---: | ---: |
| Personal Quest 3 device, standalone | 10.023–10.115 | 6.496–6.582 | 9.170–9.220 |
| Windows PCVR, RX 6700 XT | 8.334–8.335 | 3.078–3.347 | 0.988–1.019 |

These are ranges of three run means after warmup exclusion. This Xbox route
differs from the earlier Classic route; the tables do not measure the performance
cost of switching asset sources. PCVR Xbox visuals/audio were confirmed, and an
unchanged vehicle template received a live reload nonce/hash acknowledgement.
