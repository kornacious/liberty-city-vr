using System.Text.Json;
using LibertyManager;

internal static class ModChecks
{
    private static void Check(bool value) { if (!value) throw new Exception("Mod assertion failed."); }
    private static async Task Reject(Func<Task> action)
    {
        try { await action(); } catch (Exception error) when (error is IOException or InvalidDataException) { return; }
        throw new Exception("Unsafe mod operation was accepted.");
    }
    private static readonly string[] Payload = ["data/gta3.dat", "data/main.scm", "data/main_d.scm", "data/modelTraits.dat",
        "data/object.dat", "data/waterpro.dat", "data/map.zon", "data/gta3.zon", "data/maps/UL/ul.img", "data/maps/UL/ul.dir",
        "CLEO/CLEO_TEXT/ul_text.fxt"];
    private static void WritePayload(string root)
    {
        foreach (var relative in Payload) { var path = Path.Combine(root, relative); Directory.CreateDirectory(Path.GetDirectoryName(path)!); File.WriteAllText(path, "synthetic payload"); }
    }
    private static void WriteManifest(string root, CuratedMod mod)
    {
        var files = Payload.ToDictionary(p => p, p => Digests.Sha256(Path.Combine(root, p)));
        File.WriteAllText(Path.Combine(root, CuratedMods.Manifest), JsonSerializer.Serialize(new ModPackage(mod.Id, mod.Version, mod.Sha256, files)));
    }

    public static async Task Run(string root, Func<string, Func<Task>, Task> test)
    {
        await test("Curated catalog pins Upstate v1.1.2 and rejects arbitrary mod IDs", async () =>
        {
            var mod = CuratedMods.Find("upstate-liberty");
            Check(mod.Version == "1.1.2" && mod.Bytes == 32225684 && mod.Sha256.Length == 64 && mod.Url.EndsWith("/v1.1.2/UL.7z"));
            await Reject(() => Task.Run(() => CuratedMods.Find("../other")));
        });
        await test("Archive validation rejects wrong release and local file changes", async () =>
        {
            var path = Path.Combine(root, "synthetic.7z"); File.WriteAllText(path, "synthetic archive");
            var mod = CuratedMods.Upstate with { Bytes = new FileInfo(path).Length, Sha256 = Digests.Sha256(path) };
            CuratedMods.ValidateArchive(path, mod);
            File.WriteAllText(path, "tampered archive");
            await Reject(() => Task.Run(() => CuratedMods.ValidateArchive(path, mod)));
            Check(File.Exists(path));
        });
        await test("7z listing rejects traversal, links, duplicate names, encryption and expansion bombs", async () =>
        {
            CuratedMods.ValidateArchiveListing("Path = data\\main.scm\nSize = 16\nEncrypted = -\n");
            foreach (var listing in new[] { "Path = ../escape\nSize = 1", "Path = /escape\nSize = 1", "Path = C:/escape\nSize = 1",
                "Path = data/a\nSize = 1\nPath = DATA/a\nSize = 1", "Path = data/a\nSize = 1\nSymbolic Link = /escape",
                "Path = data/a\nSize = 1\nHard Link = /escape", "Path = data/a\nSize = 1\nEncrypted = +",
                "Path = data/a\nSize = 1073741825", "Path = data/a\nSize = -1", "" })
                await Reject(() => Task.Run(() => CuratedMods.ValidateArchiveListing(listing)));
        });
        await test("Prepared manifest detects tampering, missing files and Windows plugins", async () =>
        {
            var local = Path.Combine(root, "mod-manifest"); WritePayload(local); WriteManifest(local, CuratedMods.Upstate);
            CuratedMods.ValidatePackage(local, CuratedMods.Upstate);
            File.WriteAllText(Path.Combine(local, "data/main.scm"), "changed");
            await Reject(() => Task.Run(() => CuratedMods.ValidatePackage(local, CuratedMods.Upstate)));
            WritePayload(local); File.WriteAllText(Path.Combine(local, "data/plugin.asi"), "not executable");
            await Reject(() => Task.Run(() => CuratedMods.ValidatePackage(local, CuratedMods.Upstate)));
        });
        await test("Mod settings round trip without changing legacy action numbers", () =>
        {
            var store = new SettingsStore(Path.Combine(root, "mod-settings"));
            var settings = new ManagerSettings { Repository = ManagerSettings.FindRepository(), Workplace = Path.Combine(root, "mod-work") };
            // Test settings must use a workplace outside the repository, as production does.
            settings.Repository = Path.Combine(root, "source");
            settings.Actions[ManagerAction.ManageMods] = new() { ModOperation = ModOperation.Enable, ModArchive = "local UL.7z" };
            store.Save(settings); var restored = store.Load().Actions[ManagerAction.ManageMods];
            Check(restored.ModOperation == ModOperation.Enable && restored.ModArchive == "local UL.7z" && (int)ManagerAction.TransferSave == 6 && (int)ManagerAction.ManageMods == 7);
            return Task.CompletedTask;
        });

        // Run the production device shell transactions against disposable local directories.
        // No real ADB, game assets or personal saves are accessed.
        async Task Fixture(string name, Func<CuratedMods, QuestDevice, DeviceShell, string, Task> body)
        {
            await test(name, async () =>
            {
                var home = Path.Combine(root, "mod-device-" + Guid.NewGuid().ToString("N")); Directory.CreateDirectory(home);
                var local = Path.Combine(home, "prepared"); WritePayload(local); WriteManifest(local, CuratedMods.Upstate);
                using var log = new PrivateLog(); var shell = new DeviceShell(Path.Combine(home, "device"), log);
                var device = new QuestDevice(shell, log, "fake-adb", "synthetic-target");
                var mods = new CuratedMods(new() { Workplace = home }, shell, log);
                await body(mods, device, shell, local);
                Check(shell.AllPrivate);
            });
        }
        await Fixture("Unsupported APK installs verified files disabled and refuses activation", async (mods, device, shell, local) =>
        {
            var status = await mods.Install(device, CuratedMods.Upstate, local, default);
            Check(status.Installed && !status.Enabled && !status.Supported);
            await Reject(() => mods.Change(device, CuratedMods.Upstate, ModOperation.Enable, default));
            Check(!File.Exists(shell.Enabled));
        });
        await Fixture("Verified Setup APK enables Upstate before its first launch", async (mods, device, shell, local) =>
        {
            var status = await mods.Install(device, CuratedMods.Upstate, local, default, verifiedApkSupport: true);
            Check(status.Installed && status.Enabled && status.Supported);
            Check(File.Exists(shell.Enabled));
        });
        await Fixture("Supported runtime installs enabled, disables, re-enables and deletes while preserving saves", async (mods, device, shell, local) =>
        {
            shell.Support(); var saves = Path.Combine(shell.Root, "mod-saves/upstate-liberty/save.b"); Directory.CreateDirectory(Path.GetDirectoryName(saves)!); File.WriteAllText(saves, "personal save fixture");
            Check((await mods.Install(device, CuratedMods.Upstate, local, default)).Enabled);
            Check(!(await mods.Change(device, CuratedMods.Upstate, ModOperation.Disable, default)).Enabled && Directory.Exists(shell.Final));
            Check((await mods.Change(device, CuratedMods.Upstate, ModOperation.Enable, default)).Enabled);
            Check(!(await mods.Change(device, CuratedMods.Upstate, ModOperation.Delete, default)).Installed);
            Check(File.ReadAllText(saves) == "personal save fixture" && Directory.Exists(local) && !File.Exists(shell.Enabled));
        });
        await Fixture("Upload hash failure preserves the previous package and enable marker", async (mods, device, shell, local) =>
        {
            shell.Support(); await mods.Install(device, CuratedMods.Upstate, local, default);
            var old = File.ReadAllText(Path.Combine(shell.Final, "data/main.scm")); var marker = File.ReadAllText(shell.Enabled);
            shell.CorruptUpload = true; await Reject(() => mods.Install(device, CuratedMods.Upstate, local, default));
            Check(File.ReadAllText(Path.Combine(shell.Final, "data/main.scm")) == old && File.ReadAllText(shell.Enabled) == marker);
            Check(!Directory.EnumerateDirectories(Path.Combine(shell.Root, "mods"), ".incoming-*").Any());
        });
        await Fixture("Failed activation shell transaction rolls back the previous payload", async (mods, device, shell, local) =>
        {
            shell.Support(); await mods.Install(device, CuratedMods.Upstate, local, default);
            File.WriteAllText(Path.Combine(shell.Final, "keep-old.txt"), "previous payload");
            shell.FailActivation = true; await Reject(() => mods.Install(device, CuratedMods.Upstate, local, default));
            Check(File.ReadAllText(Path.Combine(shell.Final, "keep-old.txt")) == "previous payload" && File.Exists(shell.Enabled));
        });
        await Fixture("Cancelled upload cleans staging with an independent token and preserves the installed mod", async (mods, device, shell, local) =>
        {
            shell.Support(); await mods.Install(device, CuratedMods.Upstate, local, default);
            shell.CancelUpload = true;
            try { await mods.Install(device, CuratedMods.Upstate, local, default); throw new Exception("Cancellation was ignored."); }
            catch (OperationCanceledException) { }
            Check(File.Exists(Path.Combine(shell.Final, "data/main.scm")) && File.Exists(shell.Enabled));
            Check(!Directory.EnumerateDirectories(Path.Combine(shell.Root, "mods"), ".incoming-*").Any());
        });
        await Fixture("Re-enable refuses modified device content", async (mods, device, shell, local) =>
        {
            shell.Support(); await mods.Install(device, CuratedMods.Upstate, local, default);
            await mods.Change(device, CuratedMods.Upstate, ModOperation.Disable, default);
            File.WriteAllText(Path.Combine(shell.Final, "data/main.scm"), "tampered on device");
            await Reject(() => mods.Change(device, CuratedMods.Upstate, ModOperation.Enable, default)); Check(!File.Exists(shell.Enabled));
        });
        await Fixture("Unmanaged device folders are never replaced or deleted", async (mods, device, shell, local) =>
        {
            Directory.CreateDirectory(shell.Final); var owned = Path.Combine(shell.Final, "user.txt"); File.WriteAllText(owned, "keep");
            await Reject(() => mods.Install(device, CuratedMods.Upstate, local, default));
            await Reject(() => mods.Change(device, CuratedMods.Upstate, ModOperation.Delete, default)); Check(File.ReadAllText(owned) == "keep");
        });
    }

    private sealed class DeviceShell(string root, PrivateLog log) : ICommandRunner
    {
        public string Root => root;
        public string Final => Path.Combine(root, "mods/upstate-liberty");
        public string Enabled => Path.Combine(root, "mods/upstate-liberty.enabled");
        public bool AllPrivate = true, CorruptUpload, FailActivation, CancelUpload;
        public void Support() { Directory.CreateDirectory(Path.Combine(root, "mods")); File.WriteAllText(Path.Combine(root, "mods/.runtime-capabilities"), CuratedMods.Upstate.Capability + "\n"); }
        private static string Unix(string path) => OperatingSystem.IsWindows() ? "/" + char.ToLowerInvariant(path[0]) + path[2..].Replace('\\', '/') : path;
        public async Task<CommandResult> Run(string executable, IEnumerable<string> args, string? workingDirectory = null, bool showOutput = true, bool check = true, CancellationToken token = default, string? input = null)
        {
            AllPrivate &= !showOutput;
            var a = args.Skip(2).ToArray();
            if (a is ["shell", "pm", "path", _]) return new(0, "package:/synthetic/base.apk");
            if (a is ["shell", "am", "force-stop", _]) return new(0, "");
            if (a is ["push", var file, var destination])
            {
                var local = root + destination[QuestDevice.GameData.Length..].Replace('/', Path.DirectorySeparatorChar);
                Directory.CreateDirectory(Path.GetDirectoryName(local)!); File.Copy(file, local, true);
                if (CorruptUpload) File.AppendAllText(local, "corrupt");
                if (CancelUpload) throw new OperationCanceledException("Synthetic upload cancellation.");
                return new(0, "");
            }
            if (a is not ["shell", var command]) throw new Exception("Unexpected device command.");
            command = command.Replace(QuestDevice.GameData, Unix(root));
            if (FailActivation && command.Contains("trap rollback EXIT")) command = command.Replace("; moved=1;", "; moved=1; false;");
            var shell = OperatingSystem.IsWindows() ? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "Git/bin/bash.exe") : "/bin/sh";
            return await new CommandRunner(log).Run(shell, ["-c", command], showOutput: false, check: check, token: token);
        }
    }
}
