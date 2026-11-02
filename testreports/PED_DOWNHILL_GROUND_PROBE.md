# Downhill ground contact

The player could leave the ground while sprinting down ramps or stairs. The
standing ground line extended below the feet by `0.15 * timeStep`, where the
legacy timestep uses 50 units per second. This gives 25 cm at 30 FPS, 8.33 cm
at 90 FPS, and only 4.17 cm in a half-frame player collision check at 90 FPS.
A stair tread can therefore disappear from the standing probe at high FPS.
Sprint movement also crosses more downhill distance before the next probe.
Losing support enters the existing airborne/collision recovery paths, a
plausible source of the reported launch and subsequent fall damage.

`QuestPedGroundProbe::Drop` keeps a minimum 25 cm probe reach on Android with
FIX_BUGS. Larger low-FPS sweeps retain the original calculation. Existing
`bWasStanding` checks still govern extending the ground line; this does not
change gravity or disable falling damage. The same tolerance is used by the
cached polygon check, ground line and collision broad phase.

Validation on this computer:

- MSVC C++17 regression: 312 checks passed at 30, 60, 72, 90 and 120 FPS,
  with one through four collision substeps. The old calculation misses
  support in 77 tested stair/ramp cases. Low-FPS sweep and larger ledge
  boundaries are covered. These are numerical probe fixtures, not map replays.
- Full Android arm64 debug APK built successfully with the complete patch
  series. Existing compiler/SDK/Gradle warnings remain.
- APK updated on personal Quest 3 device using the manager's verified-target
  update workflow. Existing game data and saves were preserved.
- The optional clean-LF portability check is blocked by an existing failure
  in patch 46 (`src/renderer/Renderer.cpp`); its line-ending check also flags
  seven pre-existing CRLF patches. The new patch passes its line-ending
  check and the full local Windows assembly/build succeeded.

In-game confirmation is still required: sprint downhill from the Portland
first safehouse to the street and down the Staunton dock/shipyard stairs.
Compare normal running and sprinting, especially at 72/90/120 Hz. Also check
intentional jumps, a drop larger than 25 cm, uphill movement and moving boats.
No original game data was needed for the automated checks.

Run the native fixture through the existing embedded host-test adapter:

```text
dotnet run --project tools/manager -c Release -- --tool tools/tests/test-ragdoll.ps1 -- -Sources test-ped-ground-probe.cpp
```
