using System.Reflection;
using System.Text;
using System.Text.Json;

namespace LibertyManager;

public sealed record XboxDisc(string BuilderVersion, string Disc, Dictionary<string, PackFile> Files);

public sealed class OriginalXboxModels(ManagerSettings settings, ICommandRunner runner, PrivateLog log)
{
    public static XboxDisc Disc { get; } = ReadDisc();
    public const string Identity = "ORIGINAL_XBOX_US_1_0";
    private static XboxDisc ReadDisc()
    {
        var assembly = Assembly.GetExecutingAssembly();
        using var stream = assembly.GetManifestResourceStream(assembly.GetManifestResourceNames().Single(n => n.EndsWith("xbox-disc.json")))!;
        return JsonSerializer.Deserialize<XboxDisc>(stream)!;
    }
    public static IEnumerable<string> SafeFiles(string root)
    {
        Paths.NoLinks(root);
        foreach (var entry in Directory.EnumerateFileSystemEntries(root))
        {
            Paths.NoLinks(entry);
            if (Directory.Exists(entry)) { foreach (var file in SafeFiles(entry)) yield return file; }
            else yield return entry;
        }
    }
    public static Dictionary<string, string> ValidateOverlay(string root)
    {
        Paths.NoLinks(root);
        var info = File.ReadAllLines(Path.Combine(root, "BUILD_INFO.txt")).Where(l => l.Contains('='))
            .Select(l => l.Split('=', 2)).ToDictionary(p => p[0], p => p[1]);
        if (info.GetValueOrDefault("Profile") != "MODERN" || info.GetValueOrDefault("Source") != Identity ||
            info.GetValueOrDefault("BuilderVersion") != Disc.BuilderVersion) throw new InvalidDataException("Expected a verified Original Xbox US overlay.");
        var hashes = info.Where(p => p.Key.StartsWith("SHA256 ", StringComparison.Ordinal)).ToDictionary(p => p.Key[7..], p => p.Value);
        string[] required = ["models/gta3.img", "models/gta3.dir", "models/generic/player.bmp", "models/generic.txd", "data/default.ide", "data/particle.cfg", "neo/tuning.bin", "neo/neo.txd", "vegetation_models.txt", "vehicle_models.txt", "asset_categories.json"];
        if (required.Concat(OriginalXboxAudio.RequiredFiles).Any(p => !hashes.ContainsKey(p))) throw new InvalidDataException("Original Xbox manifest lacks required files. Reimport your ISO to include audio.");
        if (!File.ReadAllLines(Path.Combine(root, "audio/READY.txt")).SequenceEqual(new[] { OriginalXboxAudio.Identity }.Concat(OriginalXboxAudio.RequiredFiles.Where(p => p != "audio/READY.txt").Order(StringComparer.Ordinal))))
            throw new InvalidDataException("Invalid Xbox audio inventory.");
        var actual = SafeFiles(root).Select(p => Path.GetRelativePath(root, p).Replace('\\', '/')).ToHashSet(StringComparer.Ordinal);
        if (!actual.SetEquals(hashes.Keys.Append("BUILD_INFO.txt"))) throw new InvalidDataException("Overlay has missing or unmanifested files.");
        foreach (var (relative, hash) in hashes)
            if (!Digests.Sha256(Paths.Child(root, relative)).Equals(hash, StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Overlay checksum mismatch: " + relative);
        long expected = 0;
        foreach (var entry in ModernModels.ReadImg(Path.Combine(root, "models/gta3.img"), Path.Combine(root, "models/gta3.dir")))
        { if (entry.Sector != expected) throw new InvalidDataException("Generated IMG is not compact."); expected += entry.Count; }
        if (expected * 2048 != new FileInfo(Path.Combine(root, "models/gta3.img")).Length) throw new InvalidDataException("Unexpected IMG bytes.");
        return hashes;
    }
    public async Task<string> Prepare(ActionOptions options, CancellationToken token)
    {
        if (options.DownloadModels || !string.IsNullOrWhiteSpace(options.ModelSource))
            throw new InvalidDataException("Original Xbox preparation accepts only a user-provided US 1.0 .iso or .xiso.iso. Downloads and extracted mod packs are not supported.");
        if (string.IsNullOrWhiteSpace(options.ModelArchive) || !File.Exists(options.ModelArchive))
            throw new IOException("Select your local Original Xbox GTA III US 1.0 .iso or .xiso.iso; the Manager never downloads game assets.");
        var output = string.IsNullOrWhiteSpace(options.ModelOutput) ? Path.Combine(settings.Workplace, options.CompleteVariant ? "prepared-variants/xbox" : "original-xbox/modelsets/modern") : Path.GetFullPath(options.ModelOutput);
        if (Path.GetFileName(Path.TrimEndingDirectorySeparator(output)) != (options.CompleteVariant ? "xbox" : "modern") || Paths.Within(settings.Repository, output) || Paths.Within(output, settings.Repository) || Paths.Within(output, options.ModelArchive))
            throw new IOException("Choose a separate prepared output folder outside the repository and source ISO.");
        Paths.NoLinks(output);
        if (Directory.Exists(output) && (!File.Exists(Path.Combine(output, "BUILD_INFO.txt")) || !File.ReadAllLines(Path.Combine(output, "BUILD_INFO.txt")).Contains("Profile=MODERN")))
            throw new IOException("Refusing to replace an unmarked folder.");
        var parent = Path.GetDirectoryName(output)!; Directory.CreateDirectory(parent);
        var scratch = Path.Combine(parent, ".original-xbox-source-" + Guid.NewGuid().ToString("N"));
        var stage = Path.Combine(parent, ".original-xbox-build-" + Guid.NewGuid().ToString("N"));
        var backup = Path.Combine(parent, ".original-xbox-previous-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(scratch); Directory.CreateDirectory(stage);
        try
        {
            using (var iso = new XboxIso(options.ModelArchive))
            {
                log.Write("Reading and verifying Original Xbox US disc assets locally. The source image is read-only.");
                foreach (var (relative, expected) in Disc.Files)
                {
                    token.ThrowIfCancellationRequested();
                    if (!iso.Files.TryGetValue(relative, out var file) || file.Length != expected.Bytes) throw new InvalidDataException("Unsupported Xbox disc revision or missing asset: " + relative);
                    var target = Paths.Child(scratch, "disc/" + relative); iso.Extract(relative, target, token);
                    if (Digests.Sha256(target) != expected.Sha256) throw new InvalidDataException("Xbox US source fingerprint mismatch: " + relative);
                }
            }
            var raw = Path.Combine(scratch, "assets"); Directory.CreateDirectory(raw); Directory.CreateDirectory(Path.Combine(raw, "archive"));
            var discRoot = Path.Combine(scratch, "disc");
            var selected = ModernModels.ReadImg(Path.Combine(discRoot, "models/gta3.img"), Path.Combine(discRoot, "models/gta3.dir"), true, true).ToArray();
            foreach (var entry in selected)
            {
                using var img = File.OpenRead(entry.Archive); img.Position = entry.Sector * 2048L;
                using var target = File.Create(Paths.Child(raw, "archive/" + entry.Name));
                CopyBytes(img, target, entry.Count * 2048L, token);
            }
            foreach (var file in SafeFiles(discRoot))
            {
                var relative = Path.GetRelativePath(discRoot, file).Replace('\\', '/');
                if (relative.EndsWith(".dff", StringComparison.Ordinal) || relative.EndsWith(".txd", StringComparison.Ordinal))
                {
                    var target = Paths.Child(raw, relative); Directory.CreateDirectory(Path.GetDirectoryName(target)!); File.Copy(file, target);
                }
                else if (relative.StartsWith("data/maps/", StringComparison.Ordinal) || relative == "data/default.ide" ||
                         relative.StartsWith("models/coll/", StringComparison.Ordinal) && (options.CompleteVariant || !relative.EndsWith("vehicles.col", StringComparison.Ordinal)) ||
                         options.CompleteVariant && (relative.StartsWith("data/", StringComparison.Ordinal) || relative.StartsWith("text/", StringComparison.Ordinal) || relative.StartsWith("anim/", StringComparison.Ordinal)))
                {
                    var target = Paths.Child(stage, relative); Directory.CreateDirectory(Path.GetDirectoryName(target)!); File.Copy(file, target);
                }
            }
            File.WriteAllText(Path.Combine(stage, "data/particle.cfg"), OriginalXboxParticles.Normalize(File.ReadAllText(Path.Combine(discRoot, "data/particle.cfg"))));
            var converter = await Converter(token);
            if(options.CompleteVariant) {
                await runner.Run(converter,["--frontend",Path.Combine(stage,"models/frontend.txd")],token:token);
                File.WriteAllText(Path.Combine(stage,"PORT_FRONTEND.txt"),"Liberty City VR generated frontend icons v1\n");
            }
            await OriginalXboxEffects.Prepare(settings, runner, discRoot, stage, scratch, converter, token);
            log.Write($"Converting {selected.Length} archived assets and loose dictionaries/models; preserving original dimensions and mip levels.");
            var conversion = await runner.Run(converter, [raw, stage], showOutput: false, check: false, token: token);
            if (conversion.ExitCode != 0)
                throw new InvalidDataException("Xbox conversion failed: " + string.Join("; ", conversion.Output.Split('\n', StringSplitOptions.RemoveEmptyEntries).TakeLast(3)));
            var skin = Path.Combine(stage, "archive/player.bmp");
            if (!File.Exists(skin)) throw new InvalidDataException("Original Xbox player texture could not be exported.");
            Directory.CreateDirectory(Path.Combine(stage, "models/generic")); File.Move(skin, Path.Combine(stage, "models/generic/player.bmp"));
            WriteImg(Path.Combine(stage, "archive"), Path.Combine(stage, "models"), token);
            Directory.Delete(Path.Combine(stage, "archive"), true);
            Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);
            var vegetation = new SortedSet<string>(StringComparer.Ordinal); var vehicles = new SortedSet<string>(StringComparer.Ordinal);
            foreach (var file in SafeFiles(stage).Where(f => f.EndsWith(".ide", StringComparison.Ordinal)))
            {
                var section = "";
                foreach (var line in File.ReadLines(file, Encoding.GetEncoding(1252)))
                {
                    var fields = line.Split('#')[0].Trim().ToLowerInvariant().Split(',').Select(s => s.Trim()).ToArray();
                    if (fields.Length == 1) { if (fields[0].Length > 0) section = fields[0]; continue; }
                    if (fields.Length < 3 || !int.TryParse(fields[0], out _)) continue;
                    if (section == "cars") vehicles.Add(fields[1]);
                    if (fields[2] is "trees" or "trees2" or "tree1" or "tree2") vegetation.Add(fields[1]);
                }
            }
            File.WriteAllLines(Path.Combine(stage, "vegetation_models.txt"), vegetation);
            File.WriteAllLines(Path.Combine(stage, "vehicle_models.txt"), vehicles);
            await new OriginalXboxAudio(settings, runner, log).Prepare(discRoot, stage, scratch, token);
            File.WriteAllText(Path.Combine(stage, "asset_categories.json"), JsonSerializer.Serialize(new {
                source = Disc.Disc, entries = selected.Length, vehicles = vehicles.Count, vegetation = vegetation.Count,
                audio = "Xbox SFX decoded to PCM16 with rebuilt SDT; speech and nine radio stations decoded to lossless FLAC at their authored rates",
                excluded = options.CompleteVariant ? "Xbox executable, console frontend runtime and cutscene ANM/hand extensions; scripts, gameplay data, collision and standard animations are included from the disc" :
                    "Xbox executable, scripts, console frontend, cutscene ANM/hand extensions; identical PC gameplay and cutscene animation archives retained",
                rendering = "Quest Vulkan Neo adaptation: Xbox particles, world lightmaps, rim lighting, road gloss and vehicle highlights; optional stereo visor rain droplets; Quest reflection source retained"
            }, new JsonSerializerOptions { WriteIndented = true }));
            List<string> manifest = ["Profile=MODERN", "Source=" + Identity, "BuilderVersion=" + Disc.BuilderVersion];
            foreach (var file in SafeFiles(stage).Order(StringComparer.Ordinal)) manifest.Add("SHA256 " + Path.GetRelativePath(stage, file).Replace('\\', '/') + "=" + Digests.Sha256(file));
            File.WriteAllLines(Path.Combine(stage, "BUILD_INFO.txt"), manifest); ValidateOverlay(stage);
            var existed = Directory.Exists(output); if (existed) Directory.Move(output, backup);
            try { Directory.Move(stage, output); } catch { if (existed) Directory.Move(backup, output); throw; }
            if (existed) Directory.Delete(backup, true);
            log.Write((options.CompleteVariant ? "Complete Original Xbox game content" : "Original Xbox overlay") + " converted and verified: " + output); return output;
        }
        finally
        {
            if (Directory.Exists(stage)) Directory.Delete(stage, true);
            if (Directory.Exists(scratch)) Directory.Delete(scratch, true);
        }
    }
    private async Task<string> Converter(CancellationToken token)
    {
        if (runner is CommandRunner commands) await WindowsCompiler.Configure(commands, token);
        var tools = new Toolchain(settings, runner as CommandRunner ?? new CommandRunner(log), log);
        var cmake = tools.FindCMake(); var build = Path.Combine(settings.Workplace, "original-xbox/converter");
        var args = new List<string> { "-S", Path.Combine(settings.Repository, "tools/modelsets"), "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_TRY_COMPILE_CONFIGURATION=Release" };
        var ninja = Paths.FindExecutable("ninja") ?? Path.Combine(Path.GetDirectoryName(cmake)!, "ninja" + (OperatingSystem.IsWindows() ? ".exe" : ""));
        if (File.Exists(ninja)) args.Add("-DCMAKE_MAKE_PROGRAM=" + ninja);
        await runner.Run(cmake, args, token: token);
        await runner.Run(cmake, ["--build", build, "--config", "Release", "--parallel", "4"], token: token);
        var ctest = Path.Combine(Path.GetDirectoryName(cmake)!, "ctest" + (OperatingSystem.IsWindows() ? ".exe" : ""));
        await runner.Run(ctest, ["--test-dir", build, "--output-on-failure", "-C", "Release"], token: token);
        return Path.Combine(build, "xbox-convert" + (OperatingSystem.IsWindows() ? ".exe" : ""));
    }
    private static void CopyBytes(Stream source, Stream output, long count, CancellationToken token)
    {
        var buffer = new byte[1024 * 1024];
        while (count > 0)
        {
            token.ThrowIfCancellationRequested(); var n = source.Read(buffer, 0, (int)Math.Min(count, buffer.Length));
            if (n == 0) throw new EndOfStreamException("Truncated IMG asset."); output.Write(buffer, 0, n); count -= n;
        }
    }
    private static void WriteImg(string source, string models, CancellationToken token)
    {
        Directory.CreateDirectory(models);
        using var img = File.Create(Path.Combine(models, "gta3.img")); using var dir = new BinaryWriter(File.Create(Path.Combine(models, "gta3.dir")));
        uint sector = 0;
        foreach (var file in Directory.GetFiles(source).Order(StringComparer.Ordinal))
        {
            token.ThrowIfCancellationRequested(); var size = new FileInfo(file).Length; var count = checked((uint)((size + 2047) / 2048));
            dir.Write(sector); dir.Write(count); var name = new byte[24]; Encoding.ASCII.GetBytes(Path.GetFileName(file)).CopyTo(name, 0); dir.Write(name);
            using var input = File.OpenRead(file); input.CopyTo(img); img.Write(new byte[count * 2048L - size]); sector = checked(sector + count);
        }
    }
}
