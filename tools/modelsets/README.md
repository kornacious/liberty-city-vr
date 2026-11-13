# Local Original Xbox asset conversion

Select Install Original Xbox variant under the manager's Build and install action
with your own supported GTA III US 1.0 ISO. This prepares a complete standalone
variant under `variants/xbox`, independently of PC data. `XboxIso.cs` reads XDVDFS directly; no extract-xiso, 7-Zip, Python or
external ISO extractor is invoked. `OriginalXboxModels.cs` validates the retail
manifest and builds the C++17 converter with CMake, librw and lzokay. CTest runs
the synthetic LZO validation test. Only source, hashes and sizes ship here.

See [TOOL_NOTICE.md](TOOL_NOTICE.md) for attribution and
[the asset guide](../../overlay/docs/QUEST_MODELSETS.md) for supported inputs.
IMG inspection/injection and TXD merge auxiliary modules remain available through
`--list-tools`; they need their configured Python/PowerShell runtime. The old
downloaded HD-pack builder and compressor are retired.
