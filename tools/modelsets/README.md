# Local Original Xbox asset conversion

Use the manager's Original Xbox model action with your own supported GTA III
US 1.0 ISO. `XboxIso.cs` reads XDVDFS directly; no extract-xiso, 7-Zip, Python or
external ISO extractor is invoked. `OriginalXboxModels.cs` validates the retail
manifest and builds the C++17 converter with CMake, librw and lzokay. CTest runs
the synthetic LZO validation test. Only source, hashes and sizes ship here.

See [TOOL_NOTICE.md](TOOL_NOTICE.md) for attribution and
[the asset guide](../../overlay/docs/QUEST_MODELSETS.md) for supported inputs.
IMG inspection/injection and TXD merge auxiliary modules remain available through
`--list-tools`; they need their configured Python/PowerShell runtime. The old
downloaded HD-pack builder and compressor are retired.
