using System.Collections;
using System.Resources;
using System.Text.RegularExpressions;

namespace LibertyManager;

public sealed record AuxiliaryTool(string Path, string Category, string Description, string[] Parameters)
{
    public override string ToString() => System.IO.Path.GetFileNameWithoutExtension(Path);
}

/// <summary>
/// Embedded auxiliary implementations preserve specialised authoring and regression tools.
/// They are unpacked only into an isolated, local, source-only workspace, never into the checkout.
/// Primary workflows use native C# services, not these compatibility modules.
/// </summary>
public sealed class AuxiliaryTools(ManagerSettings settings, ICommandRunner runner, PrivateLog log, Func<int, Task<int?>>? chooseDevice = null)
{
    public static IReadOnlyDictionary<string, string> Sources { get; } = LoadSources();
    private static IReadOnlyDictionary<string, string> LoadSources()
    {
        var resources = new ResourceManager("LibertyManager.EmbeddedTools", typeof(AuxiliaryTools).Assembly);
        var set = resources.GetResourceSet(System.Globalization.CultureInfo.InvariantCulture, true, true)!;
        return set.Cast<DictionaryEntry>().ToDictionary(e => (string)e.Key, e => (string)e.Value!, StringComparer.Ordinal);
    }
    private static readonly HashSet<string> Primary = new(StringComparer.Ordinal)
    {
        "assemble", "build-and-install", "update-and-install", "import-music", "install-game-files", "install-modern-models",
        "prepare-modern-models", "prepare-xbox-models", "build-modern-modelset", "build-xbox-modelset", "xbox-modelset", "privacy_guard", "device-diagnostics"
    };
    public static IEnumerable<AuxiliaryTool> Catalog => Sources.Keys
        .Where(p => p != "overlay/android/diagnostics/asan-wrap.sh" &&
            !Primary.Contains(Path.GetFileNameWithoutExtension(p)) && p.Contains('/') && !p.EndsWith("/build.ps1", StringComparison.Ordinal) && !p.EndsWith("/build.sh", StringComparison.Ordinal))
        .GroupBy(p => p[..p.LastIndexOf('.')], StringComparer.Ordinal)
        .Select(g => g.OrderBy(p => Preference(Path.GetExtension(p))).First())
        .Select(p => new AuxiliaryTool(p, p.Contains("test", StringComparison.OrdinalIgnoreCase) ? "Tests" : "Tools", Summary(Sources[p]), Parameters(Sources[p], p)))
        .OrderBy(t => t.Category).ThenBy(t => t.Path, StringComparer.Ordinal)
        .Prepend(new AuxiliaryTool("manager-tests", "Tests", "Run the active C# manager regression checks. Requires the .NET 10 SDK.", []));
    private static int Preference(string extension) => extension == ".py" ? 0 : extension == (OperatingSystem.IsWindows() ? ".ps1" : ".sh") ? 1 : 2;
    private static string Summary(string source) => source.Split('\n').SkipWhile(l => l.StartsWith("#!", StringComparison.Ordinal)).Take(6)
        .FirstOrDefault(l => l.TrimStart().StartsWith("# ", StringComparison.Ordinal) || l.Contains("\"\"\"", StringComparison.Ordinal))?.Trim(' ', '#', '"') ?? "Integrated auxiliary module";
    private static string[] Parameters(string source, string path) => path.EndsWith(".ps1", StringComparison.Ordinal)
        ? Regex.Matches(source.Split("$ErrorActionPreference", 2)[0], @"\[(?:string|int)\]\$(\w+)").Select(m => "-" + m.Groups[1].Value).Where(p => p != "-Serial").Distinct().ToArray()
        : Regex.Matches(source, @"add_argument\(['""'](--[a-z][a-z0-9-]+)").Select(m => m.Groups[1].Value).Where(p => p is not ("--serial" or "--adb")).Distinct().ToArray();

    public static string[] ParseArguments(string text)
    {
        var result = new List<string>(); var word = new System.Text.StringBuilder(); char quote = '\0'; var present = false;
        foreach (var c in text)
        {
            if (quote != '\0') { if (c == quote) quote = '\0'; else word.Append(c); present = true; }
            else if (c is '\'' or '"') { quote = c; present = true; }
            else if (char.IsWhiteSpace(c)) { if (present) { result.Add(word.ToString()); word.Clear(); present = false; } }
            else { word.Append(c); present = true; }
        }
        if (quote != '\0') throw new ArgumentException("Close the quoted argument.");
        if (present) result.Add(word.ToString());
        return result.ToArray();
    }

    public string ResolveRuntime(string extension)
    {
        var configured = extension switch { ".py" => settings.Python, ".ps1" => settings.PowerShell, ".sh" => settings.Bash, _ => "" };
        if (configured.Length > 0) return Paths.FindExecutable(configured) ?? throw new IOException("Configured auxiliary runtime was not found.");
        string[] candidates = extension switch { ".py" => ["python3", "python"], ".ps1" => ["pwsh", "powershell"], ".sh" => ["bash"], _ => [] };
        return candidates.Select(Paths.FindExecutable).FirstOrDefault(p => p is not null && !p.Contains("WindowsApps", StringComparison.OrdinalIgnoreCase))
            ?? throw new IOException("Configure " + (extension == ".py" ? "Python 3" : extension == ".ps1" ? "PowerShell 7" : "Bash") + " in Settings for this auxiliary tool.");
    }

    public async Task Run(string module, string[] arguments, CancellationToken token)
    {
        settings.Validate();
        if (module == "manager-tests")
        {
            await runner.Run(Paths.FindExecutable("dotnet") ?? throw new IOException("Install the .NET 10 SDK for manager checks."),
                new[] { "run", "--project", Path.Combine(settings.Repository, "tools/manager/tests"), "-c", "Release", "--" }.Concat(arguments), settings.Repository, token: token);
            return;
        }
        if (!Catalog.Any(t => t.Path == module)) throw new ArgumentException("Unknown auxiliary module. Use --list-tools; primary workflows run through their native action pages.");
        var interpreter = ResolveRuntime(Path.GetExtension(module));
        var adb = new Toolchain(settings, runner as CommandRunner ?? new CommandRunner(log), log).FindAdb();
        if (runner is CommandRunner native)
        {
            native.EnvironmentOverrides["PYTHONIOENCODING"] = "utf-8";
            if (adb is not null) native.EnvironmentOverrides["PATH"] = Path.GetDirectoryName(adb) + Path.PathSeparator + Environment.GetEnvironmentVariable("PATH");
            native.EnvironmentOverrides["ANDROID_HOME"] = new Toolchain(settings, native, log).Sdk;
        }
        QuestDevice? selectedDevice = null;
        if (adb is not null && Catalog.First(t => t.Path == module).Category != "Tests")
        {
            using var limit = CancellationTokenSource.CreateLinkedTokenSource(token); limit.CancelAfter(TimeSpan.FromSeconds(15));
            var discovery = await runner.Run(adb, ["devices", "-l"], showOutput: false, check: false, token: limit.Token);
            foreach (var line in discovery.Output.Split('\n'))
            {
                var match = Regex.Match(line, @"^(\S+)\s+(?:device|offline|unauthorized)(?:\s|$)");
                if (match.Success) log.Remember(match.Groups[1].Value);
            }
        }
        if (Regex.IsMatch(Sources[module].Split("$ErrorActionPreference", 2)[0], @"\[string\]\$Serial\b", RegexOptions.IgnoreCase))
        {
            if (adb is null) throw new IOException("Configure ADB before using this device tool.");
            selectedDevice = await QuestDevice.Connect(runner, log, adb, chooseDevice, token) ?? throw new IOException("An authorized Quest 3 is required.");
            if (arguments.Any(a => a.Equals("-Serial", StringComparison.OrdinalIgnoreCase))) throw new ArgumentException("Device selection is managed by the application. Omit -Serial.");
            arguments = arguments.Concat(new[] { "-Serial", "__LCVR_SELECTED_DEVICE__" }).ToArray();
        }
        // Only tracked public source is mirrored. No out/, work/, .cache/, retail inputs, saves or Git metadata.
        var git = Paths.FindExecutable("git") ?? throw new IOException("Git is required to create the source-only auxiliary workspace.");
        var tracked = await runner.Run(git, ["-C", settings.Repository, "ls-files", "-z"], showOutput: false, token: token);
        var workspace = Path.Combine(settings.Workplace, "auxiliary", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(workspace);
        if (runner is CommandRunner commands)
        {
            var temporary = Path.Combine(workspace, ".tmp"); Directory.CreateDirectory(temporary);
            foreach (var variable in new[] { "TEMP", "TMP", "TMPDIR" }) commands.EnvironmentOverrides[variable] = temporary;
        }
        try
        {
            foreach (var relative in tracked.Output.Split('\0', StringSplitOptions.RemoveEmptyEntries))
            {
                token.ThrowIfCancellationRequested();
                if (string.IsNullOrWhiteSpace(relative)) continue;
                if (Sources.ContainsKey(relative)) continue;
                var source = Paths.Child(settings.Repository, relative);
                if (!File.Exists(source)) continue;
                var target = Paths.Child(workspace, relative);
                Directory.CreateDirectory(Path.GetDirectoryName(target)!); File.Copy(source, target);
            }
            foreach (var (relative, code) in Sources)
            {
                var target = Paths.Child(workspace, relative);
                Directory.CreateDirectory(Path.GetDirectoryName(target)!); File.WriteAllText(target, code);
                if (!OperatingSystem.IsWindows() && relative.EndsWith(".sh", StringComparison.Ordinal))
                    File.SetUnixFileMode(target, UnixFileMode.UserRead | UnixFileMode.UserWrite | UnixFileMode.UserExecute);
            }
            var extension = Path.GetExtension(module);
            var args = (extension == ".ps1" ? new[] { "-NoProfile", "-NonInteractive", "-File", Path.Combine(workspace, module) } : new[] { Path.Combine(workspace, module) }).Concat(arguments).ToArray();
            log.Write("Running integrated " + module + ". Temporary source workspace: " + workspace);
            // Source snapshots and successful exports are retained, so implicit relative output paths remain usable.
            if (selectedDevice is not null) await selectedDevice.RunAuxiliary(interpreter, args, workspace, token);
            else await runner.Run(interpreter, args, workspace, token: token);
            log.Write("Auxiliary tool complete. Outputs: " + workspace);
        }
        catch { log.Write("Auxiliary source workspace retained for diagnostics: " + workspace); throw; }
    }

    public async Task Bridge(string[] arguments, CancellationToken token)
    {
        const string key = "tools/vehicle-editor/device_bridge.py";
        var root = Path.Combine(settings.Workplace, ".tmp", "lcvr-bridge-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        try
        {
            var script = Path.Combine(root, "bridge.py"); File.WriteAllText(script, Sources[key]);
            if (runner is CommandRunner commands)
                foreach (var variable in new[] { "TEMP", "TMP", "TMPDIR" }) commands.EnvironmentOverrides[variable] = root;
            await runner.Run(ResolveRuntime(".py"), new[] { script }.Concat(arguments), token: token);
        }
        finally { Directory.Delete(root, true); }
    }
}
