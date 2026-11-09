using System.Text.Json;
using LibertyManager;

internal static class PcvrModChecks
{
    private static void Check(bool value) { if (!value) throw new Exception("PCVR mod assertion failed."); }
    private static void Reject(Action action)
    {
        try { action(); } catch (Exception error) when (error is IOException or InvalidDataException or OperationCanceledException or UnauthorizedAccessException) { return; }
        throw new Exception("Unsafe PCVR mod operation accepted.");
    }
    public static async Task Run(string root, Func<string, Func<Task>, Task> test)
    {
        var home = Path.Combine(root, "pcvr-mods");
        var paths = new PcvrPaths(Path.Combine(home, "app"), Path.Combine(home, "data"), Path.Combine(home, "user"));
        var mod = CuratedMods.Upstate;
        var source = Path.Combine(home, "prepared");
        var payload = Path.Combine(paths.Data, "mods", mod.Id);
        var marker = Path.Combine(paths.Data, "mods", mod.Id + ".enabled");
        string[] files = ["data/gta3.dat", "data/main.scm", "data/main_d.scm", "data/modelTraits.dat", "data/object.dat",
            "data/waterpro.dat", "data/map.zon", "data/gta3.zon", "data/maps/UL/ul.img", "data/maps/UL/ul.dir", "CLEO/CLEO_TEXT/ul_text.fxt"];
        foreach (var relative in files)
        {
            var path = Path.Combine(source, relative); Directory.CreateDirectory(Path.GetDirectoryName(path)!); File.WriteAllText(path, "synthetic mod asset");
        }
        File.WriteAllText(Path.Combine(source, CuratedMods.Manifest), JsonSerializer.Serialize(new ModPackage(mod.Id, mod.Version, mod.Sha256,
            files.ToDictionary(p => p, p => Digests.Sha256(Path.Combine(source, p))))));
        Directory.CreateDirectory(paths.Data); Directory.CreateDirectory(paths.User);
        var save = Path.Combine(paths.User, "save.b"); File.WriteAllText(save, "keep save");
        var original = Path.Combine(paths.Data, "base.dat"); File.WriteAllText(original, "keep retail");
        await test("PCVR mod installs disabled on older builds without touching data or saves", () =>
        {
            var status = PcvrMods.Install(paths, mod, source, default);
            Check(status.Installed && !status.Enabled && !status.Supported);
            Reject(() => PcvrMods.Change(paths, mod, ModOperation.Enable, default));
            Check(PcvrMods.SelectEngine(paths) == "libertyvr-game.exe"); return Task.CompletedTask;
        });
        var version = Path.Combine(paths.Install, "versions/test");
        foreach (var name in new[] { "libertyvr-game.exe", "libertyvr-pcvr.exe", "openxr_loader.dll", "openxr-notices/LICENSE",
                     "openxr-notices/JsonCpp-LICENSE", "IMGUI_LICENSE.txt", "LIBRW_LICENSE.txt", PcvrMods.Engine, PcvrMods.Capabilities })
        {
            var path = Path.Combine(version, name); Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            File.WriteAllText(path, name == PcvrMods.Capabilities ? mod.Capability + "\n" : "synthetic binary");
        }
        PcvrWorkflow.PublishInstallation(paths.Install, "versions/test");
        await test("PCVR supported install enables engine; disable and delete preserve saves", () =>
        {
            Check(PcvrMods.Install(paths, mod, source, default).Enabled);
            Check(PcvrMods.SelectEngine(paths) == PcvrMods.Engine);
            Check(!PcvrMods.Change(paths, mod, ModOperation.Disable, default).Enabled && Directory.Exists(payload));
            Check(PcvrMods.SelectEngine(paths) == "libertyvr-game.exe");
            Check(PcvrMods.Change(paths, mod, ModOperation.Enable, default).Enabled);
            Check(!PcvrMods.Change(paths, mod, ModOperation.Delete, default).Installed);
            Check(File.ReadAllText(save) == "keep save" && File.ReadAllText(original) == "keep retail"); return Task.CompletedTask;
        });
        await test("PCVR tampering blocks enable and launch; reinstall repairs isolated payload", () =>
        {
            PcvrMods.Install(paths, mod, source, default);
            File.WriteAllText(Path.Combine(payload, "data/main.scm"), "modified");
            Reject(() => PcvrMods.SelectEngine(paths));
            PcvrMods.Change(paths, mod, ModOperation.Disable, default);
            Reject(() => PcvrMods.Change(paths, mod, ModOperation.Enable, default)); Check(!File.Exists(marker));
            Check(PcvrMods.Install(paths, mod, source, default).Enabled);
            File.AppendAllText(marker, "extra\n"); Reject(() => PcvrMods.SelectEngine(paths));
            PcvrMods.Change(paths, mod, ModOperation.Disable, default); return Task.CompletedTask;
        });
        await test("PCVR cancelled replacement retains previous payload and enable marker", () =>
        {
            PcvrMods.Install(paths, mod, source, default); var before = File.ReadAllText(marker);
            using var cancellation = new CancellationTokenSource(); cancellation.Cancel();
            Reject(() => PcvrMods.Install(paths, mod, source, cancellation.Token));
            CuratedMods.ValidatePackage(payload, mod); Check(File.ReadAllText(marker) == before);
            Check(!Directory.EnumerateDirectories(Path.Combine(paths.Data, "mods"), ".incoming-*").Any()); return Task.CompletedTask;
        });
        await test("PCVR activation failure rolls back the previous package", () =>
        {
            var previous = File.ReadAllText(Path.Combine(payload, "data/main.scm"));
            File.WriteAllText(Path.Combine(source, "data/main.scm"), "replacement asset");
            File.WriteAllText(Path.Combine(source, CuratedMods.Manifest), JsonSerializer.Serialize(new ModPackage(mod.Id, mod.Version, mod.Sha256,
                files.ToDictionary(p => p, p => Digests.Sha256(Path.Combine(source, p))))));
            // A directory at the enable-marker path forces activation to fail
            // after the verified staging directory has replaced the old payload.
            File.Delete(marker); Directory.CreateDirectory(marker);
            Reject(() => PcvrMods.Install(paths, mod, source, default));
            CuratedMods.ValidatePackage(payload, mod);
            Check(File.ReadAllText(Path.Combine(payload, "data/main.scm")) == previous && Directory.Exists(marker));
            Check(!Directory.EnumerateDirectories(Path.Combine(paths.Data, "mods"), ".incoming-*").Any());
            Directory.Delete(marker); Check(PcvrMods.Change(paths, mod, ModOperation.Enable, default).Enabled);
            return Task.CompletedTask;
        });
        await test("PCVR unmanaged mod directory is preserved", () =>
        {
            PcvrMods.Change(paths, mod, ModOperation.Delete, default);
            Directory.CreateDirectory(payload); File.WriteAllText(Path.Combine(payload, "personal.txt"), "keep");
            Reject(() => PcvrMods.Install(paths, mod, source, default));
            Check(File.ReadAllText(Path.Combine(payload, "personal.txt")) == "keep"); return Task.CompletedTask;
        });
    }
}
