using System.Security.Cryptography;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace LibertyManager;

public sealed record CuratedMod(string Id, string Name, string Version, string Url, long Bytes, string Sha256, string Capability);
public sealed record ModPackage(string Id, string Version, string ArchiveSha256, Dictionary<string, string> Files);
public sealed record ModStatus(bool Installed, bool Enabled, bool Supported)
{
    public override string ToString() => !Installed ? "Not installed" : Enabled ? "Enabled (restart required)" :
        Supported ? "Installed, disabled" : "Installed, disabled — Upstate runtime support is required";
}

/// <summary>Curated, hash-pinned packages. Windows plugins are reference material, never executed or deployed.</summary>
public sealed class CuratedMods(ManagerSettings settings, ICommandRunner runner, PrivateLog log)
{
    public static readonly CuratedMod Upstate = new("upstate-liberty", "Upstate Liberty", "1.1.2",
        "https://github.com/Adi23041999/UpstateLiberty/releases/download/v1.1.2/UL.7z", 32225684,
        "818C909583AFFAE9D89A5C597E28E83A1D733B7DFFEAAB8D99BA93E5194EDC76", "upstate-liberty/1");
    public static IReadOnlyList<CuratedMod> Catalog { get; } = Array.AsReadOnly(new[] { Upstate });
    public const string Manifest = ".lcvr-package.json";
    private const string RemoteRoot = QuestDevice.GameData + "/mods";
    private const long MaximumExpandedBytes = 1024L * 1024 * 1024;
    private static readonly JsonSerializerOptions Json = new() { WriteIndented = true };

    public static CuratedMod Find(string id) => Catalog.SingleOrDefault(m => m.Id == id)
        ?? throw new InvalidDataException("Select a mod from the curated catalog.");

    public static void ValidateArchive(string file, CuratedMod mod)
    {
        Paths.NoLinks(file);
        if (!File.Exists(file) || new FileInfo(file).Length != mod.Bytes ||
            !Digests.Sha256(file).Equals(mod.Sha256, StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException("Package does not match curated " + mod.Name + " v" + mod.Version + ". Your file was preserved.");
    }

    public static void ValidateArchiveListing(string listing)
    {
        // -slt -ba contains entries only. Validate every name before asking 7-Zip to extract.
        long expanded = 0;
        var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var line in listing.Replace("\r", "").Split('\n'))
        {
            if (line.StartsWith("Path = ", StringComparison.Ordinal))
            {
                var path = line[7..].Replace('\\', '/');
                if (!Regex.IsMatch(path, @"\A[A-Za-z0-9_ .@/-]+\z") || path.Contains(':') ||
                    path.Split('/').Any(p => p is "" or "." or "..") || path.StartsWith('/') || !names.Add(path))
                    throw new InvalidDataException("Unsafe or duplicate package path.");
            }
            if (line.StartsWith("Size = ", StringComparison.Ordinal))
            {
                if (!long.TryParse(line[7..], out var size) || size < 0 || size > MaximumExpandedBytes - expanded)
                    throw new InvalidDataException("Package expands beyond its permitted size.");
                expanded += size;
            }
            if (line.StartsWith("Symbolic Link =", StringComparison.Ordinal) || line.StartsWith("Hard Link =", StringComparison.Ordinal) ||
                line == "Encrypted = +") throw new InvalidDataException("Linked or encrypted package entries are unsupported.");
        }
        if (names.Count == 0 || expanded == 0) throw new InvalidDataException("Empty package listing.");
    }

    private string SevenZip()
    {
        if (!string.IsNullOrWhiteSpace(settings.SevenZip)) return Paths.FindExecutable(settings.SevenZip)
            ?? throw new IOException("Configured 7-Zip executable was not found.");
        if (SetupKitTools.Find(settings.Repository, OperatingSystem.IsWindows() ? "7za.exe" : "7zz") is { } bundled)
            return bundled;
        foreach (var name in new[] { "7zz", "7z", "7za", Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "7-Zip/7z.exe") })
            if (Paths.FindExecutable(name) is { } executable) return executable;
        throw new IOException("Install 7-Zip (7zz/7z), or select its executable in Setup help or Settings.");
    }

    private string CacheRoot => Path.Combine(settings.Workplace, "mods");
    private string CachedArchive(CuratedMod mod) => Path.Combine(CacheRoot, mod.Id + "-" + mod.Version + ".7z");
    private string Prepared(CuratedMod mod) => Path.Combine(CacheRoot, mod.Id + "-" + mod.Version);

    internal bool CheckSetupArchive(CuratedMod mod, string? supplied)
    {
        var local = !string.IsNullOrWhiteSpace(supplied) ? supplied : CachedArchive(mod);
        var cached = File.Exists(local);
        if (cached || !string.IsNullOrWhiteSpace(supplied)) ValidateArchive(local, mod);
        else if (!settings.AllowToolDownloads)
            throw new IOException("Select the official Upstate Liberty UL.7z or allow verified downloads in Setup help.");
        if (!Directory.Exists(Prepared(mod))) SevenZip();
        return cached;
    }

    public async Task<string> Prepare(CuratedMod mod, string suppliedArchive, CancellationToken token)
    {
        var home = CacheRoot;
        Paths.NoLinks(home); Directory.CreateDirectory(home);
        var archive = CachedArchive(mod);
        var incoming = Path.Combine(home, ".archive-" + Guid.NewGuid().ToString("N"));
        try
        {
            if (!string.IsNullOrWhiteSpace(suppliedArchive))
            {
                // Snapshot before verification/extraction so another process cannot change the selected file between them.
                Paths.NoLinks(suppliedArchive);
                await using var source = File.OpenRead(suppliedArchive);
                await using var destination = File.Create(incoming);
                await source.CopyToAsync(destination, token);
            }
            else if (!File.Exists(archive))
            {
                if (!settings.AllowToolDownloads)
                    throw new IOException("Select the official Upstate Liberty UL.7z or allow verified downloads.");
                log.Write("Downloading " + mod.Name + " v" + mod.Version + " from the curated release.");
                await Toolchain.DownloadFile(mod.Url, incoming, log, token);
            }
            if (File.Exists(incoming)) { ValidateArchive(incoming, mod); File.Move(incoming, archive, true); }
            ValidateArchive(archive, mod);
        }
        finally { if (File.Exists(incoming)) File.Delete(incoming); }
        log.Write("Verified " + mod.Name + " v" + mod.Version + " release archive.");
        var output = Prepared(mod);
        if (Directory.Exists(output)) { ValidatePackage(output, mod); return output; }
        var stage = Path.Combine(home, ".extract-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(stage);
        try
        {
            var sevenZip = SevenZip();
            var listing = await runner.Run(sevenZip, ["l", "-slt", "-ba", archive], showOutput: false, token: token);
            ValidateArchiveListing(listing.StandardOutput);
            await runner.Run(sevenZip, ["x", "-y", "-o" + stage, archive], showOutput: false, token: token);
            // Verify again before accepting extraction and reject links independently of the extractor's listing.
            ValidateArchive(archive, mod);
            var extracted = OriginalXboxModels.SafeFiles(stage).ToArray();
            foreach (var path in extracted)
            {
                token.ThrowIfCancellationRequested();
                var relative = Path.GetRelativePath(stage, path).Replace('\\', '/');
                if (!IsPayload(relative)) File.Delete(path);
            }
            var files = OriginalXboxModels.SafeFiles(stage).ToDictionary(
                p => Path.GetRelativePath(stage, p).Replace('\\', '/'), Digests.Sha256, StringComparer.Ordinal);
            foreach (var required in Required)
                if (!files.ContainsKey(required)) throw new InvalidDataException("Curated package lacks " + required);
            File.WriteAllText(Path.Combine(stage, Manifest), JsonSerializer.Serialize(new ModPackage(mod.Id, mod.Version, mod.Sha256, files), Json));
            ValidatePackage(stage, mod);
            Directory.Move(stage, output);
            log.Write("Prepared Upstate files. Windows ASIs are excluded; base game data and saves are preserved.");
            return output;
        }
        finally { if (Directory.Exists(stage)) Directory.Delete(stage, true); }
    }

    private static readonly string[] Required = ["data/gta3.dat", "data/main.scm", "data/main_d.scm", "data/modelTraits.dat",
        "data/object.dat", "data/waterpro.dat", "data/map.zon", "data/gta3.zon", "data/maps/UL/ul.img", "data/maps/UL/ul.dir",
        "CLEO/CLEO_TEXT/ul_text.fxt"];
    private static bool IsPayload(string path) => (path.StartsWith("data/", StringComparison.Ordinal) ||
        path.StartsWith("models/", StringComparison.Ordinal) || path == "CLEO/CLEO_TEXT/ul_text.fxt") &&
        !Regex.IsMatch(path, @"\.(asi|dll|exe|so|bat|cmd|ps1|sh)$", RegexOptions.IgnoreCase);

    public static ModPackage ValidatePackage(string root, CuratedMod mod)
    {
        Paths.NoLinks(root);
        var package = JsonSerializer.Deserialize<ModPackage>(File.ReadAllText(Paths.Child(root, Manifest)))
            ?? throw new InvalidDataException("Missing mod manifest.");
        if (package.Id != mod.Id || package.Version != mod.Version || package.ArchiveSha256 != mod.Sha256 || package.Files is null ||
            Required.Any(p => !package.Files.ContainsKey(p))) throw new InvalidDataException("Invalid curated mod manifest.");
        var actual = OriginalXboxModels.SafeFiles(root).Select(p => Path.GetRelativePath(root, p).Replace('\\', '/')).ToHashSet(StringComparer.Ordinal);
        if (!actual.SetEquals(package.Files.Keys.Append(Manifest))) throw new InvalidDataException("Mod has missing or unmanifested files.");
        foreach (var (relative, hash) in package.Files)
            if (!IsPayload(relative) || !Digests.Sha256(Paths.Child(root, relative)).Equals(hash, StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("Mod content verification failed.");
        return package;
    }

    private static string DirectoryPath(CuratedMod mod) => RemoteRoot + "/" + mod.Id;
    private static string EnabledPath(CuratedMod mod) => RemoteRoot + "/" + mod.Id + ".enabled";
    private static string Marker(CuratedMod mod) => mod.Id + ":" + mod.Version + ":" + mod.Sha256;
    private static string Q(string path) => QuestDevice.Quote(path);

    public async Task<ModStatus> Status(QuestDevice device, CuratedMod mod, CancellationToken token)
    {
        // The game's own runtime must advertise support; the Manager never writes this capability file.
        // Versioned tokens identify actual engine adapters, not merely the APK's version number.
        var result = await device.Shell("if [ -L " + Q(RemoteRoot) + " ] || [ -L " + Q(DirectoryPath(mod)) +
            " ]; then exit 1; fi; " +
            "if [ -f " + Q(DirectoryPath(mod) + "/" + Manifest) + " ]; then echo installed; fi; " +
            "if grep -qxF " + Q(Marker(mod)) + " " + Q(EnabledPath(mod)) + " 2>/dev/null; then echo enabled; fi; " +
            "if grep -qxF " + Q(mod.Capability) + " " + Q(RemoteRoot + "/.runtime-capabilities") + " 2>/dev/null; then echo supported; fi; true", token);
        var lines = result.Output.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
        return new(lines.Contains("installed"), lines.Contains("enabled"), lines.Contains("supported"));
    }

    private static async Task Stop(QuestDevice device, CancellationToken token)
    {
        var installed = await device.Run(["shell", "pm", "path", QuestDevice.Package], token);
        if (!installed.Output.Contains("package:", StringComparison.Ordinal)) throw new IOException("Install Liberty City VR first.");
        await device.Run(["shell", "am", "force-stop", QuestDevice.Package], token);
    }

    public Task<ModStatus> Install(QuestDevice device, CuratedMod mod, string output, CancellationToken token) =>
        Install(device, mod, output, token, verifiedApkSupport: false);

    internal async Task<ModStatus> Install(QuestDevice device, CuratedMod mod, string output, CancellationToken token,
        bool verifiedApkSupport)
    {
        var package = ValidatePackage(output, mod);
        await Stop(device, token);
        var before = await Status(device, mod, token);
        var supported = before.Supported || verifiedApkSupport;
        var final = DirectoryPath(mod);
        var suffix = Guid.NewGuid().ToString("N");
        var stage = RemoteRoot + "/.incoming-" + suffix;
        var backup = RemoteRoot + "/.previous-" + suffix;
        var enabled = EnabledPath(mod);
        var activated = false;
        try
        {
            // Refuse to replace a directory not owned by this manager.
            await device.Shell("set -e; mkdir -p " + Q(RemoteRoot) + "; if [ -e " + Q(final) + " ] && [ ! -f " + Q(final + "/" + Manifest) +
                " ]; then exit 1; fi; mkdir " + Q(stage), token);
            foreach (var (relative, expected) in package.Files.Append(new(Manifest, Digests.Sha256(Path.Combine(output, Manifest)))))
            {
                token.ThrowIfCancellationRequested();
                var target = stage + "/" + relative;
                await device.Shell("mkdir -p " + Q(target[..target.LastIndexOf('/')]), token);
                await device.Push(Paths.Child(output, relative), target, token, expected);
            }
            await device.Shell("chmod -R a+rX " + Q(stage), token);
            // The previous payload and enable marker survive a failed transaction.
            var script = "set -e; moved=0; had=0; " +
                "rollback() { if [ \"$moved\" = 1 ]; then rm -rf " + Q(final) + "; fi; if [ \"$had\" = 1 ]; then mv " + Q(backup) + " " + Q(final) + "; fi; }; " +
                "trap rollback EXIT; if [ -d " + Q(final) + " ]; then mv " + Q(final) + " " + Q(backup) + "; had=1; fi; " +
                "mv " + Q(stage) + " " + Q(final) + "; moved=1; " +
                (supported ? "printf '%s\\n' " + Q(Marker(mod)) + " > " + Q(enabled + ".new") + "; mv " + Q(enabled + ".new") + " " + Q(enabled) + "; " : "rm -f " + Q(enabled) + "; ") +
                "trap - EXIT; rm -rf " + Q(backup);
            await device.Shell(script, token);
            activated = true;
        }
        finally
        {
            using var cleanup = new CancellationTokenSource(TimeSpan.FromSeconds(10));
            try { await device.Shell("rm -rf " + Q(stage) + "; rm -f " + Q(enabled + ".new"), cleanup.Token); }
            catch (Exception error) when (error is IOException or OperationCanceledException) { log.Write("Mod staging cleanup could not finish. Previous saves were preserved."); }
        }
        if (activated) log.Write(supported ? "Upstate Liberty installed and enabled. Fully restart the game." :
            "Upstate Liberty installed, disabled. This APK has not advertised Upstate runtime support; engine adaptation is still required.");
        return (await Status(device, mod, token)) with { Supported = supported };
    }

    public async Task<ModStatus> Change(QuestDevice device, CuratedMod mod, ModOperation operation, CancellationToken token)
    {
        if (operation is not (ModOperation.Enable or ModOperation.Disable or ModOperation.Delete)) throw new ArgumentOutOfRangeException(nameof(operation));
        await Stop(device, token);
        var status = await Status(device, mod, token);
        if (!status.Installed) throw new IOException("This mod is not installed. Choose Install first.");
        if (operation == ModOperation.Enable && !status.Supported)
            throw new IOException("This APK has not advertised Upstate runtime support. Install a compatible APK and launch it once before enabling the mod.");
        var final = DirectoryPath(mod); var enabled = EnabledPath(mod);
        var read = await device.Shell("cat " + Q(final + "/" + Manifest), token);
        var package = JsonSerializer.Deserialize<ModPackage>(read.Output);
        if (package is null || package.Id != mod.Id || package.Version != mod.Version || package.ArchiveSha256 != mod.Sha256 ||
            package.Files is null || Required.Any(p => !package.Files.ContainsKey(p)))
            throw new InvalidDataException("Device mod manifest is invalid. No files were removed or enabled.");
        if (operation == ModOperation.Enable)
        {
            // Verify uploaded content again: a package may have been edited since installation.
            foreach (var (relative, expected) in package.Files)
            {
                if (!IsPayload(relative) || relative.Contains('\\') || relative.Split('/').Any(p => p is "" or "." or "..") ||
                    !Regex.IsMatch(expected, @"\A[0-9A-Fa-f]{64}\z")) throw new InvalidDataException("Unsafe device mod manifest.");
                var hash = await device.Shell("test ! -L " + Q(final + "/" + relative) + " && sha256sum " + Q(final + "/" + relative), token);
                if (!string.Equals(hash.Output.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries).FirstOrDefault(), expected, StringComparison.OrdinalIgnoreCase))
                    throw new IOException("Device mod content changed. Reinstall to repair it before enabling.");
            }
            await device.Shell("set -e; printf '%s\\n' " + Q(Marker(mod)) + " > " + Q(enabled + ".new") + "; mv " + Q(enabled + ".new") + " " + Q(enabled), token);
        }
        else if (operation == ModOperation.Disable) await device.Shell("rm -f " + Q(enabled), token);
        else
        {
            var trash = RemoteRoot + "/.deleted-" + Guid.NewGuid().ToString("N");
            await device.Shell("set -e; mv " + Q(final) + " " + Q(trash) + "; rm -f " + Q(enabled) + "; rm -rf " + Q(trash), token);
        }
        log.Write(operation == ModOperation.Delete ? "Removed Upstate Liberty from the device. Local archive/cache and all saves were preserved." :
            operation == ModOperation.Disable ? "Upstate Liberty disabled. Files and saves retained; fully restart the game." : "Upstate Liberty enabled. Fully restart the game.");
        return await Status(device, mod, token);
    }
}
