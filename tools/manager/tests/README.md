# Manager verification

Run `dotnet run --project tools/manager/tests -c Release` from the repository.
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

Artifacts remain under `.cache/manager-tests-*` for local inspection. They must
not be committed. The synthetic privacy fixtures deliberately contain values
assembled at runtime that the scanner must reject.

In an interactive terminal, add `-- --ui PATH_TO_ARTIFACT_DIRECTORY` to run the
TUI navigation/Cancel, settings persistence and integrated file-browser checks.
They write terminal snapshots and isolated settings only; no action or device
transfer is performed.
