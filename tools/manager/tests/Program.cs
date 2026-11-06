using System.Buffers.Binary;
using System.IO.Compression;
using System.Text;
using System.Text.Json;
using LibertyManager;

if (args is ["--ui", var uiDirectory]) return UiChecks.Run(uiDirectory);

if (args is ["--probe-noise"])
{
    Console.Error.WriteLine("[mp3 @ synthetic] probe diagnostic");
    Console.WriteLine("{");
    Console.Error.WriteLine("[mp3 @ synthetic] another diagnostic");
    Console.WriteLine("\"format\": {\"duration\": \"10\"}}");
    return 0;
}

if (args is ["--child"])
{
    Console.WriteLine("child ready"); await Task.Delay(TimeSpan.FromMinutes(2)); return 0;
}

var root = Path.Combine(ManagerSettings.FindRepository(), ".cache/manager-tests-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(root);
var failures = 0; var total = 0;
async Task Test(string name, Func<Task> body)
{
    total++;
    try { await body(); Console.WriteLine("PASS " + name); }
    catch (Exception error) { failures++; Console.WriteLine("FAIL " + name + ": " + error.GetType().Name + " / " + error.Message); }
}
void Assert(bool condition, string message = "Assertion failed") { if (!condition) throw new Exception(message); }
void Throws<T>(Action body) where T : Exception
{ try { body(); } catch (T) { return; } throw new Exception("Expected " + typeof(T).Name); }
async Task ThrowsAsync<T>(Func<Task> body) where T : Exception
{ try { await body(); } catch (T) { return; } throw new Exception("Expected " + typeof(T).Name); }
var repository = Path.Combine(root, "source");
Directory.CreateDirectory(Path.Combine(repository, "patches")); Directory.CreateDirectory(Path.Combine(repository, "overlay/android/app"));
File.WriteAllText(Path.Combine(repository, "overlay/android/app/build.gradle.kts"), "versionCode = 600\n");
var config = new ManagerSettings { Repository = repository, Workplace = Path.Combine(root, "work") };
const string goodProbe = """{"streams":[{"codec_type":"audio","codec_name":"mp3","channels":2,"sample_rate":"44100"}],"format":{"duration":"10.2"}}""";

await Test("Windows and Linux workplace defaults", () =>
{ Assert(ManagerSettings.DefaultWorkplace(true) == @"C:\LCVRBuild"); Assert(ManagerSettings.DefaultWorkplace(false).EndsWith("LCVRBuild")); return Task.CompletedTask; });
await Test("Settings survive save/load without device identifiers", () =>
{
    var store = new SettingsStore(Path.Combine(root, "settings")); store.Save(config); var actual = store.Load();
    Assert(actual.Workplace == config.Workplace && actual.Repository == config.Repository);
    Assert(!File.ReadAllText(store.FilePath).Contains("serial", StringComparison.OrdinalIgnoreCase)); return Task.CompletedTask;
});
await Test("Invalid settings do not replace the saved file", () =>
{
    var store = new SettingsStore(Path.Combine(root, "atomic-settings")); store.Save(config); var before = File.ReadAllText(store.FilePath);
    Throws<InvalidDataException>(() => store.Save(config with { Workplace = repository })); Assert(File.ReadAllText(store.FilePath) == before); return Task.CompletedTask;
});
await Test("Corrupt settings fail without silently resetting", () =>
{
    var store = new SettingsStore(Path.Combine(root, "bad-settings")); Directory.CreateDirectory(store.DirectoryPath); File.WriteAllText(store.FilePath, "{bad");
    Throws<JsonException>(() => store.Load()); return Task.CompletedTask;
});
await Test("Reject unsupported settings version", () => { Throws<InvalidDataException>(() => (config with { Version = 2 }).Validate()); return Task.CompletedTask; });
await Test("Reject relative and overlapping workplaces", () =>
{
    Throws<InvalidDataException>(() => (config with { Workplace = "relative" }).Validate());
    Throws<InvalidDataException>(() => (config with { Workplace = Path.Combine(repository, "work") }).Validate());
    Throws<InvalidDataException>(() => (config with { Workplace = root }).Validate()); return Task.CompletedTask;
});
await Test("Reject traversal and absolute archive paths", () =>
{
    foreach (var path in new[] { "../escape", "a/../../escape", "/escape", "a\\escape", "a//b" }) Throws<InvalidDataException>(() => Paths.Child(root, path));
    Assert(Paths.Within(root, Paths.Child(root, "safe/file.txt"))); return Task.CompletedTask;
});
await Test("ZIP extraction rejects traversal", () =>
{
    var path = Path.Combine(root, "bad.zip"); using (var archive = ZipFile.Open(path, ZipArchiveMode.Create)) archive.CreateEntry("../outside");
    Throws<InvalidDataException>(() => Toolchain.ExtractZip(path, Path.Combine(root, "extract"), default)); Assert(!File.Exists(Path.Combine(root, "outside"))); return Task.CompletedTask;
});
if (!OperatingSystem.IsWindows())
    await Test("Linked directory traversal is rejected", () =>
    {
        var target = Path.Combine(root, "link-target"); Directory.CreateDirectory(target);
        var link = Path.Combine(root, "linked"); Directory.CreateSymbolicLink(link, target);
        Throws<InvalidDataException>(() => Paths.NoLinks(link));
        Throws<InvalidDataException>(() => Paths.Child(root, "linked/file")); return Task.CompletedTask;
    });
await Test("Private logs redact known identifiers and terminal escapes", () =>
{
    using var log = new PrivateLog(); log.Remember("synthetic-device-token");
    var device = "2G" + new string('X', 12); var host = "fixture.example." + "ts.net";
    var text = log.Sanitize("synthetic-device-token " + device + " " + host + "\u001b[31mhello");
    Assert(!text.Contains("synthetic-device-token") && !text.Contains(device) && !text.Contains(host) && !text.Contains('\u001b')); return Task.CompletedTask;
});
await Test("Privacy scanner blocks device patterns in binary blobs", () =>
{
    var bytes = Encoding.ASCII.GetBytes("\0" + "2G" + new string('X', 12) + "\0");
    Throws<InvalidDataException>(() => new PrivacyGuard(repository).Check(bytes)); return Task.CompletedTask;
});
await Test("Private denylist matching ignores ASCII case", () =>
{ Assert(PrivacyGuard.ContainsPrivate([0, 65, 66, 99, 0], [97, 98, 67])); Assert(!PrivacyGuard.ContainsPrivate([65], [65, 66])); return Task.CompletedTask; });
await Test("Quest selection verifies model and hides discovery output", async () =>
{
    using var log = new PrivateLog(); var runner = new FakeRunner(); var device = await QuestDevice.Connect(runner, log, "adb", null, default);
    Assert(device is not null); Assert(runner.Calls.All(c => !c.Show));
    await device!.Run(["install", "-r", "personal.apk"]);
    var args = runner.Calls.Last().Args; Assert(args[0] == "-s" && args[1] == "synthetic-eligible" && args[2] == "install");
    Assert(log.Sanitize("synthetic-eligible synthetic-offline") == "personal Quest 3 device personal Quest 3 device");
});
await Test("Multiple eligible devices require explicit selection", async () =>
{
    using var log = new PrivateLog(); var runner = new FakeRunner { Multiple = true };
    await ThrowsAsync<InvalidOperationException>(async () => await QuestDevice.Connect(runner, log, "adb", null, default));
    var device = await QuestDevice.Connect(runner, log, "adb", n => Task.FromResult<int?>(n - 1), default);
    Assert(device is not null); await device!.Run(["install", "-r", "personal.apk"]); Assert(runner.Calls.Last().Args[1] == "synthetic-other");
});
await Test("No eligible Quest skips deployment", async () =>
{
    using var log = new PrivateLog(); var runner = new FakeRunner { WrongModel = true };
    Assert(await QuestDevice.Connect(runner, log, "adb", null, default) is null);
    Assert(!runner.Calls.Any(c => c.Args.Contains("install")));
});
await Test("Music duration validation uses invariant decimal parsing", () =>
{ Assert(MusicImporter.ValidateProbe(".mp3", 32, goodProbe) == 10200); return Task.CompletedTask; });
await Test("Music rejects surround channels, wrong codec and excessive size", () =>
{
    Throws<InvalidDataException>(() => MusicImporter.ValidateProbe(".mp3", 32, goodProbe.Replace("\"channels\":2", "\"channels\":6")));
    Throws<InvalidDataException>(() => MusicImporter.ValidateProbe(".flac", 32, goodProbe));
    Throws<InvalidDataException>(() => MusicImporter.ValidateProbe(".mp3", 513L * 1024 * 1024, goodProbe)); return Task.CompletedTask;
});
await Test("Music rejects non-finite or zero duration", () =>
{
    Throws<InvalidDataException>(() => MusicImporter.ValidateProbe(".mp3", 32, goodProbe.Replace("10.2", "NaN")));
    Throws<InvalidDataException>(() => MusicImporter.ValidateProbe(".mp3", 32, goodProbe.Replace("10.2", "0"))); return Task.CompletedTask;
});
await Test("Music safe names are deterministic and shell-safe", () =>
{
    var name = MusicImporter.SafeName("_Tést & song.mp3", new string('A', 64));
    Assert(name == "Test-song-aaaaaaaaaaaaaaaa.mp3"); return Task.CompletedTask;
});
await Test("Provider parsing accepts Bundle wrapping and rejects errors", () =>
{
    var result = MusicImporter.ParseProvider("Result: Bundle[{payload={\"ok\":true,\"count\":1}}]"); Assert(result.GetProperty("count").GetInt32() == 1);
    Throws<IOException>(() => MusicImporter.ParseProvider("Result: Bundle[{\"ok\":false,\"error\":\"failure\"}]"));
    Throws<IOException>(() => MusicImporter.ParseProvider("no provider")); return Task.CompletedTask;
});
await Test("Music validation-only never queries ADB", async () =>
{
    var file = Path.Combine(root, "sample.mp3"); File.WriteAllText(file, "synthetic fixture");
    var runner = new FakeRunner { Probe = goodProbe }; using var log = new PrivateLog();
    await new MusicImporter(config, runner, log, _ => throw new Exception("Unexpected headset access")).Import(new() { MusicSource = file, ValidateMusicOnly = true }, default);
    Assert(runner.Calls.Count == 1 && runner.Calls[0].Args.Contains("-show_streams"));
});
await Test("Explicit music selection imports only chosen files and supports mixed folder/file input", async () =>
{
    var folder = Path.GetFullPath(Path.Combine(root,"selected-music")); Directory.CreateDirectory(folder);
    var a = Path.Combine(folder,"A.mp3"); var b = Path.Combine(folder,"B.mp3"); var c = Path.Combine(folder,"C.mp3");
    File.WriteAllText(a,"a"); File.WriteAllText(b,"b"); File.WriteAllText(c,"c"); File.WriteAllText(Path.Combine(folder,"ignored.txt"),"text");
    var options = new ActionOptions { MusicSource = folder, MusicSources = [a,c], ValidateMusicOnly = true };
    Assert(MusicImporter.SourcePaths(options).SequenceEqual(new[] {a,c}));
    var runner = new FakeRunner { Probe = goodProbe }; using var log = new PrivateLog();
    await new MusicImporter(config,runner,log,_ => throw new Exception("Unexpected headset access")).Import(options,default);
    Assert(runner.Calls.Count == 2 && runner.Calls.All(call => call.Args[^1] == a || call.Args[^1] == c));
    Assert(MusicImporter.SourcePaths(new() { MusicSources = [a,folder,c] }).Order().SequenceEqual(new[] {a,b,c}.Order()));
    Assert(MusicImporter.SourcePaths(new() { MusicSource = folder }).Length == 3);
});
await Test("Malformed IMG directories and out-of-range entries fail", () =>
{
    var img = Path.Combine(root, "bad.img"); var dir = Path.ChangeExtension(img, ".dir"); File.WriteAllBytes(img, new byte[2048]); File.WriteAllBytes(dir, [1]);
    Throws<InvalidDataException>(() => ModernModels.ReadImg(img, dir).ToArray());
    var bytes = new byte[32]; BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(4), 2); Encoding.ASCII.GetBytes("car.dff").CopyTo(bytes, 8); File.WriteAllBytes(dir, bytes);
    Throws<InvalidDataException>(() => ModernModels.ReadImg(img, dir).ToArray()); return Task.CompletedTask;
});
await Test("Overlay manifests detect modified and unmanifested files", () =>
{
    var overlay = Path.Combine(root, "modern"); Directory.CreateDirectory(Path.Combine(overlay, "models/generic")); Directory.CreateDirectory(Path.Combine(overlay, "data"));
    var img = Path.Combine(overlay, "models/gta3.img"); File.WriteAllBytes(img, new byte[2048]);
    var dir = new byte[32]; BinaryPrimitives.WriteUInt32LittleEndian(dir.AsSpan(4), 1); Encoding.ASCII.GetBytes("car.dff").CopyTo(dir, 8); File.WriteAllBytes(Path.Combine(overlay, "models/gta3.dir"), dir);
    foreach (var p in new[] { "models/generic/player.bmp", "vegetation_models.txt", "vehicle_models.txt", "asset_categories.json", "models/generic.txd", "data/default.ide" }) File.WriteAllText(Path.Combine(overlay, p), "fixture");
    var manifest = new List<string> { "Profile=MODERN", "BuilderVersion=" + OriginalXboxModels.Disc.BuilderVersion, "Source=" + OriginalXboxModels.Identity };
    foreach (var file in Directory.GetFiles(overlay, "*", SearchOption.AllDirectories)) manifest.Add("SHA256 " + Path.GetRelativePath(overlay, file).Replace('\\', '/') + "=" + Digests.Sha256(file));
    File.WriteAllLines(Path.Combine(overlay, "BUILD_INFO.txt"), manifest); Assert(ModernModels.ValidateOverlay(overlay).Count == 8);
    File.WriteAllText(Path.Combine(overlay, "extra.txt"), "extra"); Throws<InvalidDataException>(() => ModernModels.ValidateOverlay(overlay)); File.Delete(Path.Combine(overlay, "extra.txt"));
    File.AppendAllText(Path.Combine(overlay, "vegetation_models.txt"), "tampered"); Throws<InvalidDataException>(() => ModernModels.ValidateOverlay(overlay)); return Task.CompletedTask;
});
await Test("Original Xbox import rejects downloads and extracted mod sources before tools or devices", async () =>
{
    var fake = new FakeRunner(); using var log = new PrivateLog();
    var models = new OriginalXboxModels(config, fake, log);
    await ThrowsAsync<InvalidDataException>(() => models.Prepare(new() { DownloadModels = true }, default));
    await ThrowsAsync<InvalidDataException>(() => models.Prepare(new() { ModelSource = "mod-folder" }, default));
    await ThrowsAsync<IOException>(() => models.Prepare(new(), default));
    Assert(fake.Calls.Count == 0);
});
await XboxChecks.Run(Test, Assert, root);

await Test("Game data copy rejects incomplete retail folders", () =>
{ Throws<IOException>(() => Workflows.ValidateGameDirectory(root)); return Task.CompletedTask; });
await Test("Version parsing rejects missing and duplicate versionCode", () =>
{
    Assert(Workflows.ReadVersion("versionCode = 600\n") == 600);
    Throws<InvalidDataException>(() => Workflows.ReadVersion("versionCode = 600\nversionCode = 601\n"));
    Throws<InvalidDataException>(() => Workflows.ReadVersion("nothing")); return Task.CompletedTask;
});
await Test("Argument parsing preserves paths and avoids shell evaluation", () =>
{
    var values = AuxiliaryTools.ParseArguments("--input \"C:\\My Folder\\song.wav\" '--value=$(command)' \"\"");
    Assert(values.SequenceEqual(new[] { "--input", @"C:\My Folder\song.wav", "--value=$(command)", "" }));
    Throws<ArgumentException>(() => AuxiliaryTools.ParseArguments("\"unfinished")); return Task.CompletedTask;
});
await Test("All auxiliary source modules are embedded and discoverable", () =>
{
    Assert(AuxiliaryTools.Sources.Count == 176); Assert(AuxiliaryTools.Catalog.Any(t => t.Path.EndsWith("test_device_bridge.py")));
    Assert(AuxiliaryTools.Catalog.Any(t => t.Path == "tools/tests/test-player-rig.ps1"));
    Assert(AuxiliaryTools.Catalog.Any(t => t.Path == "tools/tests/test-head-attachment.ps1"));
    Assert(AuxiliaryTools.Catalog.Any(t => t.Path == "tools/tests/test-gta3-ragdoll-rig.ps1"));
    Assert(AuxiliaryTools.Catalog.Any(t => t.Path == "tools/tests/test-ped-vehicle-damage.ps1"));
    Assert(AuxiliaryTools.Catalog.Any(t => t.Path == "tools/tests/test-dodo-flight.ps1"));
    Assert(AuxiliaryTools.Catalog.All(t => !t.Path.Contains("build-and-install"))); return Task.CompletedTask;
});
await Test("Command runner keeps stderr diagnostics out of structured stdout", async () =>
{
    using var log = new PrivateLog(); var runner = new CommandRunner(log);
    var host = Environment.ProcessPath!;
    var childArgs = Path.GetFileNameWithoutExtension(host).Equals("dotnet", StringComparison.OrdinalIgnoreCase)
        ? new[] { Path.Combine(AppContext.BaseDirectory, "LibertyManager.Tests.dll"), "--probe-noise" } : new[] { "--probe-noise" };
    var result = await runner.Run(host, childArgs, showOutput: false);
    using var metadata = JsonDocument.Parse(result.StandardOutput);
    Assert(metadata.RootElement.GetProperty("format").GetProperty("duration").GetString() == "10");
    Assert(result.Output.Contains("[mp3 @ synthetic]") && !result.StandardOutput.Contains("diagnostic"));
});
await Test("Cancellation terminates the child process", async () =>
{
    using var log = new PrivateLog(); var runner = new CommandRunner(log); using var timeout = new CancellationTokenSource(TimeSpan.FromMilliseconds(800));
    var host = Environment.ProcessPath!;
    var childArgs = Path.GetFileNameWithoutExtension(host).Equals("dotnet", StringComparison.OrdinalIgnoreCase)
        ? new[] { Path.Combine(AppContext.BaseDirectory, "LibertyManager.Tests.dll"), "--child" } : new[] { "--child" };
    await ThrowsAsync<OperationCanceledException>(() => runner.Run(host, childArgs, token: timeout.Token));
});
await Test("Native privacy staged and history scans", async () =>
{
    var path = Path.Combine(root, "privacy-repo"); Directory.CreateDirectory(path); using var log = new PrivateLog(); var runner = new CommandRunner(log);
    await runner.Run("git", ["init", "-q", path], showOutput: false);
    var guard = new PrivacyGuard(path); File.WriteAllText(Path.Combine(path, "safe.txt"), "safe");
    await runner.Run("git", ["-C", path, "add", "safe.txt"], showOutput: false); await guard.Run("staged");
    await runner.Run("git", ["-C", path, "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid", "-c", "core.hooksPath=/dev/null", "commit", "-q", "-m", "Safe fixture"], showOutput: false);
    await guard.Run("history");
    File.WriteAllBytes(Path.Combine(path, "secret.dat"), Encoding.ASCII.GetBytes("prefix 2G" + new string('X', 12) + " suffix"));
    await runner.Run("git", ["-C", path, "add", "secret.dat"], showOutput: false);
    await ThrowsAsync<InvalidDataException>(() => guard.Run("staged"));
    await runner.Run("git", ["-C", path, "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid", "-c", "core.hooksPath=/dev/null", "commit", "-q", "-m", "Synthetic blocked fixture"], showOutput: false);
    await ThrowsAsync<InvalidDataException>(() => guard.Run("history"));
    await ThrowsAsync<InvalidDataException>(() => guard.Run("pre-push", pushInput: "refs/heads/master " + new string('a', 40) + " refs/checkpoint/local " + new string('0', 40)));
});

await Test("Playlist insertion moves tenth to second and preserves every other entry", () =>
{
    var music = new OrderedMusic();
    music.Load(new(new string('a', 64), Enumerable.Range(1, 12).Select(i => new MusicEntry { Name = i + ".mp3" }).ToArray()));
    Assert(music.Move(9, 1));
    Assert(music.Tracks.Select(t => t.Name).SequenceEqual(new[] { 1, 10, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12 }.Select(i => i + ".mp3")));
    Assert(music.Dirty && !music.Move(-1, 2) && !music.Move(1, 12));
    Assert(music.Move(1, 9) && music.Tracks[9].Name == "10.mp3");
    music.Load(new("revision", [])); Assert(!music.Dirty && music.Tracks.Count == 0);
    return Task.CompletedTask;
});
await Test("Music metadata display and file details retain original filename fallback", () =>
{
    var entry = new MusicEntry { Name = "hashed.mp3", Filename = "Original song.mp3", Artist = " Artist ", Album = "Album", Title = "Title", SampleRate = 48000, Channels = 2, Bytes = 1048576, Format = "MP3" };
    Assert(entry.Display == "Artist - Album - Title");
    Assert((entry with { Title = "" }).Display == "Artist - Album - Original song.mp3");
    Assert(new MusicEntry { Name = "plain.ogg" }.Display == "plain.ogg");
    Assert(entry.Details.Contains("48,000") && entry.Details.Contains("Original song.mp3") && entry.Details.Contains("1.00 MiB"));
    Assert((int)ManagerAction.ManageMusic == (int)ManagerAction.ImportMusic);
    return Task.CompletedTask;
});
await Test("Music validation-only never applies pending deletion or contacts a headset", async () =>
{
    using var log = new PrivateLog();
    await new Workflows(config, new CommandRunner(log), log).Perform(ManagerAction.ManageMusic,
        new() { ValidateMusicOnly = true, MusicRevision = new string('a',64), MusicDeleted = ["synthetic.mp3"] }, default);
});
await MusicChecks.Run(root, Test);
await RadioChecks.Run(root, Test);
await SaveTransferChecks.Run(root, Test);
Console.WriteLine($"{total - failures}/{total} manager checks passed.");
return failures == 0 ? 0 : 1;

sealed class FakeRunner : ICommandRunner
{
    public sealed record Call(string Executable, string[] Args, bool Show);
    public List<Call> Calls { get; } = [];
    public bool Multiple { get; init; }
    public bool WrongModel { get; init; }
    public string? Probe { get; init; }
    public Task<CommandResult> Run(string executable, IEnumerable<string> args, string? workingDirectory = null,
        bool showOutput = true, bool check = true, CancellationToken token = default, string? input = null)
    {
        var a = args.ToArray(); Calls.Add(new(executable, a, showOutput));
        if (a.Contains("-show_streams")) return Task.FromResult(new CommandResult(0, Probe!));
        if (a.SequenceEqual(new[] { "devices", "-l" })) return Task.FromResult(new CommandResult(0,
            "List of devices attached\nsynthetic-offline offline\nsynthetic-eligible device\n" + (Multiple ? "synthetic-other device\n" : "")));
        if (a.Contains("ro.product.model")) return Task.FromResult(new CommandResult(0, WrongModel ? "Quest 3S\n" : "Quest 3\n"));
        return Task.FromResult(new CommandResult(0, "Success\n"));
    }
}
