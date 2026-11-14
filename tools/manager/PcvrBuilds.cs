using System.Text;
using System.Text.Json;

namespace LibertyManager;

public sealed record PcvrBuild(string Label, string Directory, string Data, string User, string? Error = null);

/// <summary>Portable package metadata and recoverable current-directory publication.</summary>
public static class PcvrBuilds
{
    public const string Config = "pcvr-paths.ini";
    public const string Identity = "build-id.txt";
    private const string Journal = ".pcvr-publication.json";
    private sealed record Transaction(string Incoming, string Id, string? Archive, string? PreviousConfig);

    public static string ReadId(string directory)
    {
        var id = File.ReadAllText(Paths.Child(directory, Identity)).Trim();
        if (id.Length is < 1 or > 100 || id.Any(c => !char.IsAsciiLetterOrDigit(c) && c != '-'))
            throw new InvalidDataException("Invalid PCVR build ID.");
        return id;
    }

    public static (string Data, string User) ReadPaths(string directory)
    {
        var file = Paths.Child(directory, Config);
        if (new FileInfo(file).Length > 32768) throw new InvalidDataException("PCVR path configuration is too large.");
        var values = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var line in File.ReadAllLines(file))
        {
            if (line.Length == 0 || line.StartsWith('#')) continue;
            var equal = line.IndexOf('=');
            if (equal < 1 || !values.TryAdd(line[..equal], line[(equal + 1)..]))
                throw new InvalidDataException("Invalid PCVR path configuration.");
        }
        if (values.GetValueOrDefault("version") != "1" || values.Keys.Any(k => k is not ("version" or "data" or "user")))
            throw new InvalidDataException("Unsupported PCVR path configuration.");
        string Resolve(string key) => values.TryGetValue(key, out var value) && !string.IsNullOrWhiteSpace(value)
            ? Path.GetFullPath(value, directory) : throw new InvalidDataException("Missing PCVR " + key + " path.");
        return (Resolve("data"), Resolve("user"));
    }

    public static void WritePaths(string directory, string finalDirectory, string data, string user)
    {
        string Encode(string path)
        {
            var value = Path.GetRelativePath(finalDirectory, path).Replace('\\', '/');
            if (value.IndexOfAny(['\r', '\n', '\0']) >= 0) throw new InvalidDataException("Invalid PCVR path.");
            return value;
        }
        File.WriteAllText(Paths.Child(directory, Config), "version=1\ndata=" + Encode(data) + "\nuser=" + Encode(user) + "\n", new UTF8Encoding(false));
    }

    public static IReadOnlyList<PcvrBuild> Discover(PcvrPaths paths)
    {
        var result = new List<PcvrBuild>();
        string? active = null;
        try { active = PcvrWorkflow.ResolveInstallation(paths.Install, paths.Target); }
        catch (Exception e) when (e is IOException or InvalidDataException or JsonException or ArgumentException or UnauthorizedAccessException) { }
        void Add(string directory, bool current)
        {
            var label = current ? "Current" : Path.GetFileName(directory);
            try
            {
                PcvrWorkflow.ValidateBinaries(directory, paths.Target);
                var modern = File.Exists(Paths.Child(directory, Config));
                var identified = File.Exists(Paths.Child(directory, Identity));
                if (modern != identified || (!modern && string.Equals(directory,Paths.Child(paths.Install,"current"),Paths.Comparison)))
                    throw new InvalidDataException("PCVR build metadata is incomplete.");
                var pair = modern ? ReadPaths(directory) : (paths.Data, paths.User);
                var id = File.Exists(Paths.Child(directory, Identity)) ? ReadId(directory) : Path.GetFileName(directory);
                result.Add(new((current ? "Current — " : "") + id + (modern ? "" : " (legacy paths)"), directory, pair.Item1, pair.Item2));
            }
            catch (Exception e) when (e is IOException or InvalidDataException or ArgumentException or UnauthorizedAccessException)
            { result.Add(new(label + " (unavailable)", directory, "", "", e.Message)); }
        }
        var current = Paths.Child(paths.Install, "current");
        if (Directory.Exists(current)) Add(current, true);
        else if (active is not null) Add(active, true);
        var versions = Paths.Child(paths.Install, "versions");
        if (Directory.Exists(versions))
            foreach (var directory in Directory.EnumerateDirectories(versions).OrderByDescending(Path.GetFileName, StringComparer.Ordinal))
                if (!string.Equals(directory, active, Paths.Comparison)) Add(directory, false);
        return result;
    }

    // Recovery is performed under the same exclusive lock as publication. A missing
    // incoming directory plus the new current identity is the commit point.
    private static void Recover(string root)
    {
        var journal = Paths.Child(root, Journal);
        if (!File.Exists(journal)) return;
        var tx = JsonSerializer.Deserialize<Transaction>(File.ReadAllText(journal)) ?? throw new InvalidDataException("Invalid publication journal.");
        if (!tx.Incoming.StartsWith(".incoming-", StringComparison.Ordinal)) throw new InvalidDataException("Invalid incoming package.");
        var incoming = Paths.Child(root, tx.Incoming);
        var current = Paths.Child(root, "current");
        var archive = tx.Archive is null ? null : Paths.Child(root, "versions/" + tx.Archive);
        if (!Directory.Exists(incoming) && Directory.Exists(current) && ReadId(current) == tx.Id)
        {
            File.Delete(Paths.Child(root, "current.json"));
        }
        else if (archive is not null && Directory.Exists(archive))
        {
            if (Directory.Exists(current)) throw new IOException("Publication recovery found conflicting current and archive directories.");
            Directory.Move(archive, current);
            if (tx.PreviousConfig is not null) File.WriteAllText(Paths.Child(current, Config), tx.PreviousConfig, new UTF8Encoding(false));
        }
        else if (Directory.Exists(current) && tx.PreviousConfig is not null)
            File.WriteAllText(Paths.Child(current, Config), tx.PreviousConfig, new UTF8Encoding(false));
        File.Delete(journal);
    }

    public static void Publish(string root, string package, string id, PcvrPaths paths, CancellationToken token)
    {
        Paths.NoLinks(root); Directory.CreateDirectory(root);
        using var gate = new FileStream(Paths.Child(root, ".pcvr-install.lock"), FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None);
        Recover(root);
        PcvrWorkflow.ValidateBinaries(package, paths.Target);
        if (ReadId(package) != id) throw new InvalidDataException("Package identity mismatch.");
        var current = Paths.Child(root, "current");
        var name = ".incoming-" + Guid.NewGuid().ToString("N");
        var incoming = Paths.Child(root, name);
        // Copy before touching either public directory. Preserve bundled Linux links.
        CopyPackage(package, incoming, token);
        WritePaths(incoming, current, paths.Data, paths.User);
        PcvrWorkflow.ValidateBinaries(incoming, paths.Target);
        ReadPaths(incoming);
        string? previous = null, oldId = null;
        if (Directory.Exists(current))
        {
            oldId = ReadId(current);
            var archive = Paths.Child(root, "versions/" + oldId);
            if (Directory.Exists(archive) || File.Exists(archive)) throw new IOException("Archive already exists: " + oldId);
            previous = File.ReadAllText(Paths.Child(current, Config));
            ReadPaths(current);
            if (OperatingSystem.IsWindows())
                foreach (var executable in Directory.EnumerateFiles(current, "*.exe"))
                    using (File.Open(executable, FileMode.Open, FileAccess.Read, FileShare.None)) { }
        }
        token.ThrowIfCancellationRequested();
        var pendingJournal=Paths.Child(root,".publication-"+Guid.NewGuid().ToString("N"));
        using (var record=new FileStream(pendingJournal,FileMode.CreateNew,FileAccess.Write,FileShare.None))
        {
            JsonSerializer.Serialize(record,new Transaction(name,id,oldId,previous));
            record.Flush(true);
        }
        File.Move(pendingJournal,Paths.Child(root,Journal));
        try
        {
            if (oldId is not null)
            {
                var archive = Paths.Child(root, "versions/" + oldId);
                Directory.CreateDirectory(Paths.Child(root, "versions"));
                var oldPaths = ReadPaths(current);
                Directory.Move(current, archive);
                WritePaths(archive, archive, oldPaths.Data, oldPaths.User);
            }
            Directory.Move(incoming, current);
            Recover(root);
        }
        catch
        {
            Recover(root);
            throw;
        }
    }

    private static void CopyPackage(string source, string destination, CancellationToken token)
    {
        Paths.NoLinks(source); Directory.CreateDirectory(destination);
        foreach (var entry in Directory.EnumerateFileSystemEntries(source))
        {
            token.ThrowIfCancellationRequested();
            var target = Path.Combine(destination, Path.GetFileName(entry));
            var info = new FileInfo(entry);
            if (info.LinkTarget is { } link)
            {
                var resolved = info.ResolveLinkTarget(true)?.FullName ?? throw new IOException("Broken package link.");
                if (Path.IsPathRooted(link) || !Paths.Within(source, resolved)) throw new IOException("Package link escapes its directory.");
                File.CreateSymbolicLink(target, link);
            }
            else if (Directory.Exists(entry)) CopyPackage(entry, target, token);
            else { File.Copy(entry, target); if (!OperatingSystem.IsWindows()) File.SetUnixFileMode(target, File.GetUnixFileMode(entry)); }
        }
    }
}
