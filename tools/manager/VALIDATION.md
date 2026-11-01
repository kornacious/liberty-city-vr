# Manager migration verification

## Terminal-compatible music range selection (2026-11-01)

Windows Terminal reserves Shift-click for terminal text selection, preventing
that mouse input from reaching the manager. The earlier injected Shift-click
checks only established behavior when the application received those events.
The manager now offers Ctrl-click ranges and a one-shot **Range end** button;
Shift-click remains supported where a terminal forwards it.

The updated UI checks passed in Windows and Linux terminals, decoding actual
SGR mouse escape sequences before routing Ctrl-click input through the UI. They
cover forward/reverse ranges, unmodified clicks resetting selection, Range end
with an ordinary endpoint click, one-shot behavior, Escape cancellation and the
existing group movement/deletion checks. These are automated parser/UI tests,
not physical mouse tests in Windows Terminal. The unchanged manager suite passed
**56/56 Windows** and **57/57 Linux** checks; both published managers were refreshed.
No APK, device transfer or game data was needed for this manager-only fix.

## Music range selection and deletion (2026-11-01)

Verified **56/56 Windows** and **57/57 Linux** manager checks. Real-terminal UI
checks passed on both platforms for forward/reverse Shift-click ranges, plain
click collapse, group dragging and Up/Down movement, queued multi-delete,
delete-all/empty refresh and Cancel/reset. Synthetic model/client checks cover
group order, exact kept/deleted payloads, rejected-upload cleanup, unsafe or
duplicate names and validation-only skipping all headset operations.

Host Java tests passed for actual synthetic file deletion, delete-all, stale
revision/traversal rejection, deletion staging cleanup and restoring files after
an atomic playlist-write failure. The complete native/Android APK build and
Windows/Linux self-contained publishes succeeded. The updated APK was installed
on the **personal Quest 3 device**. A local read returned its existing 16-track
library successfully; no personal music was deleted or reordered for testing.

## Manage music extension (2026-11-01)

Verified **52/52 Windows** and **53/53 Linux** manager checks, plus real-terminal
UI checks on both platforms using synthetic tracks and mouse press/move/release
events. Checks cover drag insertion, selected-file details, pending Perform
options, Cancel/reset, tenth-to-second insertion, metadata fallback, paginated
library reads, changed revisions, duplicate entries and verified playlist upload
with temporary-file cleanup. Windows/Linux self-contained publishing passed.

The host Java playlist tests passed for persisted insertion order, stale revision
and duplicate rejection, newly added tracks and metadata fallback. Linux native
tests passed for shared order-file parsing, stale/missing entries, UTF-8 glyph
handling and left-to-right marquee motion, pause and repeat. The existing radio
timeline/layout regression test also passed.

The complete native/Android APK build succeeded and updated the **personal Quest
3 device** through the privacy-protected workflow. A local read of the installed
provider returned the existing 16-track library and metadata successfully. The
headset's actual playlist was not reordered and no music was uploaded for tests.
Headset visual/playback QA remains manual; the existing dashboard font supports
ASCII with common Latin accent transliteration, with `?` for other glyphs.

## Transfer save extension (2026-11-01)

The dedicated Transfer save page was verified with **47/47 Windows** and
**48/48 Linux** native manager checks. Terminal UI checks passed on both Windows
and Linux for direction/slot selection, Cancel/reset/focus, the integrated PC
folder browser (including a nonexistent proposed folder), saved settings and
hidden legacy save-transfer entries.

All transfer data was synthetic. Simulated ADB exercised the Liberty City
provider URI, binary sync files, size/SHA256 verification, destination backups,
read-back mismatch, cancellation recovery, empty/corrupt slots, concurrent slot
changes and backup failures. No personal saves were read or replaced, and no
APK/game-source changes or device deployment were needed for this manager-only
extension. A real-headset save transfer and in-game load were not performed.

## Original manager migration

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
