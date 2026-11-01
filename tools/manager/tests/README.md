# Manager verification

Run `dotnet run --project tools/manager/tests -c Release` from the repository.
This dependency-free executable checks the active C# workflows and returns a
nonzero exit code on any failure. It uses synthetic audio/IMG/provider/device
fixtures, disposable settings and temporary Git repositories. It reads no retail
data and performs no device transfers. Native source regression adapters are
available separately under **Tools and tests**.

The checks cover settings persistence and failed-write preservation; corrupt and
unsupported settings; path/archive traversal; log redaction; binary privacy
scanning and private refs; explicit Quest 3 targeting, multiple-device selection
and missing devices; music codec/rate/channel/duration/name/provider validation
and a validation-only run that forbids device access; IMG bounds; manifest
tampering; wheel chunk validation; game-directory preconditions; version floors;
argument tokenization; auxiliary resource completeness; child-process cancellation;
and staged/history scans of synthetic Git objects.

Artifacts remain under `.cache/manager-tests-*` for local inspection. They must
not be committed. The synthetic privacy fixtures deliberately contain values
assembled at runtime that the scanner must reject.

In an interactive terminal, add `-- --ui PATH_TO_ARTIFACT_DIRECTORY` to run the
TUI navigation/Cancel, settings persistence and integrated file-browser checks.
They write terminal snapshots and isolated settings only; no action or device
transfer is performed.
