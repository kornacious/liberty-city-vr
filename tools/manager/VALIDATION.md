# Manager migration verification

Validated locally on Windows and Arch Linux through WSL, 2026-11-01. Original
game inputs remained local throughout. No Git push or cloud deployment was used.

| Check | Result |
| --- | --- |
| .NET 10 manager build | Passed; zero compiler warnings/errors |
| Native manager checks, Windows | 29/29 passed |
| Native manager checks, Linux | 30/30 passed, including a Linux symlink check |
| Terminal UI | Navigation, Cancel/reset/focus, settings persistence and integrated directory browser passed in a real Linux terminal |
| Windows/Linux self-contained publishing | Passed |
| Fresh Quest APK through native workflow | Build succeeded; updated personal Quest 3 device; bundled asset transfers verified |
| Native Modern builder | Verified the existing local pinned source, converted 937 TXDs, repaired six wheel references, built and validated a fresh overlay; no model deployment requested for this check |
| Vehicle editor | Build, four CTest suites, installation and ZIP packaging passed |
| Embedded device bridge | All 14 synthetic tests passed |
| Standalone script removal | All 169 previously tracked `.ps1`, `.sh` and `.py` files integrated into the resource and removed from the source tree |

The broad embedded current-source runner completed with **57 passed, 7 failed**,
and seven explicitly excluded reference-only fixtures. The failures are in
older adapters that no longer match the current production declarations:

- physical-scope: missing draw-profile adapter hook
- vulkan-descriptors: missing static-instance flush adapter hook
- vulkan-pipelines: missing instanced-world shader enum
- xr-focus-input: missing replay-view state member
- quest-melee: missing model-set category adapter
- ragdoll-menu: production/fixture interface mismatch
- vehicle-seat-recenter: placement/anchor adapter interface mismatch

Those failures are not counted as passes and have not been fixed by replacing
production behavior or removing the tests. The adapters remain selectable through
the manager. An initial run under a very long Windows workplace also hit native
compiler path limits; the totals above are from the short-workplace rerun.

Auxiliary modules retain their Python/PowerShell/Bash implementations and runtime
dependencies. Native manager tests validate the active C# workflows; historical
installer tests exercise compatibility source fixtures. Not every specialised
auxiliary tool, optional download, signing configuration or device action has
been exercised end to end. Headset visual/gameplay QA was not performed.
