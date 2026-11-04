using System.Globalization;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace LibertyManager;

public sealed record MusicTrack(string Path, long Size, long Duration, string Sha256, string Name);

public sealed class MusicImporter(ManagerSettings settings, ICommandRunner runner, PrivateLog log,
    Func<CancellationToken, Task<QuestDevice?>> connect)
{
    public const long MaximumDuration = 24L * 24 * 60 * 60 * 1000;
    private static readonly Dictionary<string, string[]> Codecs = new(StringComparer.OrdinalIgnoreCase)
    {
        [".mp3"] = ["mp3"], [".m4a"] = ["aac"], [".ogg"] = ["vorbis", "opus"],
        [".opus"] = ["opus"], [".flac"] = ["flac"], [".wav"] = ["pcm_s16le"]
    };

    public static string SafeName(string path, string sha)
    {
        var ascii = new string(Path.GetFileNameWithoutExtension(path).Normalize(NormalizationForm.FormKD).Where(c => c < 128).ToArray());
        var stem = Regex.Replace(ascii, "[^A-Za-z0-9_.-]+", "-").Trim('.', '_', '-');
        if (stem.Length > 70) stem = stem[..70];
        if (stem.Length == 0) stem = "track";
        return stem + "-" + sha[..16].ToLowerInvariant() + Path.GetExtension(path).ToLowerInvariant();
    }

    public static long ValidateProbe(string extension, long size, string json)
    {
        if (size is <= 0 or > 512L * 1024 * 1024) throw new InvalidDataException("Files must be between 1 byte and 512 MiB.");
        using var doc = JsonDocument.Parse(json);
        var streams = doc.RootElement.GetProperty("streams").EnumerateArray().ToArray();
        var audio = streams.Where(s => s.GetProperty("codec_type").GetString() == "audio").ToArray();
        var movingVideo = streams.Any(s => s.GetProperty("codec_type").GetString() == "video" &&
            (!s.TryGetProperty("disposition", out var d) || !d.TryGetProperty("attached_pic", out var p) || p.GetInt32() == 0));
        if (audio.Length != 1 || movingVideo) throw new InvalidDataException("Expected one audio stream and no moving video.");
        var a = audio[0];
        if (!Codecs.TryGetValue(extension, out var supported) || !supported.Contains(a.GetProperty("codec_name").GetString()))
            throw new InvalidDataException("Unsupported codec or codec/extension combination.");
        if (a.GetProperty("channels").GetInt32() is not (1 or 2) ||
            !int.TryParse(a.GetProperty("sample_rate").ToString(), out var sampleRate) || sampleRate is not (44100 or 48000))
            throw new InvalidDataException("Use mono/stereo at 44.1 or 48 kHz.");
        var durationText = doc.RootElement.TryGetProperty("format", out var f) && f.TryGetProperty("duration", out var fd)
            ? fd.ToString() : a.TryGetProperty("duration", out var ad) ? ad.ToString() : "0";
        if (!double.TryParse(durationText, NumberStyles.Float, CultureInfo.InvariantCulture, out var seconds) ||
            !double.IsFinite(seconds) || seconds <= 0 || seconds * 1000 > MaximumDuration)
            throw new InvalidDataException("Invalid or excessive duration.");
        var duration = (long)(seconds * 1000);
        if (duration == 0) throw new InvalidDataException("Audio duration is too short.");
        return duration;
    }

    public static string[] SourcePaths(ActionOptions options)
    {
        var sources = options.MusicSources.Length > 0 ? options.MusicSources : string.IsNullOrWhiteSpace(options.MusicSource) ? [] : new[] { options.MusicSource };
        return sources.SelectMany(source => Directory.Exists(source)
                ? Directory.GetFiles(source).Where(p => Codecs.ContainsKey(Path.GetExtension(p))).Order(StringComparer.Ordinal)
                : new[] { source }.AsEnumerable())
            .Select(Path.GetFullPath).Distinct(OperatingSystem.IsWindows() ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal).ToArray();
    }
    public async Task Import(ActionOptions options, CancellationToken token)
    {
        var paths = SourcePaths(options);
        if (paths.Length == 0 && string.IsNullOrWhiteSpace(options.BoardExport)) throw new IOException("Choose a music file, a folder containing audio, or a board export destination.");
        var accepted = new List<MusicTrack>();
        var seen = new HashSet<string>();
        foreach (var path in paths)
        {
            token.ThrowIfCancellationRequested();
            Paths.NoLinks(path);
            var file = new FileInfo(path);
            if (!file.Exists) throw new IOException("Music file does not exist.");
            var probe = await runner.Run(settings.Ffprobe, ["-v", "error", "-show_format", "-show_streams", "-of", "json", file.FullName], showOutput: false, token: token);
            var duration = ValidateProbe(file.Extension, file.Length, probe.StandardOutput);
            var hash = Digests.Sha256(file.FullName);
            if (!seen.Add(hash)) { log.Write("Skipping duplicate: " + file.Name); continue; }
            accepted.Add(new(file.FullName, file.Length, duration, hash, SafeName(file.Name, hash)));
            log.Write("Validated " + file.Name);
        }
        if (accepted.Count > 1000 || accepted.Sum(t => t.Duration) > MaximumDuration) throw new IOException("Selection exceeds 1000 tracks or 24 days total duration.");
        if (options.ValidateMusicOnly) { log.Write($"Validation complete: {accepted.Count} tracks. No headset accessed."); return; }
        var device = await connect(token) ?? throw new IOException("Music import requires an authorized Quest 3.");
        if (!string.IsNullOrWhiteSpace(options.BoardExport))
        {
            if (File.Exists(options.BoardExport)) throw new IOException("Board export destination already exists. Choose a new filename.");
            var result = await device.Run(["exec-out", "content", "read", "--uri", "content://com.libertyvr.quest.music/board-export"], token);
            if (!result.Output.Contains("[RadioBoard]") || !result.Output.Contains("[Vehicle.") || Encoding.UTF8.GetByteCount(result.Output) > 65536)
                throw new IOException("Export a saved vehicle placement from the in-game Radio settings first.");
            using var stream = new FileStream(options.BoardExport, FileMode.CreateNew);
            await stream.WriteAsync(Encoding.UTF8.GetBytes(result.Output), token);
            log.Write("Radio board placement exported.");
        }
        if (accepted.Count == 0) return;
        var info = await Provider(device, "info", null, token);
        var names = info.GetProperty("names").EnumerateArray().Select(n => n.GetString()).ToHashSet();
        var fresh = new List<MusicTrack>();
        foreach (var track in accepted)
        {
            if (names.Contains(track.Name) && (await Provider(device, "check", track.Name + ":" + track.Sha256.ToLowerInvariant(), token)).GetProperty("exists").GetBoolean())
            {
                if (info.TryGetProperty("version", out var version) && version.GetInt32() >= 2)
                    await Provider(device, "original-name", JsonSerializer.Serialize(new { name = track.Name, filename = Path.GetFileName(track.Path) }), token);
                log.Write("Already present: " + Path.GetFileName(track.Path));
            }
            else fresh.Add(track);
        }
        if (info.GetProperty("count").GetInt32() + fresh.Count > 1000 ||
            (info.TryGetProperty("duration", out var d) ? d.GetInt64() : 0) + fresh.Sum(t => t.Duration) > MaximumDuration)
            throw new IOException("Existing library plus selection exceeds 1000 tracks or 24 days total duration.");
        if (info.GetProperty("free").GetInt64() - fresh.Sum(t => t.Size) - fresh.Select(t => t.Size).DefaultIfEmpty(0).Max() < 1024L * 1024 * 1024)
            throw new IOException("Import would leave less than 1 GiB free.");
        foreach (var track in fresh)
        {
            var remote = "/data/local/tmp/lcvr-music-" + Guid.NewGuid().ToString("N") + ".tmp";
            try
            {
                await device.Push(track.Path, remote, token, track.Sha256);
                await device.Shell("content write --uri " + QuestDevice.Quote("content://com.libertyvr.quest.music/upload/" + track.Name) + " < " + QuestDevice.Quote(remote), token);
                var committed = await Provider(device, "commit", track.Name + ":" + track.Sha256.ToLowerInvariant(), token);
                if (committed.GetProperty("bytes").GetInt64() != track.Size) throw new IOException("Verified music size differs from local source.");
                if (info.TryGetProperty("version", out var version) && version.GetInt32() >= 2)
                    await Provider(device, "original-name", JsonSerializer.Serialize(new { name = track.Name, filename = Path.GetFileName(track.Path) }), token);
                log.Write("Imported " + Path.GetFileName(track.Path));
            }
            catch
            {
                using var cleanup = new CancellationTokenSource(TimeSpan.FromSeconds(10));
                try { await Provider(device, "abort", track.Name, cleanup.Token); } catch (Exception e) when (e is IOException or OperationCanceledException) { log.Write("Could not abort staged music upload."); }
                throw;
            }
            finally
            {
                using var cleanup = new CancellationTokenSource(TimeSpan.FromSeconds(10));
                try { await device.Shell("rm -f " + QuestDevice.Quote(remote), cleanup.Token); } catch (Exception e) when (e is IOException or OperationCanceledException) { log.Write("Could not remove temporary music upload."); }
            }
        }
        log.Write($"Music import complete: {fresh.Count} imported. The game rescans automatically.");
    }

    public static async Task<JsonElement> Provider(QuestDevice device, string method, string? arg, CancellationToken token)
    {
        List<string> args = ["shell", "content", "call", "--uri", "content://com.libertyvr.quest.music", "--method", method];
        if (arg is not null) args.AddRange(["--arg", QuestDevice.Quote(arg)]);
        var result = await device.Run(args, token);
        return ParseProvider(result.Output);
    }
    public static JsonElement ParseProvider(string output)
    {
        var start = output.IndexOf("{\"", StringComparison.Ordinal);
        if (start < 0) throw new IOException("Music provider unavailable. Install an updated APK.");
        var reader = new Utf8JsonReader(Encoding.UTF8.GetBytes(output[start..]));
        using var doc = JsonDocument.ParseValue(ref reader);
        if (!doc.RootElement.TryGetProperty("ok", out var ok) || !ok.GetBoolean())
            throw new IOException("Music provider rejected the operation. No existing music was overwritten.");
        return doc.RootElement.Clone();
    }
}
