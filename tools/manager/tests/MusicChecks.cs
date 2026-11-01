using System.Security.Cryptography;
using System.Text.Json;
using System.Text.RegularExpressions;
using LibertyManager;

internal static class MusicChecks
{
    private static void Check(bool value) { if (!value) throw new Exception("Music assertion failed"); }
    public static async Task Run(string root, Func<string, Func<Task>, Task> test)
    {
        await test("Music library paging preserves device playlist order", async () =>
        {
            using var log = new PrivateLog(); var runner = new MusicRunner();
            var client = new MusicLibraryClient(new(runner, log, "adb", "synthetic-target"), new() { Workplace = root }, log);
            var library = await client.Read(default);
            Check(library.Tracks.Count == 51 && library.Tracks[50].Title == "Track 50" && runner.Pages == 2);
        });
        await test("Music paging rejects changed revisions and duplicate entries", async () =>
        {
            foreach (var duplicate in new[] { false, true })
            {
                using var log = new PrivateLog(); var runner = new MusicRunner { ChangedRevision = !duplicate, Duplicate = duplicate };
                var client = new MusicLibraryClient(new(runner, log, "adb", "synthetic-target"), new() { Workplace = root }, log);
                try { await client.Read(default); throw new Exception("Invalid page accepted"); } catch (IOException) { }
            }
        });
        await test("Playlist upload uses verified binary sync, expected revision and cleanup", async () =>
        {
            using var log = new PrivateLog(); var runner = new MusicRunner();
            var client = new MusicLibraryClient(new(runner, log, "adb", "synthetic-target"), new() { Workplace = root }, log);
            await client.SaveOrder(new string('a', 64), ["3.mp3", "1.mp3", "2.mp3"], default);
            Check(runner.Saved!.SequenceEqual(new[] { "3.mp3", "1.mp3", "2.mp3" }) && runner.Applied && runner.Remote.Count == 0);
            Check(!Directory.EnumerateDirectories(root, ".music-*").Any());
        });
    }
    private sealed class MusicRunner : ICommandRunner
    {
        public bool ChangedRevision, Duplicate, Applied;
        public int Pages;
        public string[]? Saved;
        public Dictionary<string, byte[]> Remote { get; } = [];
        public Task<CommandResult> Run(string executable, IEnumerable<string> arguments, string? workingDirectory = null, bool showOutput = true, bool check = true, CancellationToken token = default, string? input = null)
        {
            Check(!showOutput); var a = arguments.Skip(2).ToArray(); var output = "";
            if (a.Contains("library"))
            {
                int offset = Pages++ == 0 ? 0 : 50;
                var tracks = Enumerable.Range(offset, offset == 0 ? 50 : 1).Select(i => new { name = (Duplicate && i == 50 ? 0 : i) + ".mp3", title = "Track " + i });
                output = "Bundle[{result=" + JsonSerializer.Serialize(new { ok = true, revision = new string(ChangedRevision && offset > 0 ? 'b' : 'a', 64), total = 51, tracks }) + "}]";
            }
            else if (a[0] == "push") Remote[a[2]] = File.ReadAllBytes(a[1]);
            else if (a.Contains("set-order"))
            {
                Check(a[^1] == QuestDevice.Quote(new string('a',64))); Applied = true; output = "Bundle[{result={\"ok\":true}}]";
            }
            else if (a[0] == "shell" && a.Length == 2)
            {
                var quoted = Regex.Matches(a[1], "'([^']*)'").Select(m => m.Groups[1].Value).ToArray();
                if (a[1].StartsWith("sha256sum")) output = Convert.ToHexString(SHA256.HashData(Remote[quoted[0]]));
                else if (a[1].StartsWith("content write")) Saved = JsonSerializer.Deserialize<string[]>(Remote[quoted[1]]);
                else if (a[1].StartsWith("rm -f")) Remote.Remove(quoted[0]);
            }
            return Task.FromResult(new CommandResult(0, output));
        }
    }
}
