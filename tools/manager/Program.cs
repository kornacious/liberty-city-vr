using System.Text.Json;
using Terminal.Gui.App;

namespace LibertyManager;

internal static class Program
{
    public static async Task<int> Main(string[] args)
    {
        if (args is ["--build-pcvr-setup", var settingsPath, var outputPath])
        {
            using var log = new PrivateLog();
            log.Line += Console.Error.WriteLine;
            try
            {
                var settings = JsonSerializer.Deserialize<ManagerSettings>(File.ReadAllText(settingsPath))
                    ?? throw new InvalidDataException("Missing PCVR release build settings.");
                Paths.RequireRepository(settings.Repository);
                if (!Path.IsPathFullyQualified(settings.Workplace))
                    throw new IOException("Choose an absolute PCVR release workspace.");
                Paths.NoLinks(settings.Workplace);
                var output = Path.GetFullPath(outputPath);
                if (Directory.Exists(output) || File.Exists(output))
                    throw new IOException("Use a fresh PCVR release staging directory.");
                Directory.CreateDirectory(output);
                var runner = new CommandRunner(log);
                var host = PcvrWorkflow.HostTarget;
                var app = Paths.Child(output, "app");
                var assets = await new PcvrWorkflow(settings, runner, log, host).BuildPortablePackage(app, default);
                PortablePcvrSetup.ValidateApp(app, host);
                PortablePortAssets.Write(settings.Repository, assets, Paths.Child(output, PortablePortAssets.Manifest));
                Console.WriteLine(await new OriginalXboxModels(settings, runner, log).Converter(default));
                return 0;
            }
            catch (Exception error) { Console.Error.WriteLine(log.Sanitize(error.Message)); return 1; }
        }
        if (args.Length >= 2 && args[0] == "--privacy")
        {
            try
            {
                await new PrivacyGuard(ManagerSettings.FindRepository()).Run(args[1], args.ElementAtOrDefault(2));
                Console.WriteLine("Privacy check passed."); return 0;
            }
            catch { Console.Error.WriteLine("Privacy check failed. Sensitive content and diagnostic values are suppressed."); return 1; }
        }
        if (args is ["--help"] or ["-h"])
        {
            Console.WriteLine("""
                Liberty City VR Manager (.NET 10 / Terminal.Gui 2.5.0)
                Run with no arguments to open the TUI.
                --run ACTION REQUEST.json   Run a configured action without the TUI.
                --assemble RE3 OUTPUT       Assemble patched sources locally without building or accessing a device.
                --prepare-windows-data GTA OUTPUT  Prepare an isolated local PCVR data copy; never overwrites a directory.
                --benchmark-compare LEFT.run.csv RIGHT.run.csv  Compare local matched captures; JSON, exit 2 on mismatch.
                --benchmark-collect OUTPUT  Copy recent Quest profiles/routes/captures into a new local folder.
                --benchmark-inventory OUTPUT  Compare local Quest/PCVR asset hashes into a new diagnostic folder.
                --benchmark-align-quest OUTPUT  Prepare a new isolated PCVR benchmark copy matching local Quest assets.
                Actions: BuildAndInstall, BuildVehicleEditor, ManageMusic (ImportMusic alias),
                         InstallModernModels, RepairGameFiles, Update, TransferSave, ManageMods,
                         LaunchPcvr, DiagnosePcvr.
                BuildAndInstall Target: 0 Quest (default), 1 WindowsPcvr, 2 LinuxPcvr.
                Variant options: InstallPcVariant, InstallXboxVariant, XboxIsoPath.
                AllowUnverifiedXboxIso opts into importing a trusted US GTA III image without retail fingerprints.
                Both variants may be selected. PC uses Settings.GameDirectory.
                Leave both unchecked for an application-only build/update.
                PCVR options: PcvrBuildOnly, PcvrGpuTests, PcvrAudioTests.
                ManageMods options: ModId "upstate-liberty", ModArchive (blank downloads v1.1.2),
                                    Target 0 Quest (default), 1 WindowsPcvr, 2 LinuxPcvr,
                                    ModOperation 0 Status, 1 Install, 2 Enable, 3 Disable,
                                                 4 Delete, 5 Validate locally (no device access).
                TransferSave options: SaveDirection 0 (Quest to PC) or 1 (PC to Quest),
                                      SaveSlots [1..9], PcSaveDirectory (9 = Autosave).
                                      Legacy SaveSlot selects one slot when SaveSlots is omitted.
                Request JSON: {"Settings":{...},"Options":{...}}
                --settings PATH            Use another local settings directory.
                --page ACTION              Open an action page (or Settings/Tools).
                --list-tools               List embedded auxiliary tools and test adapters.
                --tool NAME [-- ARGS...]    Run an integrated auxiliary tool.
                --privacy MODE [FILE]      staged, message, history, pre-push, install-hooks.
                Auxiliary modules require their Python 3, PowerShell 7 or Bash runtime.
                """);
            return 0;
        }
        using var cancellation = new CancellationTokenSource();
        Console.CancelKeyPress += (_, e) => { e.Cancel = true; cancellation.Cancel(); };
        try
        {
            if(args.Length is 2 or 3 && args[0] is "--benchmark-inventory" or "--benchmark-align-quest")
            {
                var benchmarkMode=args[0];var benchmarkDestination=args[1];
                var assetSettings=args.Length==3 ? (JsonSerializer.Deserialize<RunRequest>(File.ReadAllText(args[2]))??throw new InvalidDataException("Invalid benchmark settings request.")).Settings : new SettingsStore().Load();
                assetSettings.Validate();using var assetLog=new PrivateLog();assetLog.Line+=Console.WriteLine;
                var assetRunner=new CommandRunner(assetLog);
                var assetAdb=new Toolchain(assetSettings,assetRunner,assetLog).FindAdb() ?? throw new IOException("Configure ADB for local benchmark preparation.");
                var assetDevice=await QuestDevice.Connect(assetRunner,assetLog,assetAdb,null,cancellation.Token) ?? throw new IOException("Connect the personal Quest 3 device.");
                var reference=await BenchmarkAssets.Inventory(assetDevice,cancellation.Token);
                var assetPaths=PcvrPaths.From(assetSettings,PcvrWorkflow.HostTarget);assetPaths.Validate(assetSettings);
                if(benchmarkMode=="--benchmark-inventory") {
                    var destination=Path.GetFullPath(benchmarkDestination);Paths.NoLinks(destination);
                    if(Directory.Exists(destination) || File.Exists(destination))throw new IOException("Choose a new inventory output directory.");
                    var differences=BenchmarkAssets.Compare(reference,BenchmarkAssets.LocalInventory(assetPaths.Data));Directory.CreateDirectory(destination);
                    File.WriteAllText(Paths.Child(destination,"quest-inventory.json"),JsonSerializer.Serialize(reference,new JsonSerializerOptions{WriteIndented=true}));
                    File.WriteAllText(Paths.Child(destination,"differences.json"),JsonSerializer.Serialize(differences,new JsonSerializerOptions{WriteIndented=true}));
                    assetLog.Write($"Compared local asset inventories: {differences.Length} differences. No game assets or saves transferred.");
                } else {
                    var setup=await BenchmarkAssets.Prepare(assetSettings,benchmarkDestination,reference,(asset,path,token)=>BenchmarkAssets.CopyFromDevice(assetDevice,asset,path,token),cancellation.Token);
                    assetLog.Write($"Prepared isolated benchmark data: {setup.LocalCopies} local copies, {setup.DeviceCopies} verified headset copies. Existing data and saves preserved.");
                }
                return 0;
            }
            if(args is ["--benchmark-collect",var benchmarkOutput])
            {
                var benchmarkSettings=new SettingsStore().Load();using var log=new PrivateLog();log.Line+=Console.WriteLine;
                var runner=new CommandRunner(log);
                var adb=new Toolchain(benchmarkSettings,runner,log).FindAdb() ?? throw new IOException("Configure ADB to collect local Quest diagnostics.");
                var device=await QuestDevice.Connect(runner,log,adb,null,cancellation.Token) ?? throw new IOException("Connect the personal Quest 3 device.");
                await BenchmarkTransfer.Collect(device,benchmarkOutput,log,cancellation.Token);return 0;
            }
            if(args is ["--benchmark-compare",var leftRun,var rightRun])
            {
                var comparison=BenchmarkReports.Compare(leftRun,rightRun);
                Console.WriteLine(JsonSerializer.Serialize(comparison,new JsonSerializerOptions { WriteIndented=true }));
                return comparison.WorkloadMatched?0:2;
            }
            if (args is ["--prepare-windows-data", var retailSource, var dataOutput])
            {
                Workflows.PrepareWindowsData(ManagerSettings.FindRepository(), Path.GetFullPath(retailSource),
                    Path.GetFullPath(dataOutput), cancellation.Token);
                Console.WriteLine("Windows game data prepared locally; original files and saves preserved.");
                return 0;
            }
            if (args is ["--assemble", var re3Source, var outputDirectory])
            {
                using var log = new PrivateLog(); log.Line += Console.WriteLine;
                var runner = new CommandRunner(log);
                var git = Paths.FindExecutable("git") ?? throw new IOException("Git is required for source assembly.");
                var source = Path.GetFullPath(re3Source);
                var head = await runner.Run(git, ["-C", source, "rev-parse", "HEAD"], showOutput: false, token: cancellation.Token);
                var changes = await runner.Run(git, ["-C", source, "status", "--porcelain"], showOutput: false, token: cancellation.Token);
                if (head.Output.Trim() != Toolchain.Re3Commit || changes.Output.Trim().Length != 0)
                    throw new IOException("Source assembly requires the clean pinned public re3 checkout. Existing sources were not changed.");
                await Workflows.Assemble(ManagerSettings.FindRepository(), Path.GetFullPath(re3Source),
                    Path.GetFullPath(outputDirectory), git, runner, log, cancellation.Token);
                log.Write("Source assembly complete. No game data or device access.");
                return 0;
            }
            if (args is ["--list-tools"])
            {
                foreach (var tool in AuxiliaryTools.Catalog) Console.WriteLine(tool.Category + "  " + tool.Path);
                return 0;
            }
            if (args.Length >= 2 && args[0] is "--tool" or "--vehicle-bridge")
            {
                var toolSettings = new SettingsStore().Load();
                using var log = new PrivateLog(); log.Line += Console.WriteLine;
                var host = new AuxiliaryTools(toolSettings, new CommandRunner(log), log);
                if (args[0] == "--vehicle-bridge") await host.Bridge(args.Skip(1).ToArray(), cancellation.Token);
                else await host.Run(args[1], args.Skip(args.ElementAtOrDefault(2) == "--" ? 3 : 2).ToArray(), cancellation.Token);
                return 0;
            }
            if (args is ["--run", var name, var file] && Enum.TryParse<ManagerAction>(name, true, out var action))
            {
                var request = JsonSerializer.Deserialize<RunRequest>(File.ReadAllText(file)) ?? throw new InvalidDataException("Invalid action request.");
                using var log = new PrivateLog(); log.Line += Console.WriteLine;
                await new Workflows(request.Settings, new CommandRunner(log), log).Perform(action, request.Options, cancellation.Token);
                return 0;
            }
            if (args is ["--update-build", var requestFile])
            {
                var request = JsonSerializer.Deserialize<RunRequest>(File.ReadAllText(requestFile)) ?? throw new InvalidDataException("Invalid update request.");
                request.Settings.Validate();
                using var log = new PrivateLog(); log.Line += Console.WriteLine;
                await new Workflows(request.Settings, new CommandRunner(log), log).Build(request.Options, true, cancellation.Token);
                return 0;
            }
            var page = 0;
            SaveTransferDirection? saveDirection = null;
            if (args is ["--ui-check", _, var checkPage] && (!int.TryParse(checkPage, out page) || page is < 0 or > ManagerUi.LastPage)) throw new ArgumentException("Invalid check page.");
            if (args is ["--page", var pageName])
            {
                if ((pageName.Equals(nameof(SaveTransferDirection.PcToQuest), StringComparison.OrdinalIgnoreCase) || pageName.Equals(nameof(SaveTransferDirection.QuestToPc), StringComparison.OrdinalIgnoreCase)) && Enum.TryParse<SaveTransferDirection>(pageName, true, out var direction))
                { page = (int)ManagerAction.TransferSave; saveDirection = direction; }
                else page = pageName.Equals("Settings", StringComparison.OrdinalIgnoreCase) ? ManagerUi.SettingsPage : pageName.Equals("Tools", StringComparison.OrdinalIgnoreCase) ? ManagerUi.ToolsPage
                    : Enum.TryParse<ManagerAction>(pageName, true, out var parsedPage) && Enum.IsDefined(parsedPage) ? (int)parsedPage : throw new ArgumentException("Unknown page.");
            }
            var store = args is ["--settings", var directory] ? new SettingsStore(directory) : new SettingsStore();
            if (args.Length > 0 && args is not ["--settings", _] and not ["--page", _] and not ["--ui-check", _] and not ["--ui-check", _, _]) throw new ArgumentException("Unknown arguments. Use --help.");
            var settings = store.Load();
            if (saveDirection is { } preset)
            {
                settings.Actions.TryGetValue(ManagerAction.TransferSave, out var savedTransfer);
                settings.Actions[ManagerAction.TransferSave] = (savedTransfer ?? new ActionOptions()) with { SaveDirection = preset };
            }
            using var app = Application.Create().Init();
            using var ui = new ManagerUi(app, store, settings);
            ui.Initialize(page);
            if (args.Length >= 2 && args[0] == "--ui-check")
            {
                app.AddTimeout(TimeSpan.FromMilliseconds(800), () => { File.WriteAllText(args[1], app.ToString()); app.RequestStop(); return false; });
            }
            app.Run(ui.Window);
            return 0;
        }
        catch (OperationCanceledException) { Console.Error.WriteLine("Action cancelled."); return 130; }
        catch (Exception error)
        {
            using var log = new PrivateLog();
            Console.Error.WriteLine(log.Sanitize("Manager failed: " + error.Message));
            return 1;
        }
    }
}
