using System.Diagnostics;
using System.Text;
using System.Text.RegularExpressions;

namespace LibertyManager;

/// <summary>Fail-closed staged-tree, message and reachable-history checks. Never print matches or Git diagnostics.</summary>
public sealed class PrivacyGuard(string repository)
{
    private readonly List<byte[]> privateValues = [];
    public static readonly Regex DevicePattern = new(@"(?<![A-Za-z0-9])2G[A-Z0-9]{12}(?![A-Za-z0-9])", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);
    private static readonly Regex HostPattern = new(@"\b[A-Za-z0-9.-]+\.ts\.net\b", RegexOptions.IgnoreCase | RegexOptions.CultureInvariant);
    private static bool IsOid(string value) => Regex.IsMatch(value, @"\A(?:[0-9a-f]{40}|[0-9a-f]{64})\z");

    private Process StartGit(params string[] args)
    {
        var start = new ProcessStartInfo(Paths.FindExecutable("git") ?? "git")
        { WorkingDirectory = repository, UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true, RedirectStandardInput = true, CreateNoWindow = true };
        foreach (var arg in args) start.ArgumentList.Add(arg);
        return Process.Start(start) ?? throw new IOException("Privacy check failed.");
    }
    private async Task<byte[]> Git(string[] args, string? input = null)
    {
        using var process = StartGit(args);
        var error = process.StandardError.ReadToEndAsync();
        if (input is not null) await process.StandardInput.WriteAsync(input);
        process.StandardInput.Close();
        using var bytes = new MemoryStream();
        await process.StandardOutput.BaseStream.CopyToAsync(bytes);
        await process.WaitForExitAsync(); await error;
        if (process.ExitCode != 0) throw new IOException("Privacy Git operation failed.");
        return bytes.ToArray();
    }
    private async Task<string> GitText(params string[] args) => Encoding.UTF8.GetString(await Git(args)).Trim();

    public static bool ContainsPrivate(ReadOnlySpan<byte> content, ReadOnlySpan<byte> value)
    {
        if (value.IsEmpty) return false;
        static byte Lower(byte b) => b is >= 65 and <= 90 ? (byte)(b + 32) : b;
        for (var i = 0; i <= content.Length - value.Length; i++)
        {
            var j = 0;
            while (j < value.Length && Lower(content[i + j]) == Lower(value[j])) j++;
            if (j == value.Length) return true;
        }
        return false;
    }
    public void Check(byte[] bytes)
    {
        var text = Encoding.Latin1.GetString(bytes);
        if (DevicePattern.IsMatch(text) || HostPattern.IsMatch(text) || privateValues.Any(value => ContainsPrivate(bytes, value)))
            throw new InvalidDataException("Sensitive content blocked.");
    }

    private async Task ScanObjects(IEnumerable<string> ids)
    {
        using var process = StartGit("cat-file", "--batch");
        var error = process.StandardError.ReadToEndAsync();
        var stream = process.StandardOutput.BaseStream;
        try
        {
            foreach (var oid in ids.Distinct(StringComparer.Ordinal))
            {
                if (!IsOid(oid)) throw new InvalidDataException("Invalid object reference.");
                await process.StandardInput.WriteLineAsync(oid); await process.StandardInput.FlushAsync();
                var headerBytes = new List<byte>();
                int b;
                while ((b = stream.ReadByte()) != '\n' && b >= 0) { headerBytes.Add((byte)b); if (headerBytes.Count > 256) throw new InvalidDataException("Invalid object header."); }
                var header = Encoding.ASCII.GetString(headerBytes.ToArray()).Split(' ');
                if (header.Length != 3 || header[1] is not ("blob" or "commit" or "tag" or "tree") || !int.TryParse(header[2], out var length) || length < 0)
                    throw new InvalidDataException("Invalid object header.");
                var data = new byte[length]; await stream.ReadExactlyAsync(data);
                if (stream.ReadByte() != '\n') throw new InvalidDataException("Incomplete Git object.");
                Check(data);
            }
        }
        finally
        {
            process.StandardInput.Close();
            if (!process.HasExited) process.Kill(true);
            await process.WaitForExitAsync(); await error;
        }
    }

    private async Task History(string[]? tips)
    {
        if (tips is { Length: 0 }) return;
        var listing = Encoding.UTF8.GetString(await Git(tips is null ? ["rev-list", "--objects", "--all"] : ["rev-list", "--objects", "--stdin"], tips is null ? null : string.Join('\n', tips) + "\n"));
        var ids = new List<string>();
        foreach (var line in listing.Split('\n', StringSplitOptions.RemoveEmptyEntries))
        {
            var fields = line.Split(' ', 2);
            if (fields.Length == 2) Check(Encoding.UTF8.GetBytes(fields[1]));
            ids.Add(fields[0]);
        }
        await ScanObjects(ids);
    }

    public async Task Run(string mode, string? messagePath = null, string? pushInput = null)
    {
        if (mode == "install-hooks")
        {
            foreach (var hook in new[] { "pre-commit", "commit-msg", "pre-push" })
                if (!File.Exists(Path.Combine(repository, ".githooks", hook))) throw new IOException("Tracked privacy hooks are missing.");
            using (var existing = StartGit("config", "--get", "core.hooksPath"))
            {
                var result = await existing.StandardOutput.ReadToEndAsync(); await existing.WaitForExitAsync();
                if (existing.ExitCode is not (0 or 1) || result.Trim() is not ("" or ".githooks")) throw new IOException("Custom hooks require review.");
            }
            await Git(["config", "--local", "privacy.dotnet", Paths.FindExecutable("dotnet") ?? throw new IOException(".NET host is required.")]);
            await Git(["config", "--local", "core.hooksPath", ".githooks"]);
            return;
        }
        var denylist = await GitText("rev-parse", "--git-path", "info/privacy-denylist");
        if (!Path.IsPathRooted(denylist)) denylist = Path.Combine(repository, denylist);
        if (File.Exists(denylist)) privateValues.AddRange(File.ReadAllLines(denylist).Where(l => l.Length > 0).Select(Encoding.UTF8.GetBytes));
        switch (mode)
        {
            case "staged":
                var ids = new List<string>();
                var records = Encoding.UTF8.GetString(await Git(["ls-files", "--stage", "-z"]));
                foreach (var record in records.Split('\0', StringSplitOptions.RemoveEmptyEntries))
                {
                    var fields = record.Split('\t', 2); var metadata = fields[0].Split(' ');
                    if (fields.Length != 2 || metadata.Length != 3 || metadata[2] != "0") throw new InvalidDataException("Unmerged index.");
                    Check(Encoding.UTF8.GetBytes(fields[1]));
                    if (metadata[0] != "160000") ids.Add(metadata[1]);
                }
                await ScanObjects(ids); break;
            case "message": Check(File.ReadAllBytes(messagePath ?? throw new ArgumentException("A message file is required."))); break;
            case "history": await History(null); break;
            case "pre-push":
                var tips = new List<string>();
                foreach (var line in (pushInput ?? await Console.In.ReadToEndAsync()).Split('\n', StringSplitOptions.RemoveEmptyEntries))
                {
                    var parts = line.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
                    if (parts.Length != 4 || !IsOid(parts[1]) || !IsOid(parts[3])) throw new InvalidDataException("Invalid push input.");
                    if (parts[1].All(c => c == '0')) continue;
                    if (!parts[2].StartsWith("refs/heads/", StringComparison.Ordinal) && !parts[2].StartsWith("refs/tags/", StringComparison.Ordinal)) throw new InvalidDataException("Private refs cannot be published.");
                    Check(Encoding.UTF8.GetBytes(parts[0] + " " + parts[2])); tips.Add(parts[1]);
                }
                await History(tips.ToArray()); break;
            default: throw new ArgumentException("Unknown privacy mode.");
        }
    }
}
