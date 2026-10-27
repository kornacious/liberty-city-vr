# Recording and comparing performance

In **Graphics → Performance Profiler (FPS)** select **ON**, the full diagnostic
mode. MINIMAL does not record detailed captures. Turn full profiling OFF after
the route to drain accepted samples and close the files.

Full profiling keeps the existing one-second `quest_perf.csv` and also writes
a unique pair of files for each session under:

```text
/sdcard/Android/data/com.libertyvr.quest/files/perf-captures/
    <UTC timestamp>-<monotonic timestamp>.frames.csv
    <UTC timestamp>-<monotonic timestamp>.session.csv
```

Copy both files locally using the headset file manager or local ADB workflow.
Android scoped storage can deny direct `adb pull` even for a debug APK. The app
therefore exposes a read-only ADB bridge: query
`content://com.libertyvr.quest.performance/captures` to list names/sizes, then
read `content://com.libertyvr.quest.performance/capture/<filename>` using
`adb shell content query` / `adb exec-out content read`. Select the verified
device locally and keep its identifier out of logs. Only the app, root and ADB
shell can use this provider; only generated frame/session CSV names are accepted.
The provider cannot modify captures or expose saves and game assets. Installing
the export update preserves recordings made by the earlier capture build.
New sessions never overwrite old pairs. `quest_perf.csv` retains its existing
overwrite-on-enable behavior. Captures are excluded from Git. They include
gameplay coordinates, but no device identifiers or original game assets.

## Recorded data

- Raw timings for every completed profiled app frame: monotonic start, interval
  between frame starts, local CPU time and pre/begin/step/post/end timings.
  The first interval is `-1`. Canceled frames leave frame-ID gaps.
- Raw engine phases and `phase_valid_mask`: bits follow phase columns in header
  order, from `game_ms` at bit 0 to `render_effects_ms` at bit 23. An unset bit
  means unobserved, not free work. Raw phases retain stalls over one second,
  unlike the legacy overlay's smoothing filter.
- Separate latest runtime CPU/GPU and Vulkan GPU values; `-1` means unavailable.
  GPU observations are asynchronous, can repeat and are **not attributed to that
  row's CPU frame**. Their measurement scopes differ; do not add them together.
- Actual render dimensions, effective scale and reported refresh rate.
- Per-frame active world/vegetation/vehicle/pedestrian/weapon selections:
  `0=Classic`, `1=Modern`, `2=legacy Xbox identity`. Requested selection is separate
  because model changes require a restart.
- Player presence, ped/movement activity, health/armour, wanted level, weapon/ammo,
  vehicle model, position, engine speed, and pause/cutscene/on-mission flags.
  `speed_game_units` is the engine speed magnitude, not m/s. Activity reflects
  engine state, not a headset-motion classifier.
- Entity-render call counts for vehicles, pedestrians and static objects.
  These are **not** Vulkan draw-call counts or final visible-object counts.

Session metadata includes readable asset names/source labels at capture start,
game version, profiler compilation time, UTC/monotonic clock anchors and selected
saved graphics values. Early startup metadata can precede model initialization;
per-frame active selection is authoritative. Asset names describe configured
routing, not content hashes or proof that base files are unmodified. `saved_*`
values can be `-1` when absent and can differ from active settings. Toggle full
profiling OFF/ON after changing settings for fresh metadata.

Coarse streaming/scripts/population/car/world-process/particle timers now have
producer hooks. Detailed physics channels without producer hooks remain
unsupported; the legacy summary still provides other existing diagnostics.

## Player event flags

| Value | Meaning |
|---:|---|
| 1 | Observed health or armour decrease |
| 2 | Entered dying/dead state |
| 4 | Alive after an observed death, including across temporary player absence |
| 8 / 16 | Entered / left a vehicle |
| 32 | Successful weapon activation in instrumented player Fire/drive-by paths |
| 64 / 128 | Player appeared / disappeared |
| 256 | Moved over 50 game units between adjacent valid player samples |
| 512 / 1024 / 2048 / 4096 | Pause / cutscene / mission / wanted state changed |

`event_mask` can contain several flags. `weapon_uses` counts successful
activations, not pellets, damage or hits. Direct physical-melee queries are not
counted; melee/attack ped states are still recorded. State changes occurring and
being undone within one frame can be missed. Recovery is not proof of hospital
respawn, and relocation can also be loading, teleportation or scripted movement.

## Before/after comparison

Use the same save, route, assets, settings, refresh rate and warm-up. Record at
least three repeats per build and retain matching OVR Metrics reports. Use a
controlled route without deaths for the main comparison; test combat and
death/respawn separately for transition stalls.

```text
python tools/analyze-perf-capture.py "before.frames.csv" "after.frames.csv"
```

The command prints one JSON summary per run: metadata, activity counts, events,
missing/dropped records and mean/median/p95/p99 timings. Default timing selection
requires an alive, present player outside pause/cutscenes; events cover all rows.
`--include-nongameplay` includes all states. Phase distributions require their
validity bit. GPU percentiles describe asynchronous observations, not uniquely
measured GPU frames. Frame intervals include pacing, not compositor missed counts.

Clean close adds `closed`, `written_rows`, `dropped_rows` and `write_error` to
session metadata. Incomplete/error captures are normally rejected; after a crash,
`--allow-incomplete` salvages complete rows with explicit limitations. Missing
IDs, drops and selection transitions must be checked before tail comparisons.

## Overhead and storage

A bounded 1024-frame queue feeds a background writer. The game thread uses a
nonblocking enqueue and drops samples instead of waiting on a contended/full
queue. Formatting/disk I/O run on the worker, with periodic flushing. Disabling
full profiling drains accepted samples and closes the pair; force-stop can lose
the last buffered portion. Disk errors are recorded in session metadata.

`recorder_prepare_ms` excludes enqueue, legacy logging and worker contention.
Measure total overhead with matched full-profiling ON/OFF OVR recordings.
Files grow with session length and are not automatically deleted. Disable full
profiling when finished and remove unwanted recordings locally.
