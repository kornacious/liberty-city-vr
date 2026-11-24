using System.IO.Compression;
using LibertyManager;

internal static class SetupChecks
{
    private static void Require(bool condition)
    {
        if (!condition) throw new Exception("Setup assertion failed.");
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
