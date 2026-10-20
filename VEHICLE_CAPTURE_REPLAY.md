# Opt-in native vehicle capture and input replay

The harness records a player automobile through the real world/vehicle/collision
pipeline. It provides comparable input tapes and telemetry for the next wheel
and suspension investigation. It changes no normal physics rate or force law.

## Build and enable

Normal builds compile the runtime out. Build an assembled Android tree with:

```powershell
gradle --no-daemon assembleDebug -PlibertyvrVehicleCapture=true
```

The dedicated CMake option is `LIBERTYVR_VEHICLE_CAPTURE=ON`, default OFF. It is
independent of the broader `libertyvrDevTools` configuration.
Even a capture-enabled build remains inactive until an explicit command.

Install the capture-enabled APK, enter a normal player automobile, and leave
the game unpaused. The helper requires an explicit serial and verifies Quest 3:

```powershell
$adb = 'C:\LCVRBuild\.android-sdk\platform-tools\adb.exe'
.\tools\vehicle-capture.ps1 -Action Record -Serial REDACTED_QUEST_SERIAL -Label brake72 -Seconds 20 -Adb $adb
.\tools\vehicle-capture.ps1 -Action Status -Serial REDACTED_QUEST_SERIAL -Adb $adb
.\tools\vehicle-capture.ps1 -Action Stop -Serial REDACTED_QUEST_SERIAL -Adb $adb
```

Commands are published atomically in the app-owned `files/vehicle-capture/`
directory and consumed at world-frame boundaries. The helper waits up to six
seconds for acknowledgement. A paused/not-running/non-capture build can leave
the command pending; the helper reports that instead of claiming it started.
No game assets or saves are accessed by these commands.
`Status` reports the last acknowledgement, not a process heartbeat; an app kill
can leave an old status beside an incomplete tape, which replay rejects.

## Replay

Return the same vehicle model, with matching recorded handling values, to within
100 meters of the recorded starting position. Replay explicitly restores its
recorded position, orientation and linear/angular velocities using native
teleport/relinking before moving-list traversal. Native teleport also resets
suspension; friction is cleared and quiet-state history reset.

```powershell
.\tools\vehicle-capture.ps1 -Action Replay -Serial REDACTED_QUEST_SERIAL -Label brake72 -Adb $adb
```

Replay applies the recorded, already-filtered gas/brake/steering-angle/handbrake
values after normal vehicle input processing. It selects samples by accumulated
scaled/clipped game time, with zero-order hold between samples. It does not
replay raw controller poses, menus, weapons, exit actions or the whole world.
At lower simulation rates, short input transitions between updates may be
skipped. It does not force the original FPS or timestep sequence.

Traffic, pedestrians, weather, RNG, damage, tire damage, gearbox/transmission
history, wheel/suspension history and mission state are **not** restored.
Use a fresh, equivalent vehicle and repeatable location/state, and inspect
initial-state differences before attributing divergence to FPS. This is input
replay with native telemetry, not deterministic world replay or a save-state.

The session stops on exit/deletion/handle reuse, gameplay playback/cutscene mode,
model/recorded-handling change, explicit stop, duration/sample limits or errors.
Recording is limited to 1–120 game seconds and 20,000 input samples. An active
run also has a 180-second wall-time bound, so long paused/slow-motion runs can
end early. Replayed input does not continue after the tape ends. Labels allow
only 1–32 ASCII letters, digits, underscores and hyphens; recordings cannot
overwrite existing input files. Completed tapes require a validated END marker.

## Traces and analysis

Files stay local on the headset under:

```text
/sdcard/Android/data/com.libertyvr.quest/files/vehicle-capture/
  brake72.inputs.csv
  brake72.record-<monotonic-ns>.telemetry.csv
  brake72.replay-<monotonic-ns>.telemetry.csv
  status.txt
```

Input tapes contain a version, model, fingerprint of the recorded handling
scalars, initial transform/velocities and strictly increasing game timestamps.
The fingerprint covers the values printed in the handling metadata; it is not
a hash of the full handling database, collision model or APK. Telemetry includes:

- Scaled/clipped and non-clipped timestep, game timestamp, monotonic wall time,
  render frame and world processing duration before row serialization.
- Applied controls, model/handling/build metadata, health, gear and damage impulse.
- Position, full orientation basis and native linear/angular velocity.
- Each wheel's raw next-pass spring ratio, previous consumed spring ratio,
  contact grace timer, wheel state, ray point/normal/surface and moving support.
- Native narrow-phase call/body-contact totals, collision retry/probe hooks,
  planned collision substeps, quiet counter, static and stuck state.

Native velocities use engine units; multiply by 50 for m/s or rad/s. Wheel order
is front-left, rear-left, front-right, rear-right. Raw spring ratios at world
end represent the next control pass and include tire radius; previous ratios
represent the just-consumed suspension result. Ray points are zeroed when the
current raw ratio reports no hit; grace timers can retain earlier contact.
Contacts count repeated narrow-phase contacts, not unique physical impacts.
Substeps are planned counts from collision attempts, not a count of completed
solver steps. Retries include suspension/body movement probes and automobile
retry loops. These phase/counter distinctions matter when interpreting traces.

Pull closes the active session first, then copies only this diagnostics folder:

```powershell
.\tools\vehicle-capture.ps1 -Action Pull -Serial REDACTED_QUEST_SERIAL -Adb $adb
python .\tools\analyze-vehicle-capture.py <record.telemetry.csv> <replay.telemetry.csv>
```

The analyzer rejects unclosed traces and summarizes time, path between recorded
end-of-frame samples, speed, pitch/roll, contacts, retry probes and planned steps.
The path omits the unlogged initial-to-first-frame segment. The reported update
rate is updates per **game second**, not headset display FPS. Compare wall time
and the native profiler separately. Buffered file writing adds diagnostic cost;
`world_ns` excludes the current telemetry-row write and is not a complete frame
or GPU measurement. Pulls remain local; original game data stays on the device.

## Suggested first real-engine comparison

Use the same stock car/handling and a quiet, flat practice area. Start with
settled straight-line braking, then a fixed steering input, an identified curb
and a controlled landing. Record at 72 Hz, replay at the same rate to establish
repeatability, then repeat at 90/120 Hz and a controlled low-rate case. Record
actual timestep distributions rather than assuming display Hz equals physics
rate. Compare suspension/contact histories and initial damage/gear before
choosing a bounded substep prototype. No runtime substepping is enabled here.

## Validation for this task

The host runtime/tape tests cover recording and replay, temporal selection,
pose restoration, pause, duplicate control calls, maximum clipped duration,
model/handling mismatch, handle-generation changes, vehicle exit, explicit stop,
invalid/truncated tapes and file-label restrictions. Disabled hooks also link
without the runtime implementation. Native patch integration is checked by
the runner. Capture-enabled and normal arm64 builds are validated separately.
The final build uses an isolated tracked-source snapshot plus this task's
changes, excluding unrelated uncommitted files in the shared workspace.

This validation passed all 41 supported suites and 42 focused tape/session
checks, plus the disabled-hook executable. Both arm64 APK builds succeeded;
native symbol inspection confirms the capture runtime is absent from the
normal library and present in the opt-in library. The opt-in APK was installed
on the verified Quest 3 without erasing game data or saves. Logs and the
installed APK are under `.cache/vehicle-capture-integrated/`; the final full
suite report is `.cache/vehicle-capture-integrated/results.json`. The separate
normal-build log is `.cache/vehicle-capture-full-final/apk-player-build.log`.

Full in-game driving captures, cross-FPS replay accuracy and Quest logging
overhead still require headset trial runs. Host adapters establish lifecycle
behavior, not retail-car trajectory correctness.
