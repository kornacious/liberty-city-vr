using System.IO.Compression;
using LibertyManager;

internal static class SetupChecks
{
    private static void Require(bool condition, [System.Runtime.CompilerServices.CallerArgumentExpression(nameof(condition))] string? assertion = null)
    {
        if (!condition) throw new Exception("Setup assertion failed: " + assertion);
    }

    private static async Task Reject<T>(Func<Task> action) where T : Exception
    {
        try { await action(); }
        catch (T) { return; }
        throw new Exception("Expected " + typeof(T).Name);
    }

    public static async Task Run(string root, Func<string, Func<Task>, Task> test)
    {
        var repository = Path.Combine(root, "setup-kit");
        var workplace = Path.Combine(root, "setup-work");
        var settings = new ManagerSettings { Repository = repository, Workplace = workplace, AllowToolDownloads = false };
        using var log = new PrivateLog();
        var runner = new ProbeRunner();
        await test("Setup rejects changed Upstate archives and supports supplied or verified cached files", async () =>
        {
            var directory = Directory.CreateDirectory(Path.Combine(workplace, "mods")).FullName;
            var archive = Path.Combine(directory, "upstate-liberty-1.1.2.7z");
            var supplied = Path.Combine(root, "UL.7z");
            File.WriteAllText(supplied, "archive fixture");
            var mod = CuratedMods.Upstate with { Bytes = new FileInfo(supplied).Length, Sha256 = Digests.Sha256(supplied) };
            var mods = new CuratedMods(settings, runner, log);
            await Reject<IOException>(() => Task.Run(() => mods.CheckSetupArchive(mod, null)));
            Directory.CreateDirectory(Path.Combine(directory, "upstate-liberty-1.1.2"));
            Require(mods.CheckSetupArchive(mod, supplied));
            File.Copy(supplied, archive);
            Require(mods.CheckSetupArchive(mod, null));
            File.WriteAllText(supplied, "changed");
            await Reject<InvalidDataException>(() => Task.Run(() => mods.CheckSetupArchive(mod, supplied)));
            Require(File.ReadAllText(supplied) == "changed");
        });
        await test("Bundled Xbox converter uses host package, verified checksum and both self-tests", async () =>
        {
            var folder = Directory.CreateDirectory(Path.Combine(repository, "Setup/tools", SetupKitTools.HostRid)).FullName;
            var converter = Path.Combine(folder, OperatingSystem.IsWindows() ? "xbox-convert.exe" : "xbox-convert");
            Require(SetupKitTools.XboxConverter(repository) is null);
            File.WriteAllText(converter, "converter fixture");
            await Reject<InvalidDataException>(() => Task.Run(() => SetupKitTools.XboxConverter(repository)));
            File.WriteAllText(converter + ".sha256", Digests.Sha256(converter));
            Require(SetupKitTools.XboxConverter(repository) == converter);
            runner.Working.Add(converter);
            await SetupKitTools.TestXboxConverter(runner, converter, default);
            Require(runner.Calls.TakeLast(2).SequenceEqual(["--self-test", "--head-uv-test"]));
            File.AppendAllText(converter, "changed");
            await Reject<InvalidDataException>(() => Task.Run(() => SetupKitTools.XboxConverter(repository)));
        });
        await test("Setup FFmpeg handles missing or broken tools, disabled downloads and cached retry", async () =>
        {
            var configured = Path.Combine(root, "configured-ffmpeg");
            File.WriteAllText(configured, "broken fixture");
            var tools = new FfmpegTools(settings with { Ffmpeg = configured }, runner, log);
            await Reject<IOException>(() => tools.Check(default));
            var downloadsEnabled = new FfmpegTools(settings with { AllowToolDownloads = true, Ffmpeg = configured }, runner, log);
            Require(!await downloadsEnabled.Check(default));
            var package = "ffmpeg-n9.0.2-62-gac1c6e003f-" + (OperatingSystem.IsWindows() ? "win64" : "linux64") + "-lgpl-9.0";
            var bin = Directory.CreateDirectory(Path.Combine(workplace, ".tools", package, "bin")).FullName;
            var suffix = OperatingSystem.IsWindows() ? ".exe" : "";
            var ffmpeg = Path.Combine(bin, "ffmpeg" + suffix);
            var ffprobe = Path.Combine(bin, "ffprobe" + suffix);
            foreach (var path in new[] { ffmpeg, ffprobe })
            {
                File.WriteAllText(path, "working fixture");
                runner.Working.Add(path);
            }
            Require(await tools.Check(default));
            var resolved = await tools.Resolve(default);
            Require(resolved.Ffmpeg == ffmpeg && resolved.Ffprobe == ffprobe);
            runner.Working.Add(configured);
            Require((await tools.Resolve(default)).Ffmpeg == configured);
            runner.Working.Remove(ffprobe);
            var downloads = Directory.CreateDirectory(Path.Combine(workplace, ".downloads")).FullName;
            File.WriteAllText(Path.Combine(downloads, package + (OperatingSystem.IsWindows() ? ".zip" : ".tar.xz")), "invalid archive");
            await Reject<InvalidDataException>(() => tools.Resolve(default));
            using var cancelled = new CancellationTokenSource();
            cancelled.Cancel();
            await Reject<OperationCanceledException>(() => tools.Resolve(cancelled.Token));
        });
        await test("Setup Quest activation requires a full dual-engine APK", async () =>
        {
            var path = Path.Combine(root, "setup-apk-fixture.apk");
            using (var archive = ZipFile.Open(path, ZipArchiveMode.Create))
            using (var engine = archive.CreateEntry("lib/arm64-v8a/liblibertyvr.so").Open())
                engine.WriteByte(1);
            await Reject<InvalidDataException>(() => Task.Run(() => Workflows.RequireUpstateApk(path)));
            using (var archive = ZipFile.Open(path, ZipArchiveMode.Update))
                foreach (var name in new[] { "classic", "upstate" })
                {
                    using var engine = archive.CreateEntry($"lib/arm64-v8a/liblibertyvr_{name}.so").Open();
                    engine.WriteByte(1);
                }
            Workflows.RequireUpstateApk(path);
        });
        await test("Portable PCVR Setup copies a legacy install, recovers interrupted publication and relocates", async () =>
        {
            var kit = Path.Combine(root, "portable-kit");
            var host = PcvrWorkflow.HostTarget;
            var suffix = OperatingSystem.IsWindows() ? ".exe" : "";
            var bundle = Directory.CreateDirectory(Path.Combine(kit, "PCVR", SetupKitTools.HostRid, "app")).FullName;
            var legacy = Path.Combine(root, "portable-legacy");
            var oldApp = Directory.CreateDirectory(Path.Combine(legacy, "app", "current")).FullName;
            var oldData = Directory.CreateDirectory(Path.Combine(legacy, "data")).FullName;
            var oldUser = Directory.CreateDirectory(Path.Combine(legacy, "user", "userfiles")).Parent!.FullName;
            var install = Path.GetFullPath(Path.Combine(root, "portable-installed"));
            var android = Directory.CreateDirectory(Path.Combine(kit, "overlay/android/app/src/main/assets")).FullName;
            Directory.CreateDirectory(Path.Combine(kit, "patches"));
            Directory.CreateDirectory(Path.Combine(kit, "overlay/gamefiles"));
            File.WriteAllText(Path.Combine(kit, "overlay/android/app/build.gradle.kts"), "");
            File.WriteAllText(Path.Combine(android, "vehicle_placements.bundled.ini"), "bundled");
            foreach (var notice in new[] { "LICENSE", "NOTICE.md" }) File.WriteAllText(Path.Combine(kit, notice), notice);
            var required = OperatingSystem.IsWindows()
                ? new[] { "openxr_loader.dll", "openxr-notices/LICENSE", "openxr-notices/JsonCpp-LICENSE" }
                : new[] { "libopenxr_loader.so.1", "desktop-backend.txt", "libSDL3.so.0", "libavformat.so.63",
                    "libavcodec.so.63", "libavutil.so.61", "libswresample.so.7", "dependency-notices/SDL_LICENSE.txt",
                    "dependency-notices/FFMPEG_LICENSE.txt", "dependency-notices/config.mak" };
            foreach (var app in new[] { bundle, oldApp })
                foreach (var name in required.Concat(["libertyvr-game" + suffix, "IMGUI_LICENSE.txt", "LIBRW_LICENSE.txt"]))
                {
                    var path = Path.Combine(app, name);
                    Directory.CreateDirectory(Path.GetDirectoryName(path)!);
                    File.WriteAllText(path, name);
                }
            File.WriteAllText(Path.Combine(oldApp, "libertyvr-pcvr" + suffix), "");
            PcvrBuilds.WritePaths(oldApp, oldApp, oldData, oldUser);
            foreach (var mode in new[] { "classic", "upstate" }) File.WriteAllText(Path.Combine(bundle, PcvrMods.PortableEngine(host, mode)), "");
            File.WriteAllText(Path.Combine(bundle, PcvrMods.Capabilities), "upstate-liberty/1\nportable-engine/1\n");
            File.WriteAllText(Path.Combine(oldData, ".libertyvr-pcvr-data"), "");
            File.WriteAllText(Path.Combine(oldData, "vr_settings.ini"), "saved preference");
            File.WriteAllText(Path.Combine(oldUser, "userfiles/save.b"), "saved game");
            foreach (var name in VariantInstaller.Required)
            {
                var path = Path.Combine(oldData, "variants/pc", name);
                Directory.CreateDirectory(Path.GetDirectoryName(path)!);
                File.WriteAllText(path, name);
            }
            VariantInstaller.Seal(Path.Combine(oldData, "variants/pc"), "pc");
            var setup = new SetupInstaller(settings with { Repository = kit, PcvrInstallDirectory = oldApp, LinuxPcvrInstallDirectory = oldApp }, log);
            var selection = new SetupSelection(host, null, null, InstallRoot: install, LegacyInstall: legacy);
            var attempts = await Task.WhenAll(
                Task.Run(() => setup.Install(selection, null, default)),
                Task.Run(() => setup.Install(selection, null, default)));
            Require(attempts.Count(attempt => attempt.Status == InstallStatus.Installed) == 1);
            var result = attempts.Single(attempt => attempt.Status == InstallStatus.Installed);
            if (result is not { Status: InstallStatus.Installed, Built: false } || result.InstallFolder != install)
                throw new Exception(result.Message);
            Require(Directory.GetDirectories(install).Select(Path.GetFileName).Order().SequenceEqual(["app", "data", "user"]));
            Require(File.ReadAllText(Path.Combine(install, "user/configs/vr_settings.ini")) == "saved preference");
            Require(File.ReadAllText(Path.Combine(install, "user/userfiles/save.b")) == "saved game");
            Require(File.Exists(Path.Combine(oldData, "vr_settings.ini")) && !File.Exists(Path.Combine(install, "data/vr_settings.ini")));
            var scratch = Directory.CreateDirectory(Path.Combine(install, "tools")).FullName;
            File.WriteAllText(Path.Combine(scratch, ".setup-owned"), host.ToString());
            File.WriteAllText(Path.Combine(scratch, "publishing"), "");
            File.WriteAllText(Path.Combine(scratch, "moving-app"), "");
            Directory.CreateDirectory(Path.Combine(scratch, "previous"));
            Directory.CreateDirectory(Path.Combine(scratch, "incoming"));
            Directory.Move(Path.Combine(install, "app"), Path.Combine(scratch, "previous/app"));
            Directory.CreateDirectory(Path.Combine(install, "app"));
            var update = await setup.Install(selection with { LegacyInstall = null }, null, default);
            if (update.Status != InstallStatus.Installed) throw new Exception(update.Message);
            Require(!Directory.Exists(scratch) && File.ReadAllText(Path.Combine(install, "user/userfiles/save.b")) == "saved game");
            var relocated = Path.GetFullPath(Path.Combine(root, "portable-relocated"));
            Directory.Move(install, relocated);
            Require(PcvrBuilds.ReadPaths(Path.Combine(relocated, "app")) ==
                (Path.Combine(relocated, "data"), Path.Combine(relocated, "user")));
        });
    }

    private sealed class ProbeRunner : ICommandRunner
    {
        public HashSet<string> Working { get; } = [];
        public List<string> Calls { get; } = [];
        public Task<CommandResult> Run(string executable, IEnumerable<string> args, string? workingDirectory = null,
            bool showOutput = true, bool check = true, CancellationToken token = default, string? input = null)
        {
            token.ThrowIfCancellationRequested();
            Calls.Add(args.Single());
            return Task.FromResult(new CommandResult(Working.Contains(executable) ? 0 : 1, ""));
        }
    }
}
