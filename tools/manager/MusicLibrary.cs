using System.Text.Json;
using System.Text.RegularExpressions;

namespace LibertyManager;

public sealed record MusicEntry
{
    public string Name { get; init; } = "";
    public string Filename { get; init; } = "";
    public string Artist { get; init; } = "";
    public string Album { get; init; } = "";
    public string Title { get; init; } = "";
    public string Track { get; init; } = "";
    public string Genre { get; init; } = "";
    public string Year { get; init; } = "";
    public long Bytes { get; init; }
    public long Duration { get; init; }
    public string Format { get; init; } = "";
    public string Mime { get; init; } = "";
    public int SampleRate { get; init; }
    public int Channels { get; init; }
    public int Bitrate { get; init; }
    public string Display => string.Join(" - ", new[] { Clean(Artist), Clean(Album), string.IsNullOrWhiteSpace(Title) ? Clean(string.IsNullOrEmpty(Filename) ? Name : Filename) : Clean(Title) }.Where(s => s.Length > 0));
    public static string Clean(string? text) => new string((text ?? "").Where(c => !char.IsControl(c)).ToArray()).Trim();
    public string Details => $"Filename: {Clean(Filename)}\nStored as: {Clean(Name)}\nArtist: {Clean(Artist)}\nAlbum: {Clean(Album)}\nTitle: {Clean(Title)}\nTrack: {Clean(Track)}\nYear: {Clean(Year)}\nGenre: {Clean(Genre)}\nSize: {Bytes:N0} bytes ({Bytes / 1048576.0:F2} MiB)\nFormat: {Clean(Format)}\nCodec: {Clean(Mime)}\nSample rate: {SampleRate:N0} Hz\nChannels: {Channels}\nBitrate: {Bitrate / 1000.0:F0} kb/s\nDuration: {TimeSpan.FromMilliseconds(Math.Max(0, Duration)):g}";
}

public sealed record MusicLibrarySnapshot(string Revision, IReadOnlyList<MusicEntry> Tracks);

public sealed class OrderedMusic
{
    public string Revision { get; private set; } = "";
    public List<MusicEntry> Tracks { get; } = [];
    public bool Dirty { get; private set; }
    public void Load(MusicLibrarySnapshot snapshot) { Revision = snapshot.Revision; Tracks.Clear(); Tracks.AddRange(snapshot.Tracks); Dirty = false; }
    public bool Move(int from, int to)
    {
        if (from < 0 || to < 0 || from >= Tracks.Count || to >= Tracks.Count || from == to) return false;
        var track = Tracks[from]; Tracks.RemoveAt(from); Tracks.Insert(to, track); Dirty = true; return true;
    }
}

public sealed class MusicLibraryClient(QuestDevice device, ManagerSettings settings, PrivateLog log)
{
    public async Task<MusicLibrarySnapshot> Read(CancellationToken token)
    {
        var tracks = new List<MusicEntry>(); string revision = ""; var total = 0;
        do
        {
            var page = await MusicImporter.Provider(device, "library", tracks.Count + (revision.Length == 0 ? "" : ":" + revision), token);
            var current = page.GetProperty("revision").GetString() ?? "";
            if (!Regex.IsMatch(current, "\\A[a-f0-9]{64}\\z") || (revision.Length > 0 && current != revision)) throw new IOException("Library changed while reading. Refresh again.");
            revision = current; total = page.GetProperty("total").GetInt32();
            if (total is < 0 or > 1000) throw new IOException("Headset library exceeds the supported limit.");
            var entries = page.GetProperty("tracks").Deserialize<MusicEntry[]>(new JsonSerializerOptions { PropertyNameCaseInsensitive = true }) ?? [];
            if (entries.Length == 0 && tracks.Count < total) throw new IOException("Incomplete library response. Refresh again.");
            tracks.AddRange(entries);
            if (tracks.Count > total) throw new IOException("Inconsistent library response.");
        } while (tracks.Count < total);
        if (tracks.Select(t => t.Name).Distinct(StringComparer.Ordinal).Count() != tracks.Count || tracks.Any(t => t.Name.Length is 0 or > 200 || t.Name.Contains('/') || t.Name.Contains('\\') || t.Name.Any(char.IsControl)))
            throw new IOException("Invalid headset library filenames.");
        log.Write($"Loaded {tracks.Count} headset music files."); return new(revision, tracks);
    }
    public async Task SaveOrder(string revision, string[] names, CancellationToken token)
    {
        if (!Regex.IsMatch(revision, "\\A[a-f0-9]{64}\\z") || names.Length > 1000 || names.Distinct(StringComparer.Ordinal).Count() != names.Length)
            throw new ArgumentException("Refresh the headset library before saving order.");
        var data = JsonSerializer.SerializeToUtf8Bytes(names);
        if (data.Length > 262144) throw new IOException("Playlist is too large.");
        var root = Path.Combine(settings.Workplace, ".music-" + Guid.NewGuid().ToString("N")); Paths.NoLinks(root); Directory.CreateDirectory(root);
        var local = Path.Combine(root, "playlist.json"); var remote = "/data/local/tmp/lcvr-playlist-" + Guid.NewGuid().ToString("N") + ".json";
        try
        {
            await File.WriteAllBytesAsync(local, data, token); await device.Push(local, remote, token);
            await device.Shell("content write --uri 'content://com.libertyvr.quest.music/playlist' < " + QuestDevice.Quote(remote), token);
            await MusicImporter.Provider(device, "set-order", revision, token);
            log.Write("Playlist order saved. The custom station refreshes automatically.");
        }
        finally
        {
            using var cleanup = new CancellationTokenSource(TimeSpan.FromSeconds(10));
            try { await device.Shell("rm -f " + QuestDevice.Quote(remote), cleanup.Token, check: false); }
            catch (Exception e) when (e is IOException or OperationCanceledException) { log.Write("Temporary playlist cleanup failed."); }
            Directory.Delete(root, true);
        }
    }
}
