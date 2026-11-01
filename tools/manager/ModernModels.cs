using System.Buffers.Binary;
using System.IO.Compression;
using System.Net;
using System.Reflection;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace LibertyManager;

public sealed record PackFile(long Bytes, string Sha256);
public sealed record ModelPack(string BuilderVersion, string Page, string DownloadPage, string ArchiveName,
    long ArchiveBytes, string ArchiveSha256, string SourceFolder, string[] Archives, Dictionary<string, PackFile> Files);
public sealed record ImgEntry(string Name, uint Sector, uint Count, string Archive);

public sealed class ModernModels(ManagerSettings settings, ICommandRunner runner, PrivateLog log)
{
    public static ModelPack Pack { get; } = ReadPack();
    private static ModelPack ReadPack()
    {
        var assembly = Assembly.GetExecutingAssembly();
        using var stream = assembly.GetManifestResourceStream(assembly.GetManifestResourceNames().Single(n => n.EndsWith("xbox-pack.json")))!;
        return JsonSerializer.Deserialize<ModelPack>(stream, new JsonSerializerOptions { PropertyNameCaseInsensitive = true })!;
    }

    public static IEnumerable<ImgEntry> ReadImg(string img, string directory, bool allowEmpty = false)
    {
        var length = new FileInfo(img).Length;
        var bytes = File.ReadAllBytes(directory);
        if (bytes.Length == 0 || bytes.Length % 32 != 0) throw new InvalidDataException("Invalid IMG v1 directory length.");
        var names = new HashSet<string>();
        for (var i = 0; i < bytes.Length; i += 32)
        {
            var sector = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(i, 4));
            var count = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(i + 4, 4));
            var name = Encoding.ASCII.GetString(bytes, i + 8, 24).Split('\0')[0].ToLowerInvariant();
            if (!Regex.IsMatch(name, @"\A[a-z0-9_@]{1,19}\.(dff|txd)\z") || !names.Add(name) ||
                (!allowEmpty && count == 0) || ((long)sector + count) * 2048 > length)
                throw new InvalidDataException("Invalid or duplicate IMG v1 entry.");
            if (count != 0) yield return new(name, sector, count, img);
        }
    }

    public static Dictionary<string, string> ValidateOverlay(string root)
    {
        Paths.NoLinks(root);
        var lines = File.ReadAllLines(Path.Combine(root, "BUILD_INFO.txt"));
        var info = lines.Where(l => l.Contains('=')).Select(l => l.Split('=', 2)).ToDictionary(p => p[0], p => p[1]);
        if (info.GetValueOrDefault("Profile") != "MODERN" || info.GetValueOrDefault("BuilderVersion") != Pack.BuilderVersion ||
            info.GetValueOrDefault("SourceSHA256") != Pack.ArchiveSha256) throw new InvalidDataException("Overlay does not match the pinned Modern builder.");
        var hashes = info.Where(p => p.Key.StartsWith("SHA256 ", StringComparison.Ordinal)).ToDictionary(p => p.Key[7..], p => p.Value);
        string[] required = ["models/gta3.img", "models/gta3.dir", "models/generic/player.bmp", "vegetation_models.txt", "vehicle_models.txt", "asset_categories.json"];
        if (required.Any(p => !hashes.ContainsKey(p))) throw new InvalidDataException("Overlay manifest lacks required files.");
        var actual = SafeFiles(root).Select(p => Path.GetRelativePath(root, p).Replace('\\', '/')).ToHashSet(StringComparer.Ordinal);
        if (!actual.SetEquals(hashes.Keys.Append("BUILD_INFO.txt"))) throw new InvalidDataException("Overlay has missing or unmanifested files.");
        foreach (var (relative, hash) in hashes)
            if (!Digests.Sha256(Paths.Child(root, relative)).Equals(hash, StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("Overlay file checksum mismatch: " + relative);
        long expected = 0;
        foreach (var entry in ReadImg(Path.Combine(root, "models/gta3.img"), Path.Combine(root, "models/gta3.dir")))
        { if (entry.Sector != expected) throw new InvalidDataException("Generated IMG is not compact."); expected += entry.Count; }
        if (expected * 2048 != new FileInfo(Path.Combine(root, "models/gta3.img")).Length) throw new InvalidDataException("Unexpected trailing IMG bytes.");
        return hashes;
    }

    private static IEnumerable<string> SafeFiles(string root)
    {
        Paths.NoLinks(root);
        foreach (var entry in Directory.EnumerateFileSystemEntries(root))
        {
            Paths.NoLinks(entry);
            if (Directory.Exists(entry)) { foreach (var file in SafeFiles(entry)) yield return file; }
            else yield return entry;
        }
    }

    private static Dictionary<string, string> VerifySource(string source)
    {
        var candidates = SafeFiles(source).Where(p => Path.GetFileName(p).Equals("xbox_vehicles.img", StringComparison.OrdinalIgnoreCase))
            .Select(p => Directory.GetParent(p)!.Parent!.FullName).Append(Path.GetFullPath(source)).Distinct(OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal);
        var valid = new List<Dictionary<string, string>>();
        foreach (var candidate in candidates)
        {
            var files = SafeFiles(candidate).ToDictionary(p => Path.GetRelativePath(candidate, p).Replace('\\', '/').ToLowerInvariant(), p => p);
            if (Pack.Files.Keys.All(files.ContainsKey)) valid.Add(files);
        }
        if (valid.Count != 1) throw new InvalidDataException("Select the extracted Xbox Version HD (re3) source folder.");
        var selected = new Dictionary<string, string>();
        foreach (var (relative, expected) in Pack.Files)
        {
            var path = valid[0][relative];
            if (new FileInfo(path).Length != expected.Bytes || Digests.Sha256(path) != expected.Sha256)
                throw new InvalidDataException("Source file does not match the pinned re3 pack: " + relative);
            selected.Add(relative, path);
        }
        return selected;
    }

    private static void VerifyArchive(string archive)
    {
        if (new FileInfo(archive).Length != Pack.ArchiveBytes || Digests.Sha256(archive) != Pack.ArchiveSha256)
            throw new InvalidDataException("Source archive does not match the supported ModDB re3 release (size/SHA256).");
    }

    public async Task<string> Prepare(ActionOptions options, CancellationToken token)
    {
        var work = Path.Combine(settings.Workplace, "modern-assets");
        var output = string.IsNullOrWhiteSpace(options.ModelOutput)
            ? Path.Combine(work, "modelsets/modern") : Path.GetFullPath(options.ModelOutput);
        if (Path.GetFileName(Path.TrimEndingDirectorySeparator(output)) != "modern" || Paths.Within(settings.Repository, output) || Paths.Within(output, settings.Repository))
            throw new IOException("Use a separate output folder named modern, outside the repository.");
        Paths.NoLinks(output);
        if (string.IsNullOrWhiteSpace(options.ModelSource) && string.IsNullOrWhiteSpace(options.ModelArchive) && Directory.Exists(output))
        {
            ValidateOverlay(output);
            log.Write("Reusing verified Modern overlay."); return output;
        }
        Directory.CreateDirectory(work);
        var source = options.ModelSource;
        if (string.IsNullOrWhiteSpace(source))
        {
            var archive = string.IsNullOrWhiteSpace(options.ModelArchive) ? Path.Combine(work, Pack.ArchiveName) : Path.GetFullPath(options.ModelArchive);
            if (!File.Exists(archive))
            {
                if (!options.DownloadModels) throw new IOException("Choose the pinned ModDB ZIP, an extracted pack, or enable the 1.45 GiB pack download.");
                log.Write("Requesting an official ModDB mirror.");
                using var client = new HttpClient { Timeout = TimeSpan.FromSeconds(30) };
                var page = await client.GetStringAsync(Pack.DownloadPage, token);
                var mirror = Regex.Match(page, "href=[\"']([^\"']*/downloads/mirror/224588/[^\"']+)");
                if (!mirror.Success) throw new IOException("ModDB download unavailable. Download the supported ZIP manually from " + Pack.Page);
                var uri = new Uri(new Uri(Pack.DownloadPage), WebUtility.HtmlDecode(mirror.Groups[1].Value));
                if (uri.Host is not ("www.moddb.com" or "moddb.com") || uri.Scheme != "https") throw new IOException("Unexpected ModDB mirror URL.");
                await Toolchain.DownloadFile(uri.AbsoluteUri, archive, log, token);
            }
            VerifyArchive(archive);
            source = Path.Combine(work, "sources", Pack.ArchiveSha256[..16]);
            using var zip = ZipFile.OpenRead(archive);
            var entries = zip.Entries.ToDictionary(e => e.FullName.ToLowerInvariant());
            foreach (var relative in Pack.Files.Keys)
            {
                token.ThrowIfCancellationRequested();
                if (!entries.TryGetValue((Pack.SourceFolder + "/" + relative).ToLowerInvariant(), out var entry)) throw new InvalidDataException("Missing allowlisted pack entry.");
                var target = Paths.Child(source, relative);
                Directory.CreateDirectory(Path.GetDirectoryName(target)!);
                entry.ExtractToFile(target, true);
            }
        }
        if (Paths.Within(source, output) || Paths.Within(output, source)) throw new IOException("Model output must be separate from the pack source.");
        log.Write("Verifying pinned source files. Large texture archives can take several minutes.");
        var files = VerifySource(source);
        if (Directory.Exists(output) && !File.ReadAllLines(Path.Combine(output, "BUILD_INFO.txt")).Contains("Profile=MODERN"))
            throw new IOException("Refusing to replace a folder without a generated Modern profile marker.");
        var parent = Path.GetDirectoryName(output)!;
        Directory.CreateDirectory(parent);
        var stage = Path.Combine(parent, ".xbox.build-" + Guid.NewGuid().ToString("N"));
        var scratch = Path.Combine(parent, ".xbox.source-" + Guid.NewGuid().ToString("N"));
        var backup = Path.Combine(parent, ".xbox.previous-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(stage); Directory.CreateDirectory(scratch);
        try
        {
            var selected = new SortedDictionary<string, ImgEntry>(StringComparer.Ordinal);
            foreach (var archive in Pack.Archives)
                foreach (var entry in ReadImg(files["models/" + archive + ".img"], files["models/" + archive + ".dir"], true)) selected[entry.Name] = entry;
            foreach (var entry in selected.Values)
            {
                token.ThrowIfCancellationRequested();
                using var img = File.OpenRead(entry.Archive);
                img.Position = entry.Sector * 2048L;
                using var target = File.Create(Path.Combine(scratch, entry.Name));
                CopyBytes(img, target, entry.Count * 2048L);
            }
            foreach (var (relative, file) in files)
            {
                if (Path.GetExtension(file).ToLowerInvariant() is ".img" or ".dir") continue;
                var target = Paths.Child(stage, relative);
                Directory.CreateDirectory(Path.GetDirectoryName(target)!); File.Copy(file, target);
            }
            var wheels = Path.Combine(stage, "models/generic/wheels.dff");
            if (File.Exists(wheels)) { var bytes = File.ReadAllBytes(wheels); var repaired = RepairWheelTextures(bytes); File.WriteAllBytes(wheels, bytes); log.Write($"Repaired {repaired} wheel texture references."); }
            var compressor = await Compressor(token);
            var txds = Directory.GetFiles(scratch, "*.txd").Concat(SafeFiles(stage).Where(p => Path.GetExtension(p) == ".txd")).Order(StringComparer.Ordinal).ToArray();
            for (var i = 0; i < txds.Length; i++)
            {
                if (i % 20 == 0) log.Write($"Converting textures: {i + 1}/{txds.Length}");
                await runner.Run(compressor, ["--cap", "1024", txds[i]], showOutput: false, token: token);
            }
            Directory.CreateDirectory(Path.Combine(stage, "models"));
            using (var img = File.Create(Path.Combine(stage, "models/gta3.img")))
            using (var dir = new BinaryWriter(File.Create(Path.Combine(stage, "models/gta3.dir"))))
            {
                uint sector = 0;
                foreach (var file in Directory.GetFiles(scratch).Order(StringComparer.Ordinal))
                {
                    token.ThrowIfCancellationRequested();
                    var size = new FileInfo(file).Length;
                    var count = checked((uint)((size + 2047) / 2048));
                    dir.Write(sector); dir.Write(count);
                    var name = new byte[24]; Encoding.ASCII.GetBytes(Path.GetFileName(file)).CopyTo(name, 0); dir.Write(name);
                    using var input = File.OpenRead(file); input.CopyTo(img);
                    img.Write(new byte[count * 2048L - size]); sector += count;
                }
            }
            Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
            var vegetation = new SortedSet<string>(StringComparer.Ordinal);
            foreach (var (rel, file) in files.Where(p => p.Key.EndsWith(".ide", StringComparison.Ordinal)))
                foreach (var line in File.ReadLines(file, Encoding.GetEncoding(1252)))
                {
                    var fields = line.Split(',').Select(s => s.Trim().ToLowerInvariant()).ToArray();
                    if (fields.Length >= 4 && int.TryParse(fields[0], out _) && fields[2] is "trees" or "trees2" or "tree1" or "tree2") vegetation.Add(fields[1]);
                }
            var vehicles = selected.Values.Where(e => Path.GetFileNameWithoutExtension(e.Archive) == "xbox_vehicles" && e.Name.EndsWith(".dff", StringComparison.Ordinal))
                .Select(e => e.Name[..^4]).Order(StringComparer.Ordinal).ToArray();
            File.WriteAllText(Path.Combine(stage, "vegetation_models.txt"), string.Join('\n', vegetation) + "\n");
            File.WriteAllText(Path.Combine(stage, "vehicle_models.txt"), string.Join('\n', vehicles) + "\n");
            File.WriteAllText(Path.Combine(stage, "asset_categories.json"), JsonSerializer.Serialize(new
            {
                archives = Pack.Archives, entries = selected.Count, vehicles = vehicles.Length, vegetation = vegetation.Count,
                world = "World models/textures, map IDE/IPL/COL, HUD/fonts/particles and splash textures",
                peds = "Player and pedestrian models/textures, matching default Claude bitmap", weapons = "Weapon model IDs and textures",
                excluded = "PC binaries, scripts, settings, saves, audio, skins, neo configuration; retail vehicle collisions retained"
            }, new JsonSerializerOptions { WriteIndented = true }));
            List<string> manifest = ["Profile=MODERN", "BuilderVersion=" + Pack.BuilderVersion, "SourceSHA256=" + Pack.ArchiveSha256];
            foreach (var file in SafeFiles(stage).Order(StringComparer.Ordinal))
                manifest.Add("SHA256 " + Path.GetRelativePath(stage, file).Replace('\\', '/') + "=" + Digests.Sha256(file));
            File.WriteAllText(Path.Combine(stage, "BUILD_INFO.txt"), string.Join('\n', manifest) + "\n");
            ValidateOverlay(stage);
            var existed = Directory.Exists(output);
            if (existed) Directory.Move(output, backup);
            try { Directory.Move(stage, output); }
            catch { if (existed) Directory.Move(backup, output); throw; }
            if (existed) Directory.Delete(backup, true);
            log.Write("Modern overlay built and verified: " + output);
            return output;
        }
        finally { if (Directory.Exists(stage)) Directory.Delete(stage, true); if (Directory.Exists(scratch)) Directory.Delete(scratch, true); }
    }

    private async Task<string> Compressor(CancellationToken token)
    {
        var bundled = Path.Combine(settings.Repository, "tools/modelsets/txdcompress.exe");
        if (OperatingSystem.IsWindows() && File.Exists(bundled)) return bundled;
        var output = Path.Combine(settings.Workplace, "modern-assets/txdcompress" + (OperatingSystem.IsWindows() ? ".exe" : ""));
        var source = Path.Combine(settings.Repository, "tools/modelsets/txdcompress.cpp");
        if (!File.Exists(output) || File.GetLastWriteTimeUtc(source) > File.GetLastWriteTimeUtc(output))
            await runner.Run(Paths.FindExecutable("g++") ?? throw new IOException("Install a C++ compiler to build the texture compressor."), ["-O2", "-o", output, source], token: token);
        return output;
    }

    private static void CopyBytes(Stream source, Stream destination, long count)
    {
        var buffer = new byte[1024 * 1024];
        while (count > 0)
        {
            var read = source.Read(buffer, 0, (int)Math.Min(count, buffer.Length));
            if (read == 0) throw new EndOfStreamException("Truncated IMG archive.");
            destination.Write(buffer, 0, read); count -= read;
        }
    }

    public static int RepairWheelTextures(byte[] bytes)
    {
        var repaired = 0;
        void Visit(int begin, int end)
        {
            var offset = begin;
            while (offset < end)
            {
                if (offset + 12 > end) throw new InvalidDataException("Truncated wheel chunk header.");
                var kind = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(offset, 4));
                var size = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(offset + 4, 4));
                var start = offset + 12; var stop = checked(start + (int)size);
                if (stop > end) throw new InvalidDataException("Truncated wheel chunk payload.");
                if (kind == 6)
                {
                    for (var child = start; child < stop;)
                    {
                        if (child + 12 > stop) throw new InvalidDataException("Truncated wheel texture chunk.");
                        var tag = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(child, 4));
                        var length = checked((int)BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(child + 4, 4)));
                        var value = child + 12;
                        if (value + length > stop) throw new InvalidDataException("Truncated wheel texture string.");
                        if (tag == 2)
                        {
                            if (Encoding.ASCII.GetString(bytes, value, length).Split('\0')[0] == "wheel_lightvan64back")
                            { Array.Clear(bytes, value, length); Encoding.ASCII.GetBytes("wheel_lightvan64").CopyTo(bytes, value); repaired++; }
                            break;
                        }
                        child = value + length;
                    }
                }
                else if (kind is 16 or 26 or 15 or 8 or 7) Visit(start, stop);
                offset = stop;
            }
        }
        Visit(0, bytes.Length); return repaired;
    }

    public async Task Install(QuestDevice device, string output, CancellationToken token)
    {
        var hashes = ValidateOverlay(output);
        var installed = await device.Run(["shell", "pm", "path", QuestDevice.Package], token);
        if (!installed.Output.Contains("package:")) throw new IOException("Install Liberty City VR before installing models.");
        await device.Bootstrap(token);
        await device.Run(["shell", "am", "force-stop", QuestDevice.Package], token);
        var root = QuestDevice.GameData + "/modelsets";
        var stamp = Guid.NewGuid().ToString("N");
        var stage = root + "/.modern-incoming-" + stamp;
        var backup = root + "/.modern-backup-" + stamp;
        var final = root + "/modern";
        var activated = false;
        try
        {
            foreach (var (relative, expectedHash) in hashes.Append(new KeyValuePair<string, string>("BUILD_INFO.txt", Digests.Sha256(Path.Combine(output, "BUILD_INFO.txt")))))
            {
                var remote = stage + "/" + relative;
                await device.Shell("mkdir -p " + QuestDevice.Quote(remote[..remote.LastIndexOf('/')]), token);
                await device.Push(Paths.Child(output, relative), remote, token, expectedHash);
            }
            await device.Shell("chmod -R a+rX " + QuestDevice.Quote(stage), token);
            // One remote shell transaction restores the old profile if activation fails.
            var qFinal = QuestDevice.Quote(final); var qBackup = QuestDevice.Quote(backup); var qStage = QuestDevice.Quote(stage);
            await device.Shell($"if [ -d {qFinal} ]; then mv {qFinal} {qBackup} || exit 1; fi; if mv {qStage} {qFinal}; then :; else if [ -d {qBackup} ]; then mv {qBackup} {qFinal}; fi; exit 1; fi", token);
            activated = true;
            await device.Shell("rm -rf " + qBackup, token);
            log.Write("Modern models installed. Select Modern under MODEL ASSETS and fully restart the game.");
        }
        finally
        {
            if (!activated)
            {
                using var cleanup = new CancellationTokenSource(TimeSpan.FromSeconds(10));
                try { await device.Shell("rm -rf " + QuestDevice.Quote(stage), cleanup.Token, check: false); }
                catch (Exception error) when (error is IOException or OperationCanceledException) { log.Write("Staged model cleanup failed; inspect the local action log before retrying."); }
            }
        }
    }
}
