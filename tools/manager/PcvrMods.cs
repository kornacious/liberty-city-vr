using System.Diagnostics;
using System.Text.Json;

namespace LibertyManager;

/// <summary>Curated overlays in the isolated PCVR data copy, never in retail data.</summary>
public static class PcvrMods
{
    public const string Engine = "libertyvr-upstate.exe";
    public static string EngineFor(GameTarget target)=>target==GameTarget.LinuxPcvr?"libertyvr-upstate":Engine;
    internal static string PortableEngine(GameTarget target, string mode) =>
        target == GameTarget.LinuxPcvr ? "libertyvr-engine-" + mode + ".so" : "libertyvr-engine-" + mode + ".dll";
    public const string Capabilities = "pcvr-mod-capabilities.txt";
    private static string Marker(CuratedMod mod) => mod.Id + ":" + mod.Version + ":" + mod.Sha256 + "\n";
    private static string Payload(string data, CuratedMod mod) => Paths.Child(data, "mods/" + mod.Id);
    private static string Enabled(string data, CuratedMod mod) => Paths.Child(data, "mods/" + mod.Id + ".enabled");

    public static PcvrPaths Layout(ManagerSettings settings, GameTarget? target = null)
    {
        var paths = PcvrPaths.From(settings,target ?? PcvrWorkflow.HostTarget); paths.Validate(settings);
        PcvrWorkflow.ValidatePreparedData(paths.Data);
        return paths;
    }
    internal static void RequireStopped()
    {
        foreach (var name in OperatingSystem.IsLinux()?new[]{"libertyvr-game","libertyvr-upstate","libertyvr-upstat"}:new[] { "libertyvr-game", "libertyvr-upstate" })
        {
            var processes = Process.GetProcessesByName(name);
            try { if (processes.Length > 0) throw new IOException("Quit PCVR before changing mods."); }
            finally { foreach (var process in processes) process.Dispose(); }
        }
    }
    public static ModStatus Status(PcvrPaths paths, CuratedMod mod)
    {
        var installed = File.Exists(Paths.Child(Payload(paths.Data, mod), CuratedMods.Manifest));
        var marker = Enabled(paths.Data, mod);
        var enabled = File.Exists(marker) && File.ReadAllText(marker) == Marker(mod);
        var supported = false;
        if (Directory.Exists(paths.Install) &&
            (Directory.Exists(Paths.Child(paths.Install,"current")) || File.Exists(Paths.Child(paths.Install,"current.json")) ||
             File.Exists(Paths.Child(paths.Install,paths.Target==GameTarget.LinuxPcvr?"libertyvr-game":"libertyvr-game.exe"))))
        {
            var install = File.Exists(Paths.Child(paths.Install, ".portable-pcvr"))
                ? paths.Install : PcvrWorkflow.ResolveInstallation(paths.Install,paths.Target);
            supported = Supports(install, paths.Target, mod);
        }
        return new(installed, enabled, supported);
    }
    internal static bool Supports(string install, GameTarget target, CuratedMod mod)
    {
        var capabilities = Paths.Child(install, Capabilities);
        return (File.Exists(Paths.Child(install, EngineFor(target))) || File.Exists(Paths.Child(install, PortableEngine(target, "upstate")))) && File.Exists(capabilities) &&
            File.ReadAllLines(capabilities).Contains(mod.Capability, StringComparer.Ordinal);
    }
    public static string SelectEngine(PcvrPaths paths)
    {
        var mod = CuratedMods.Upstate;
        if (!File.Exists(Enabled(paths.Data, mod))) return paths.Target==GameTarget.LinuxPcvr?"libertyvr-game":"libertyvr-game.exe";
        var status = Status(paths, mod);
        if (!status.Enabled || !status.Installed || !status.Supported)
            throw new IOException("Upstate activation or PCVR engine is unavailable. Disable the mod or rebuild/install PCVR from the manager.");
        CuratedMods.ValidatePackage(Payload(paths.Data, mod), mod);
        return File.Exists(Paths.Child(paths.Install, ".portable-pcvr"))
            ? paths.Target == GameTarget.LinuxPcvr ? "libertyvr-game" : "libertyvr-game.exe"
            : EngineFor(paths.Target);
    }
    public static ModStatus Install(PcvrPaths paths, CuratedMod mod, string source, CancellationToken token)
    {
        RequireStopped();
        CuratedMods.ValidatePackage(source, mod);
        if (Paths.Within(paths.Data, source) || Paths.Within(source, paths.Data) ||
            Paths.Within(paths.User, source) || Paths.Within(paths.Install, source))
            throw new IOException("Prepare the mod outside live PCVR directories.");
        var before = Status(paths, mod);
        var final = Payload(paths.Data, mod);
        if (Directory.Exists(final)) ValidateOwnership(final, mod);
        var root = Paths.Child(paths.Data, "mods"); Directory.CreateDirectory(root);
        var stage = Paths.Child(root, ".incoming-" + Guid.NewGuid().ToString("N"));
        var backup = Paths.Child(root, ".previous-" + Guid.NewGuid().ToString("N"));
        var marker = Enabled(paths.Data, mod); var temporary = Paths.Child(root, ".enabled-" + Guid.NewGuid().ToString("N"));
        var had = Directory.Exists(final); var moved = false; var committed = false;
        try
        {
            Directory.CreateDirectory(stage);
            foreach (var file in OriginalXboxModels.SafeFiles(source))
            {
                token.ThrowIfCancellationRequested();
                var target = Paths.Child(stage, Path.GetRelativePath(source, file).Replace('\\', '/'));
                Directory.CreateDirectory(Path.GetDirectoryName(target)!); File.Copy(file, target);
            }
            CuratedMods.ValidatePackage(stage, mod);
            if (before.Supported) File.WriteAllText(temporary, Marker(mod));
            token.ThrowIfCancellationRequested();
            if (had) Directory.Move(final, backup);
            try
            {
                Directory.Move(stage, final); moved = true;
                if (before.Supported) File.Move(temporary, marker, true);
                else File.Delete(marker);
                committed = true;
            }
            catch
            {
                if (moved) Directory.Move(final, stage);
                if (had) Directory.Move(backup, final);
                throw;
            }
        }
        finally
        {
            if (Directory.Exists(stage)) Directory.Delete(stage, true);
            if (File.Exists(temporary)) File.Delete(temporary);
        }
        if (committed && had) Directory.Delete(backup, true);
        return Status(paths, mod);
    }
    private static void ValidateOwnership(string path, CuratedMod mod)
    {
        var manifest = Paths.Child(path, CuratedMods.Manifest);
        if (!File.Exists(manifest)) throw new IOException("Refusing to replace or delete an unmanaged mod directory.");
        var package = JsonSerializer.Deserialize<ModPackage>(File.ReadAllText(manifest));
        if (package is null || package.Id != mod.Id || package.Version != mod.Version || package.ArchiveSha256 != mod.Sha256)
            throw new InvalidDataException("Invalid PCVR mod manifest. Existing files preserved.");
        // Reject links anywhere in a directory before a recursive delete or rename.
        _ = OriginalXboxModels.SafeFiles(path).ToArray();
    }
    public static ModStatus Change(PcvrPaths paths, CuratedMod mod, ModOperation operation, CancellationToken token)
    {
        if (operation is not (ModOperation.Enable or ModOperation.Disable or ModOperation.Delete)) throw new ArgumentOutOfRangeException(nameof(operation));
        RequireStopped(); var status = Status(paths, mod);
        var final = Payload(paths.Data, mod); var marker = Enabled(paths.Data, mod);
        if (operation == ModOperation.Disable) { token.ThrowIfCancellationRequested(); File.Delete(marker); return Status(paths, mod); }
        if (!status.Installed) throw new IOException("Install this mod first.");
        ValidateOwnership(final, mod);
        if (operation == ModOperation.Enable)
        {
            if (!status.Supported) throw new IOException("Rebuild and install a PCVR build with Upstate support first.");
            CuratedMods.ValidatePackage(final, mod);
            var temporary = Paths.Child(paths.Data, "mods/.enabled-" + Guid.NewGuid().ToString("N"));
            try { File.WriteAllText(temporary, Marker(mod)); token.ThrowIfCancellationRequested(); File.Move(temporary, marker, true); }
            finally { if (File.Exists(temporary)) File.Delete(temporary); }
        }
        else
        {
            token.ThrowIfCancellationRequested();
            File.Delete(marker); Directory.Delete(final, true);
        }
        return Status(paths, mod);
    }
}
