using System.Text.Json;
using Terminal.Gui.App;

namespace LibertyManager;

internal static class Program
{
    public static async Task<int> Main(string[] args)
    {
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
                Actions: BuildAndInstall, BuildVehicleEditor, ManageMusic (ImportMusic alias),
                         InstallModernModels, RepairGameFiles, Update, TransferSave, ManageMods,
                         LaunchPcvr, DiagnosePcvr.
                BuildAndInstall Target: 0 Quest (default), 1 WindowsPcvr.
                PCVR options: PcvrBuildOnly, PcvrPrepareData, PcvrGpuTests, PcvrAudioTests.
                ManageMods options: ModId "upstate-liberty", ModArchive (blank downloads v1.1.2),
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
