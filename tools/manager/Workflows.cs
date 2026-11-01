using System.Text.RegularExpressions;

namespace LibertyManager;

public sealed class Workflows(ManagerSettings settings, CommandRunner runner, PrivateLog log,
    Func<int, Task<int?>>? chooseDevice = null)
{
    private readonly Toolchain tools = new(settings, runner, log);
    public string? LastApk { get; private set; }
    private QuestDevice? connected;
    public async Task Perform(ManagerAction action, ActionOptions options, CancellationToken token)
    {
        settings.Validate();
        Paths.NoLinks(settings.Workplace);
        Directory.CreateDirectory(settings.Workplace);
        switch (action)
        {
            case ManagerAction.BuildAndInstall: await Build(options, false, token); break;
            case ManagerAction.BuildVehicleEditor: await Editor(options, token); break;
            case ManagerAction.ManageMusic:
                if (!options.ValidateMusicOnly && options.MusicRevision.Length > 0)
                    await new MusicLibraryClient(await Connect(token) ?? throw new IOException("Connect an authorized Quest 3."), settings, log).Apply(options.MusicRevision, options.MusicOrder, options.MusicDeleted, token);
                if (!string.IsNullOrWhiteSpace(options.MusicSource) || !string.IsNullOrWhiteSpace(options.BoardExport))
                    await new MusicImporter(settings, runner, log, Connect).Import(options, token);
                else if (options.MusicRevision.Length == 0) log.Write("No imports or playlist changes selected. Use Refresh to load the headset library.");
                break;
            case ManagerAction.InstallModernModels:
                var models = new ModernModels(settings, runner, log);
                var output = await models.Prepare(options, token);
                if (!options.BuildOnly)
                {
                    var device = await Connect(token) ?? throw new IOException("Models built, but no eligible Quest 3 is available for installation.");
                    await models.Install(device, output, token);
                }
                break;
            case ManagerAction.RepairGameFiles: await Repair(options, token); break;
            case ManagerAction.Update: await Update(options, token); break;
            case ManagerAction.TransferSave:
                await new SaveTransfer(settings, log, async (temporary, cancellation) =>
                {
                    var device = await Connect(cancellation) ?? throw new IOException("Save transfer requires an authorized, responsive Quest 3.");
                    return new QuestSaveSlots(device, temporary, log);
                }).Perform(options, token);
                break;
            default: throw new ArgumentOutOfRangeException(nameof(action));
        }
    }

    private async Task<QuestDevice?> Connect(CancellationToken token)
    {
        if (connected is not null) return connected;
        var adb = tools.FindAdb();
        if (adb is null) { log.Write("ADB is unavailable. Device deployment skipped."); return null; }
        return connected = await QuestDevice.Connect(runner, log, adb, chooseDevice, token);
    }
    public async Task<MusicLibrarySnapshot> MusicLibrary(CancellationToken token) => await new MusicLibraryClient(await Connect(token) ?? throw new IOException("Connect an authorized Quest 3 and press Refresh."), settings, log).Read(token);

    public async Task Build(ActionOptions options, bool updateOnly, CancellationToken token)
    {
        if (updateOnly && File.Exists(Path.Combine(settings.Repository, "tools/update-required-assets.txt")))
            throw new IOException("This version needs updated bundled assets. Use Build and install instead of Update.");
        // Validate requested retail inputs before changing anything on a device.
        var gameDirectories = options.CopyGameData && !options.BuildOnly && !updateOnly ? ValidateGameDirectory(settings.GameDirectory) : [];
        await tools.EnsureJava(token);
        await tools.EnsureSdk(options.AcceptSdkLicenses, token);
        var git = await tools.Git(token);
        var source = await tools.Re3(token);
        var assembled = Path.Combine(settings.Workplace, "liberty-city-vr-build");
        if (Directory.Exists(assembled) || File.Exists(assembled)) assembled += "-" + DateTime.Now.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N")[..6];
        log.Write("Assembling private Quest source. Previous build directories are preserved.");
        await Assemble(settings.Repository, source, assembled, git, runner, log, token);
        var gradle = await tools.Gradle(token);
        var variant = options.Release ? "release" : "debug";
        await runner.Run(tools.Java, ["-Xmx64m", "-Xms64m", "-Dorg.gradle.appname=gradle", "-classpath",
            Path.Combine(gradle, "lib/gradle-gradle-cli-main-8.13.jar"), "org.gradle.launcher.GradleMain",
            ":app:assemble" + (options.Release ? "Release" : "Debug"), "--no-daemon"], Path.Combine(assembled, "android"), token: token);
        var apk = Path.Combine(assembled, $"android/app/build/outputs/apk/{variant}/app-{variant}.apk");
        if (!File.Exists(apk)) throw new IOException("Build did not produce the expected signed APK. Configure release signing for release builds.");
        LastApk = apk;
        log.Write("APK: " + apk);
        log.Write("APK SHA256: " + Digests.Sha256(apk));
        if (options.BuildOnly) { log.Write("Build complete. Installation disabled by Build only."); return; }
        var device = await Connect(token);
        if (device is null) return;
        await device.Run(["install", "-r", apk], token);
        log.Write("APK updated on personal Quest 3 device.");
        if (updateOnly) { log.Write("Update complete. Launch the game from the headset when ready."); return; }
        await device.Bootstrap(token);
        foreach (var directory in gameDirectories)
            await PushGameTree(device, directory, Path.GetFileName(directory), token);
        await CopyPortAssets(device, Path.Combine(assembled, "gamefiles"), includeHands: true, token);
        await device.Run(["shell", "am", "force-stop", QuestDevice.Package], token);
        log.Write("Installation complete. Launch Liberty City VR from the headset.");
    }

    public static async Task Assemble(string repository, string re3, string destination, string git,
        ICommandRunner runner, PrivateLog log, CancellationToken token)
    {
        if (Directory.Exists(destination) || File.Exists(destination)) throw new IOException("Assembly destination already exists.");
        if (Paths.Within(re3, destination) || Paths.Within(destination, re3) || Paths.Within(repository, destination) || Paths.Within(destination, repository))
            throw new IOException("Assembly destination must be separate from source trees.");
        if (!File.Exists(Path.Combine(re3, "src/core/main.cpp"))) throw new IOException("The selected source is not a re3 tree.");
        Paths.CopyTree(re3, destination, token);
        // git apply --no-index applies directly to the private copy; no temporary Git metadata is needed.
        foreach (var patch in Directory.GetFiles(Path.Combine(repository, "patches"), "*.patch").Order(StringComparer.Ordinal))
        {
            log.Write("Applying " + Path.GetFileName(patch));
            await runner.Run(git, ["apply", "--no-index", "--whitespace=nowarn", Path.GetFullPath(patch)], destination, token: token);
        }
        Paths.CopyTree(Path.Combine(repository, "overlay"), destination, token);
        Paths.CopyTree(Path.Combine(repository, "librw"), Path.Combine(destination, "vendor/librw"), token);
        // Android requires a shell wrapper inside diagnostic APKs. Keep its source in the manager,
        // and generate it only in the private build product.
        var wrapper = Path.Combine(destination, "android/diagnostics/asan-wrap.sh");
        Directory.CreateDirectory(Path.GetDirectoryName(wrapper)!);
        File.WriteAllText(wrapper, AuxiliaryTools.Sources["overlay/android/diagnostics/asan-wrap.sh"]);
    }

    public static List<string> ValidateGameDirectory(string root)
    {
        if (!Directory.Exists(root)) throw new IOException("Choose your legally owned GTA III PC installation in Settings before copying game data.");
        Paths.NoLinks(root);
        var directories = Directory.GetDirectories(root).ToDictionary(p => Path.GetFileName(p), p => p, StringComparer.OrdinalIgnoreCase);
        string[] required = ["data", "TEXT", "anim", "models", "movies", "Audio"];
        if (required.Any(name => !directories.ContainsKey(name))) throw new IOException("The game folder must contain data, TEXT, anim, models, movies and Audio.");
        var audio = Directory.GetFiles(directories["Audio"]).Select(Path.GetFileName).ToHashSet(StringComparer.OrdinalIgnoreCase);
        if (!audio.Contains("sfx.raw") || !audio.Contains("sfx.sdt")) throw new IOException("Audio/sfx.RAW and sfx.SDT are required. Use a complete PC installation.");
        return required.Concat(new[] { "mp3", "skins", "txd" }.Where(directories.ContainsKey)).Select(n => directories[n]).ToList();
    }

    private static async Task PushGameTree(QuestDevice device, string source, string relative, CancellationToken token)
    {
        Paths.NoLinks(source);
        await device.Run(["shell", "content", "call", "--uri", QuestDevice.SaveUri, "--method", "mkdir", "--arg", QuestDevice.Quote(relative)], token);
        foreach (var directory in Directory.GetDirectories(source))
            await PushGameTree(device, directory, relative + "/" + Path.GetFileName(directory), token);
        foreach (var file in Directory.GetFiles(source))
        {
            Paths.NoLinks(file);
            await device.Push(file, QuestDevice.GameData + "/" + relative + "/" + Path.GetFileName(file), token);
        }
    }

    public static readonly string[] PortText = ["american.gxt", "english.gxt", "french.gxt", "german.gxt", "italian.gxt", "JAPANESE.gxt", "polish.gxt", "russian.gxt", "spanish.gxt"];
    public static readonly string[] PortModels = ["fonts_j.txd", "fonts_p.txd", "fonts_r.txd", "frontend_ds3.txd", "frontend_ds4.txd", "frontend_nsw.txd", "frontend_x360.txd", "frontend_xone.txd", "generic.txd", "menu.txd", "nswbtns.txd", "particle.txd", "ps3btns.txd", "x360btns.txd"];
    public static IEnumerable<string> PortAssets(bool includeHands) => PortText.Select(n => "TEXT/" + n).Concat(PortModels.Select(n => "models/" + n))
        .Concat(includeHands ? new[] { "BigHandLeft.uxrh", "BigHandRight.uxrh", "BigHandsAlbedo.png" }.Select(n => "models/vrhands/" + n) : []);

    private static async Task CopyPortAssets(QuestDevice device, string source, bool includeHands, CancellationToken token)
    {
        var assets = PortAssets(includeHands).ToArray();
        foreach (var relative in assets)
            if (!File.Exists(Paths.Child(source, relative))) throw new IOException("Required bundled asset is missing: " + relative);
        foreach (var relative in assets)
        {
            var remote = QuestDevice.GameData + "/" + relative;
            await device.Shell("mkdir -p " + QuestDevice.Quote(remote[..remote.LastIndexOf('/')]), token);
            await device.Push(Path.Combine(source, relative), remote, token);
        }
    }

    private async Task Repair(ActionOptions options, CancellationToken token)
    {
        string source;
        if (!string.IsNullOrWhiteSpace(options.GameFiles))
            source = Directory.Exists(Path.Combine(options.GameFiles, "gamefiles")) ? Path.Combine(options.GameFiles, "gamefiles") : options.GameFiles;
        else source = Path.Combine(await tools.Re3(token), "gamefiles");
        foreach (var relative in PortAssets(false))
            if (!File.Exists(Paths.Child(source, relative))) throw new IOException("Selected gamefiles directory lacks " + relative);
        var device = await Connect(token) ?? throw new IOException("Repair needs an authorized Quest 3.");
        await device.Shell("test -f " + QuestDevice.Quote(QuestDevice.GameData + "/models/gta3.img"), token);
        await device.Bootstrap(token);
        await device.Run(["shell", "am", "force-stop", QuestDevice.Package], token);
        await CopyPortAssets(device, source, false, token);
        log.Write("Port text and frontend files repaired. Launch Liberty City VR again.");
    }

    private async Task Editor(ActionOptions options, CancellationToken token)
    {
        var cmake = tools.FindCMake();
        await WindowsCompiler.Configure(runner, token);
        var bin = Path.GetDirectoryName(cmake)!;
        var ctest = Path.Combine(bin, "ctest" + (OperatingSystem.IsWindows() ? ".exe" : ""));
        var cpack = Path.Combine(bin, "cpack" + (OperatingSystem.IsWindows() ? ".exe" : ""));
        var build = Path.Combine(settings.Workplace, OperatingSystem.IsWindows() ? "vehicle-editor-build" : "vehicle-editor-linux-build");
        var cache = Path.Combine(build, "CMakeCache.txt");
        if (File.Exists(cache) && !File.ReadAllText(cache).Contains("CMAKE_GENERATOR:INTERNAL=Ninja", StringComparison.Ordinal))
        {
            build = Path.Combine(settings.Workplace, "vehicle-editor-ninja-build");
            if (File.Exists(Path.Combine(build, "CMakeCache.txt")) && !File.ReadAllText(Path.Combine(build, "CMakeCache.txt")).Contains("CMAKE_GENERATOR:INTERNAL=Ninja", StringComparison.Ordinal))
                throw new IOException("The alternate editor build uses another generator. Preserve it and select a different workplace.");
            log.Write("Preserving the existing editor generator configuration; using " + build);
        }
        var output = string.IsNullOrWhiteSpace(options.EditorOutput) ? Path.Combine(settings.Workplace, "vehicle-editor") : Path.GetFullPath(options.EditorOutput);
        if (Paths.Within(settings.Repository, output) || Paths.Within(output, settings.Repository) || Paths.Within(build, output) || Paths.Within(output, build))
            throw new IOException("Editor output must be separate from the repository and its build directory.");
        Paths.NoLinks(build); Paths.NoLinks(output);
        List<string> arguments = ["-S", Path.Combine(settings.Repository, "tools/vehicle-editor"), "-B", build, "-DCMAKE_BUILD_TYPE=Release"];
        var managerRuntime = Path.Combine(settings.Workplace, "editor-manager-runtime-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(managerRuntime);
        foreach (var file in Directory.GetFiles(AppContext.BaseDirectory).Where(p => !p.EndsWith(".pdb", StringComparison.OrdinalIgnoreCase)))
            File.Copy(file, Path.Combine(managerRuntime, Path.GetFileName(file)));
        foreach (var directory in Directory.GetDirectories(AppContext.BaseDirectory).Where(d => Directory.GetFiles(d, "*.resources.dll").Length > 0))
            Paths.CopyTree(directory, Path.Combine(managerRuntime, Path.GetFileName(directory)), token);
        arguments.Add("-DVEHICLE_EDITOR_MANAGER_RUNTIME=" + managerRuntime);
        if (!File.Exists(Path.Combine(build, "CMakeCache.txt")))
            arguments.AddRange(["-G", "Ninja"]);
        var ninja = Path.Combine(bin, "ninja" + (OperatingSystem.IsWindows() ? ".exe" : ""));
        if (File.Exists(ninja)) arguments.Add("-DCMAKE_MAKE_PROGRAM=" + ninja);
        await runner.Run(cmake, arguments, token: token);
        await runner.Run(cmake, ["--build", build, "--config", "Release", "--target", "liberty-vehicle-editor", "placement-tests", "placement-runtime-tests", "placement-process-tests", "--parallel"], token: token);
        await runner.Run(ctest, ["--test-dir", build, "-C", "Release", "--output-on-failure"], token: token);
        await runner.Run(cmake, ["--install", build, "--config", "Release", "--prefix", output], token: token);
        if (options.PackageEditor) await runner.Run(cpack, ["-G", "ZIP", "-C", "Release"], build, token: token);
        log.Write("Vehicle editor installed: " + output);
    }

    private async Task Update(ActionOptions options, CancellationToken token)
    {
        const string official = "https://github.com/kornacious/liberty-city-vr-quest.git";
        var git = await tools.Git(token);
        var source = File.Exists(Path.Combine(settings.Repository, ".git")) || Directory.Exists(Path.Combine(settings.Repository, ".git"))
            ? settings.Repository : Path.Combine(settings.Workplace, "source-kit");
        async Task<string> Git(params string[] args) => (await runner.Run(git, new[] { "-C", source }.Concat(args), showOutput: false, token: token)).Output.Trim();
        async Task RequireClean()
        {
            if (await Git("status", "--porcelain", "--untracked-files=no") != "") throw new IOException("Tracked local changes prevent updating. Commit or preserve your work; nothing was reset or stashed.");
        }
        var floor = Math.Max(520, ReadVersion(File.ReadAllText(Path.Combine(settings.Repository, "overlay/android/app/build.gradle.kts"))));
        if (!Directory.Exists(source))
        {
            if (options.UpdateDryRun) { log.Write("Dry run: would clone official master into the workplace. No network, build or installation."); return; }
            await runner.Run(git, ["clone", "--single-branch", "--branch", "master", official, source], token: token);
        }
        if (!Path.GetFullPath(await Git("rev-parse", "--show-toplevel")).Equals(Path.GetFullPath(source), Paths.Comparison)) throw new IOException("Update source must be the root of its own Git checkout.");
        if (await Git("symbolic-ref", "--short", "HEAD") != "master") throw new IOException("Update requires the master branch. Your branch was left untouched.");
        await RequireClean();
        var before = await Git("rev-parse", "HEAD");
        floor = Math.Max(floor, ReadVersion(await Git("show", before + ":overlay/android/app/build.gradle.kts")));
        if (options.UpdateDryRun) { log.Write($"Dry run: clean master; version floor {floor}. No network, build or installation."); return; }
        await Git("fetch", "--no-tags", official, "refs/heads/master");
        var fetched = await Git("rev-parse", "FETCH_HEAD");
        var version = ReadVersion(await Git("show", fetched + ":overlay/android/app/build.gradle.kts"));
        if (version < floor) throw new IOException("Official master is older than your source kit. Update stopped.");
        var assetMarker = await runner.Run(git, ["-C", source, "cat-file", "-e", fetched + ":tools/update-required-assets.txt"], showOutput: false, check: false, token: token);
        if (assetMarker.ExitCode == 0) throw new IOException("This update requires bundled assets. Use Build and install for that version.");
        await Git("merge-base", "--is-ancestor", before, fetched);
        await RequireClean();
        if (await Git("rev-parse", "HEAD") != before) throw new IOException("HEAD changed during update. Retry.");
        await Git("merge", "--ff-only", "--no-autostash", "--no-overwrite-ignore", fetched);
        await RequireClean();
        if (await Git("rev-parse", "HEAD") != fetched) throw new IOException("Updated checkout differs from verified commit.");
        var project = Path.Combine(source, "tools/manager/LibertyManager.csproj");
        if (!File.Exists(project)) throw new IOException("Updated kit does not contain the integrated manager. Source updated; no APK was built or installed.");
        var dotnet = Paths.FindExecutable("dotnet") ?? throw new IOException("Install the .NET 10 SDK to rebuild the updated manager.");
        var updated = Path.Combine(settings.Workplace, "manager-updates", fetched);
        await runner.Run(dotnet, ["build", project, "-c", "Release", "-o", updated], token: token);
        var request = Path.Combine(settings.Workplace, ".update-request-" + Guid.NewGuid().ToString("N") + ".json");
        try
        {
            File.WriteAllText(request, System.Text.Json.JsonSerializer.Serialize(new RunRequest(settings with { Repository = source }, options with { CopyGameData = false })));
            await runner.Run(dotnet, [Path.Combine(updated, "LibertyManager.dll"), "--update-build", request], token: token);
        }
        finally { if (File.Exists(request)) File.Delete(request); }
        log.Write("Update complete. Restart the manager to load the updated application.");
    }

    public static int ReadVersion(string source)
    {
        var matches = Regex.Matches(source, @"(?m)^\s*versionCode\s*=\s*(\d+)\s*$");
        if (matches.Count != 1 || !int.TryParse(matches[0].Groups[1].Value, out var version)) throw new InvalidDataException("Cannot read a unique numeric versionCode.");
        return version;
    }
}

public sealed record RunRequest(ManagerSettings Settings, ActionOptions Options);
