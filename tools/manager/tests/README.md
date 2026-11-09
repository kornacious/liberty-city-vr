# Manager verification

Run `dotnet run --project tools/manager/tests -c Release` from the repository.
File-browser checks cover the host's root/shortcut ordering, default tree
visibility and expansion, shortcut navigation and layout at 80 × 24 and 120 × 35.
They also check deep initial paths, navigation from the file list, parent navigation,
automatic tree scrolling, focus and filename/edit-selection preservation. Run just
these checks with `dotnet run --project tools/manager/tests -c Release -- --file-browser`.
Benchmark checks cover warmup exclusion, delayed GPU-sample deduplication,
mismatch/dirty/incomplete capture rejection, optional profile launch arguments,
source identities and diagnostic filename restrictions.
Asset preparation checks cover bounded inventory parsing, path/case rejection,
verified reuse/transfers, exclusion of PC-only assets, partial failure and
preservation of original data and saves using synthetic fixtures.
Native Windows core checks exercise the shared profile parser and in-memory overrides; the replay runtime
adapter checks fixed environment restoration and asset fingerprints.
This dependency-free executable checks the active C# workflows and returns a
nonzero exit code on any failure. It uses synthetic audio/IMG/provider/device
fixtures, disposable settings and temporary Git repositories. It reads no retail
data and performs no device transfers. Native source regression adapters are
available separately under **Tools and tests**.

Radio checks exercise copy-only, conversion-only and combined transfers with
fake ADB, plus failed encoding, metadata validation, duration mismatch, full
decode, upload verification and cancellation. When FFmpeg/ffprobe are available
on PATH, a synthetic IMA ADPCM station also goes through the real AAC conversion
and validation pipeline. No retail station files are used.

Compile `radio-stream-path.cpp` with a host C++17 compiler and run the result to
check the production resolver's compressed preference, WAV fallback, missing
station and dialogue behavior. For example, in a Visual Studio developer shell:
`cl /EHsc /std:c++17 tools/manager/tests/radio-stream-path.cpp /Fe.cache/radio-stream-path.exe /Fo.cache/radio-stream-path.obj`.

The checks cover settings persistence and failed-write preservation; corrupt and
unsupported settings; path/archive traversal; log redaction; binary privacy
scanning and private refs; explicit Quest 3 targeting, multiple-device selection
and missing devices; music codec/rate/channel/duration/name/provider validation
and a validation-only run that forbids device access; IMG bounds; manifest
tampering; wheel chunk validation; game-directory preconditions; version floors;
argument tokenization; auxiliary resource completeness; child-process cancellation;
and staged/history scans of synthetic Git objects.

Save-transfer fixtures cover the GTA III compatible container/checksum, both
directions, raw destination backups, corrupt/empty slots, read-back failure,
cancellation recovery, concurrent destination changes, backup failure, exact
binary ADB sync-file transport, hidden legacy entries and saved action options.
These tests never connect to a real headset or read personal save files.

Curated mod checks pin the Upstate release identity, reject malformed archive
paths/links/expansion limits and tampered payloads, and exercise production device
shell transactions against disposable local directories. They cover unsupported
runtime gating, auto-enable on supported runtimes, disable/re-enable/delete,
upload verification failures, activation rollback, unmanaged-folder preservation
and save preservation. These transaction fixtures use Git Bash on Windows and
`/bin/sh` on Linux; they never run ADB. UI checks include the Mods lifecycle
selector and Cancel behavior. See [curated mod guide](../../../overlay/docs/QUEST_MODS.md)
for the current runtime limitation and local/download workflows.

PCVR mod fixtures cover older-build gating, automatic enablement, engine selection,
disable/re-enable/delete, modified payloads and activation markers, cancelled
replacement, unmanaged directories and save/base-file preservation. PCVR UI checks
switch the Mods target and verify its isolated directory controls. Windows CTest
also checks both engine help paths and clean rejection of an absent Upstate package;
the portable Upstate package checks cover Windows drive-letter asset resolution.

Artifacts remain under `.cache/manager-tests-*` for local inspection. They must
not be committed. The synthetic privacy fixtures deliberately contain values
assembled at runtime that the scanner must reject.

In an interactive terminal, add `-- --ui PATH_TO_ARTIFACT_DIRECTORY` to run the
TUI navigation/Cancel, settings persistence and integrated file-browser checks.
They write terminal snapshots and isolated settings only; no action or device
transfer is performed.

Use `-- --ui-pcvr PATH_TO_ARTIFACT_DIRECTORY` for Windows target switching,
draft preservation, Cancel, launch and diagnostic page checks. PCVR service
fixtures also cover legacy settings, isolated paths, failed installation publication,
runtime manifests, exact launch arguments and probe cancellation without game data.

`tools/tests/test-upstate-package.cpp` checks the production native SHA256,
activation-marker and overlay-path helpers against synthetic fixtures. It also
accepts an optional local prepared-package directory to check the pinned release
fingerprints without uploading or copying assets. Run through the existing C++
host adapter with `--tool tools/tests/test-ragdoll.ps1 -- -Sources test-upstate-package.cpp`.
The engine build additionally asserts that its expanded entity, save-table, zone
and signed-path capacities are present before the Upstate library can compile.
