using System.Diagnostics;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;

namespace LibertyManager;

public sealed class PrivateLog : IDisposable
{
    private readonly List<string> secrets = [];
    private readonly object gate = new();
    private readonly StreamWriter? writer;
    public event Action<string>? Line;
    public PrivateLog(string? path = null)
    {
        if (path is null) return;
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        writer = new StreamWriter(path, append: false) { AutoFlush = true };
        if (!OperatingSystem.IsWindows()) File.SetUnixFileMode(path, UnixFileMode.UserRead | UnixFileMode.UserWrite);
    }
    public void Remember(string secret) { if (secret.Length > 0) lock (gate) secrets.Add(secret); }
    public string Sanitize(string value)
    {
        lock (gate)
            foreach (var secret in secrets.OrderByDescending(s => s.Length))
                value = value.Replace(secret, "personal Quest 3 device", StringComparison.OrdinalIgnoreCase);
        value = Regex.Replace(value, @"(?<![A-Za-z0-9])2G[A-Z0-9]{12}(?![A-Za-z0-9])", "personal Quest 3 device", RegexOptions.IgnoreCase);
        value = Regex.Replace(value, @"\b[A-Za-z0-9.-]+\.ts\.net\b", "[private host]", RegexOptions.IgnoreCase);
        // Do not allow child tools to inject terminal control sequences.
        return Regex.Replace(value, @"\x1B(?:\[[0-?]*[ -/]*[@-~]|\][^\x07]*(?:\x07|$))|[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]", "");
    }
    public void Write(string value)
    {
        var safe = Sanitize(value);
        lock (gate) writer?.WriteLine(safe);
        Line?.Invoke(safe);
    }
    public void Dispose() => writer?.Dispose();
}

public sealed record CommandResult(int ExitCode, string Output)
{
    // Keep combined diagnostics for existing callers (e.g. java -version),
    // but structured-output consumers must never parse stderr as JSON.
    public string StandardOutput { get; init; } = Output;
}

public interface ICommandRunner
{
    Task<CommandResult> Run(string executable, IEnumerable<string> args, string? workingDirectory = null,
        bool showOutput = true, bool check = true, CancellationToken token = default, string? input = null);
}

public sealed class CommandRunner(PrivateLog log) : ICommandRunner
{
    public TimeSpan CommandTimeout { get; set; } = TimeSpan.FromHours(2);
    public Dictionary<string, string> EnvironmentOverrides { get; } = new(OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal);
    public async Task<CommandResult> Run(string executable, IEnumerable<string> args, string? workingDirectory = null,
        bool showOutput = true, bool check = true, CancellationToken token = default, string? input = null)
    {
        var start = new ProcessStartInfo(executable)
        {
            UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true,
            RedirectStandardInput = true, CreateNoWindow = true, WorkingDirectory = workingDirectory ?? System.Environment.CurrentDirectory
        };
        foreach (var arg in args) start.ArgumentList.Add(arg);
        foreach (var (name, value) in EnvironmentOverrides) start.Environment[name] = value;
        using var timeout = CancellationTokenSource.CreateLinkedTokenSource(token);
        timeout.CancelAfter(CommandTimeout);
        using var process = new Process { StartInfo = start };
        if (!process.Start()) throw new IOException("Could not start the required tool.");
        using var registration = timeout.Token.Register(() => { try { process.Kill(entireProcessTree: true); } catch (InvalidOperationException) { } });
        var output = new StringBuilder();
        var standardOutput = new StringBuilder();
        async Task Drain(StreamReader stream, bool isStandardOutput)
        {
            while (await stream.ReadLineAsync() is { } line)
            {
                lock (output) { if (output.Length < 8 * 1024 * 1024) output.AppendLine(line); }
                if (isStandardOutput && standardOutput.Length < 8 * 1024 * 1024) standardOutput.AppendLine(line);
                if (showOutput) log.Write(line);
            }
        }
        var drain = Task.WhenAll(Drain(process.StandardOutput, true), Drain(process.StandardError, false));
        try
        {
            if (input is not null) await process.StandardInput.WriteAsync(input.AsMemory(), timeout.Token);
            process.StandardInput.Close();
            await process.WaitForExitAsync(timeout.Token);
            await drain;
        }
        catch { try { process.Kill(true); } catch (InvalidOperationException) { } await drain; throw; }
        if (check && process.ExitCode != 0)
            throw new IOException($"{Path.GetFileName(executable)} failed (exit {process.ExitCode}). " +
                (showOutput ? "See the action log." : "Private diagnostics suppressed."));
        return new(process.ExitCode, output.ToString()) { StandardOutput = standardOutput.ToString() };
    }
}

public static class Digests
{
    public static string Sha256(string file) { using var stream = File.OpenRead(file); return Convert.ToHexString(SHA256.HashData(stream)); }
}

public sealed class QuestDevice(ICommandRunner runner, PrivateLog log, string executable, string serial)
{
    public const string Package = "com.libertyvr.quest";
    public const string GameData = "/sdcard/Android/data/" + Package + "/files/gamedata";
    public const string SaveUri = "content://" + Package + ".saves";
    public Task<CommandResult> Run(IEnumerable<string> args, CancellationToken token = default, bool check = true)
        => runner.Run(executable, new[] { "-s", serial }.Concat(args), showOutput: false, check: check, token: token);
    internal Task<CommandResult> RunAuxiliary(string interpreter, IEnumerable<string> args, string directory, CancellationToken token)
        => runner.Run(interpreter, args.Select(a => a == "__LCVR_SELECTED_DEVICE__" ? serial : a), directory, token: token);
    public Task<CommandResult> Shell(string command, CancellationToken token = default, bool check = true)
        => Run(["shell", command], token, check);
    public static string Quote(string text) => "'" + text.Replace("'", "'\"'\"'") + "'";

    public static async Task<QuestDevice?> Connect(ICommandRunner runner, PrivateLog log, string adb,
        Func<int, Task<int?>>? choose, CancellationToken token)
    {
        using var discovery = CancellationTokenSource.CreateLinkedTokenSource(token);
        discovery.CancelAfter(TimeSpan.FromSeconds(15));
        var result = await runner.Run(adb, ["devices", "-l"], showOutput: false, token: discovery.Token);
        var all = result.Output.Split('\n').Select(l => Regex.Match(l, @"^(\S+)\s+(device|offline|unauthorized)(?:\s|$)"))
            .Where(m => m.Success).ToList();
        foreach (var item in all) log.Remember(item.Groups[1].Value);
        var eligible = new List<string>();
        foreach (var match in all.Where(m => m.Groups[2].Value == "device"))
        {
            var id = match.Groups[1].Value;
            using var limit = CancellationTokenSource.CreateLinkedTokenSource(token);
            limit.CancelAfter(TimeSpan.FromSeconds(10));
            CommandResult model;
            try { model = await runner.Run(adb, ["-s", id, "shell", "getprop", "ro.product.model"], showOutput: false, check: false, token: limit.Token); }
            catch (OperationCanceledException) when (!token.IsCancellationRequested) { continue; }
            if (model.ExitCode == 0 && model.Output.Trim().Equals("Quest 3", StringComparison.OrdinalIgnoreCase)) eligible.Add(id);
        }
        if (eligible.Count == 0) { log.Write("No authorized, responsive Quest 3 found. Device deployment skipped."); return null; }
        var selected = eligible.Count == 1 ? 0 : choose is null ? null : await choose(eligible.Count);
        if (selected is null || selected < 0 || selected >= eligible.Count)
            throw new InvalidOperationException("Choose one of the connected Quest 3 devices before installing.");
        log.Write("Using personal Quest 3 device.");
        return new(runner, log, adb, eligible[selected.Value]);
    }

    public async Task Bootstrap(CancellationToken token)
    {
        var result = await Run(["shell", "content", "query", "--uri", SaveUri + "/slot/1", "--projection", "_display_name:_size"], token);
        if (!result.Output.Contains("_display_name=GTA3sf1.b")) throw new IOException("Safe application storage bootstrap failed.");
    }
    public async Task Push(string file, string remote, CancellationToken token, string? expectedHash = null)
    {
        var hash = Digests.Sha256(file);
        if (expectedHash is not null && !hash.Equals(expectedHash, StringComparison.OrdinalIgnoreCase))
            throw new IOException("Local source changed after validation. Revalidate before transferring.");
        await Run(["push", file, remote], token);
        var result = await Shell("sha256sum " + Quote(remote), token);
        if (!string.Equals(result.Output.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries).FirstOrDefault(),
            hash, StringComparison.OrdinalIgnoreCase))
            throw new IOException("Transferred file SHA256 verification failed.");
        log.Write("Verified " + Path.GetFileName(file));
    }
}
