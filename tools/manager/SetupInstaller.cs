namespace LibertyManager;

public enum InstallStage { CheckingSetup, PreparingGameFiles, BuildingGame, ConnectingHeadset, InstallingApplication, InstallingGameFiles }
public enum InstallStatus { Installed, HeadsetPending, Cancelled, Failed }
public enum SetupCheckStatus { Ready, Automatic, NeedsAttention }

public sealed record InstallProgress(InstallStage Stage, int Completed = 0, int Total = 0, bool CountingFiles = false);
public sealed record InstallResult(InstallStatus Status, InstallStage Stage, bool Built, string Message, string? InstallFolder = null);
public sealed record SetupCheck(string Title, string Message, SetupCheckStatus Status);
public sealed record SetupReadiness(IReadOnlyList<SetupCheck> Checks, bool NeedsAndroidConsent)
{
    public bool CanInstall => Checks.All(check => check.Status != SetupCheckStatus.NeedsAttention);
}

internal sealed class InstallRun(IProgress<InstallProgress>? progress)
{
    public InstallStage Stage { get; private set; }
    public bool Built { get; set; }
    public bool Installed { get; set; }
    public bool ShortcutFailed { get; set; }
    public string? OutputPath { get; set; }

    public void Report(InstallStage stage, int completed = 0, int total = 0, bool countingFiles = false)
    {
        Stage = stage;
        progress?.Report(new(stage, completed, total, countingFiles));
    }
}

public sealed record SetupSelection(GameTarget Target, string? PcFolder, string? XboxImage,
    bool AcceptAndroidLicenses = false, bool? UseReleaseSigning = null);

public sealed class SetupInstaller(ManagerSettings settings, PrivateLog log)
{
    public static GameTarget DesktopTarget => PcvrWorkflow.HostTarget;

    private (ManagerSettings Settings, ActionOptions Options) Request(SetupSelection selection)
    {
        var quest = selection.Target == GameTarget.Quest;
        var pc = !string.IsNullOrWhiteSpace(selection.PcFolder);
        var xbox = !string.IsNullOrWhiteSpace(selection.XboxImage);
        var release = selection.UseReleaseSigning ?? (settings.Actions.TryGetValue(ManagerAction.BuildAndInstall, out var existing)
            && existing.Target == GameTarget.Quest && existing.Release);
        return (settings with { GameDirectory = selection.PcFolder ?? settings.GameDirectory }, new ActionOptions
        {
            Target = selection.Target,
            InstallPcVariant = pc,
            InstallXboxVariant = xbox,
            XboxIsoPath = selection.XboxImage ?? "",
            CacheAstcTextures = quest && (pc || xbox),
            ConvertRadioStations = quest && pc,
            AcceptSdkLicenses = quest && selection.AcceptAndroidLicenses,
            PcvrDesktopShortcut = !quest,
            Release = quest && release
        });
    }

    public static void CheckGameFiles(SetupSelection selection)
    {
        if (string.IsNullOrWhiteSpace(selection.PcFolder) && string.IsNullOrWhiteSpace(selection.XboxImage))
            throw new InvalidDataException("Choose the folder for your PC copy, the disc image for your Original Xbox copy, or both.");
        if (!string.IsNullOrWhiteSpace(selection.PcFolder))
            Workflows.ValidateGameDirectory(selection.PcFolder);
        if (!string.IsNullOrWhiteSpace(selection.XboxImage))
        {
            using var image = new XboxIso(selection.XboxImage);
            foreach (var (name, entry) in OriginalXboxModels.Disc.Files)
                if (!image.Files.TryGetValue(name, out var file) || file.Length != entry.Bytes)
                    throw new InvalidDataException("Choose a supported GTA III Original Xbox US 1.0 disc image.");
        }
    }

    public async Task<SetupReadiness> Check(SetupSelection selection, CancellationToken token)
    {
        var checks = new List<SetupCheck>();
        var (local, options) = Request(selection);
        var runner = new CommandRunner(log) { CommandTimeout = TimeSpan.FromSeconds(30) };
        var tools = new Toolchain(local, runner, log);
        var quest = selection.Target == GameTarget.Quest;
        var needsConsent = false;
        void Add(string title, string message, SetupCheckStatus status = SetupCheckStatus.Ready) =>
            checks.Add(new(title, message, status));
        async Task Probe(string title, Func<Task> action)
        {
            token.ThrowIfCancellationRequested();
            try { await action(); }
            catch (OperationCanceledException) when (token.IsCancellationRequested) { throw; }
            catch (Exception error)
            {
                Add(title, error is OperationCanceledException ? "The check timed out. Check your tools and try again." : log.Sanitize(error.Message),
                    SetupCheckStatus.NeedsAttention);
            }
        }

        await Probe("Source kit and build folder", () =>
        {
            if (System.Runtime.InteropServices.RuntimeInformation.ProcessArchitecture != System.Runtime.InteropServices.Architecture.X64 ||
                (!OperatingSystem.IsWindows() && !OperatingSystem.IsLinux()) || (!quest && selection.Target != DesktopTarget))
                throw new PlatformNotSupportedException("Choose a target that can be installed from this Windows or Linux x64 computer.");
            local.Validate();
            Paths.NoLinks(local.Workplace);
            foreach (var source in new[] { selection.PcFolder, selection.XboxImage })
                if (!string.IsNullOrWhiteSpace(source) && (Paths.Within(source, local.Workplace) || Paths.Within(local.Workplace, source)))
                    throw new IOException("Choose a build folder in Setup help that is separate from your original game files.");
            if (File.Exists(local.Workplace))
                throw new IOException("Choose a folder for local builds in Setup help. The current location is a file.");
            Add("Source kit and build folder", "Found the source kit and a separate location for local builds.");
            return Task.CompletedTask;
        });
        if (checks.Any(check => check.Status == SetupCheckStatus.NeedsAttention)) return new(checks, false);

        await Probe("Your game files", () =>
        {
            CheckGameFiles(selection);
            Add("Your game files", "Found the selected files. Setup verifies them as it prepares the game.");
            return Task.CompletedTask;
        });
        if (tools.HasGit) Add("Git", "Found.");
        else if (OperatingSystem.IsWindows() && local.AllowToolDownloads)
            Add("Git", "Setup can use or download a verified copy.", SetupCheckStatus.Automatic);
        else if (!OperatingSystem.IsWindows()) Add("Git", "Install Git with your Linux package manager, then check again.", SetupCheckStatus.NeedsAttention);
        else Add("Git", "Install Git or allow verified downloads in the existing manager.", SetupCheckStatus.NeedsAttention);

        if (quest)
        {
            var javaFound = tools.HasJava;
            Add("Java build tools", javaFound ? "Found." : "Setup can obtain a verified Java 21 download.",
                javaFound ? SetupCheckStatus.Ready : SetupCheckStatus.Automatic);
            if (!javaFound && (!string.IsNullOrWhiteSpace(local.JavaHome) || !local.AllowToolDownloads))
                checks[^1] = new("Java build tools", "Choose a JDK 21 folder in Setup help or clear it to use the verified download.", SetupCheckStatus.NeedsAttention);
            needsConsent = tools.NeedsSdkPackages;
            Add("Android build tools", needsConsent ? "Setup can install the required components after you accept Google’s license terms." : "Found.",
                needsConsent ? SetupCheckStatus.Automatic : SetupCheckStatus.Ready);
            Add("Quest 3", "Enable Developer Mode, connect the headset by USB and accept the USB debugging prompt. Setup checks the connection after the local build.",
                SetupCheckStatus.Automatic);
            if (options.InstallXboxVariant)
                await Probe("Xbox conversion tools", async () =>
                {
                    if (OperatingSystem.IsWindows()) await WindowsCompiler.Configure(runner, token);
                    else if (Paths.FindExecutable("c++") is null) throw new IOException("Install GCC or Clang C++ tools, then check again.");
                    if (!needsConsent || !string.IsNullOrWhiteSpace(local.CMake))
                    {
                        var cmake = tools.FindCMake();
                        if (tools.FindNinja(cmake) is null)
                            throw new IOException("Install Ninja or select a CMake installation that includes it.");
                        if (!File.Exists(Path.Combine(Path.GetDirectoryName(cmake)!, OperatingSystem.IsWindows() ? "ctest.exe" : "ctest")))
                            throw new IOException("Choose a CMake installation that includes CTest.");
                    }
                    Add("Xbox conversion tools", "Setup uses CMake, Ninja and your computer’s C++ compiler to prepare the disc image.",
                        SetupCheckStatus.Automatic);
                });
        }
        else
            await Probe("PCVR build tools", async () =>
            {
                var ready = await new PcvrWorkflow(local, runner, log, selection.Target).CheckSetupTools(token);
                Add("PCVR build tools", ready ? "Found the compiler, build tools and desktop libraries." :
                    "Setup builds the desktop libraries using verified source downloads. See Setup help for Linux development packages.",
                    ready ? SetupCheckStatus.Ready : SetupCheckStatus.Automatic);
            });

        if (quest || options.InstallXboxVariant)
            await Probe("Game file preparation", async () =>
            {
                if (options.ConvertRadioStations || options.InstallXboxVariant)
                    foreach (var (name, configured) in new[] { ("FFmpeg", local.Ffmpeg), ("ffprobe", local.Ffprobe) })
                    {
                        var executable = Paths.FindExecutable(configured);
                        if (executable is null)
                            throw new IOException($"Install {name} or select its executable in Setup help, then check again.");
                        if ((await runner.Run(executable, ["-version"], showOutput: false, check: false, token: token)).ExitCode != 0)
                            throw new IOException($"{name} did not start successfully. Check its installation and try again.");
                    }
                if (quest && !tools.CanUseAstcEncoder)
                    throw new IOException("Choose the required game-file preparation tool in Setup help or allow verified tool downloads.");
                Add("Game file preparation", "Setup prepares a separate copy. Your original files stay in place.",
                    quest ? SetupCheckStatus.Automatic : SetupCheckStatus.Ready);
            });
        return new(checks, needsConsent);
    }

    public async Task<InstallResult> Install(SetupSelection selection, IProgress<InstallProgress>? progress, CancellationToken token)
    {
        var run = new InstallRun(progress);
        run.Report(InstallStage.CheckingSetup);
        InstallResult Result(InstallStatus status, string message) => new(status, run.Stage, run.Built, message, run.OutputPath);
        try
        {
            var readiness = await Check(selection, token);
            var blocker = readiness.Checks.FirstOrDefault(check => check.Status == SetupCheckStatus.NeedsAttention);
            if (blocker is not null) return Result(InstallStatus.Failed, blocker.Message);
            if (readiness.NeedsAndroidConsent && !selection.AcceptAndroidLicenses)
                return Result(InstallStatus.Failed, "Accept the Android SDK license terms on the setup checklist before installing.");
            var (local, options) = Request(selection);
            Directory.CreateDirectory(local.Workplace);
            // Setup windows sharing a workplace must not install over each other.
            var lockPath = Path.Combine(local.Workplace, ".graphical-setup.lock");
            Paths.NoLinks(lockPath);
            using var installLock = new FileStream(lockPath, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None);
            var runner = new CommandRunner(log);
            if (options.Target == GameTarget.Quest)
            {
                var workflow = new Workflows(local, runner, log, _ => throw new SeveralHeadsetsException());
                await workflow.Build(options, false, token, run);
                if (!run.Installed)
                    return Result(InstallStatus.HeadsetPending, "Connect your Quest 3, accept USB debugging inside the headset, then retry.");
                return Result(InstallStatus.Installed, "Open the app library on your Quest 3, choose Unknown Sources, and open Liberty City VR.");
            }
            await new PcvrWorkflow(local, runner, log, options.Target).Build(options, token, run);
            return Result(InstallStatus.Installed, run.ShortcutFailed
                ? "Open the install folder to start the game. Setup could not create a desktop shortcut."
                : "Connect your headset through your PCVR software, then open Liberty City VR from your desktop.");
        }
        catch (SeveralHeadsetsException)
        {
            return Result(InstallStatus.HeadsetPending, "More than one Quest 3 is connected. Leave only the headset you want to use connected, then retry.");
        }
        catch (OperationCanceledException) when (token.IsCancellationRequested)
        {
            return Result(InstallStatus.Cancelled, "Setup stopped. You can retry with the same game files.");
        }
        catch (Exception error)
        {
            var detail = log.Sanitize(error.Message);
            log.Write(detail);
            var message = detail;
            if (selection.Target == GameTarget.Quest && run.Stage == InstallStage.InstallingApplication)
                message += " Check that your Quest 3 is connected and unlocked.";
            if (OperatingSystem.IsWindows() && run.Stage == InstallStage.BuildingGame)
                message += " If the log shows a path-length error, choose a short Build folder in Setup help and try again.";
            return Result(InstallStatus.Failed, message);
        }
    }

    private sealed class SeveralHeadsetsException : Exception;
}
