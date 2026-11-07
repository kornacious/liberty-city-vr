using System.Buffers.Binary;
using System.Text.Json;

namespace LibertyManager;

// All sources and decoded products remain local. FLAC preserves the decoded
// retail signal and original sample rate, without another lossy encoding pass.
public sealed class OriginalXboxAudio(ManagerSettings settings, ICommandRunner runner, PrivateLog log)
{
    public const string Identity = "ORIGINAL_XBOX_AUDIO_1";
    public static string ConvertedPath(string source) => Path.ChangeExtension(source, ".flac").Replace('\\', '/');
    public static IEnumerable<string> RequiredFiles => OriginalXboxModels.Disc.Files.Keys
        .Where(p => p.StartsWith("audio/", StringComparison.Ordinal))
        .Select(p => p.EndsWith(".raw", StringComparison.Ordinal) || p.EndsWith(".sdt", StringComparison.Ordinal) ? p : ConvertedPath(p))
        .Append("audio/READY.txt");

    // FFprobe's WAV duration estimate treats Xbox ADPCM bytes as frames on
    // stereo files. Count complete 64-frame blocks instead of trusting it.
    public static double WaveDuration(string path, int rate, int channels)
    {
        using var reader = new BinaryReader(File.OpenRead(path));
        if (!reader.ReadBytes(4).AsSpan().SequenceEqual("RIFF"u8)) throw new InvalidDataException("Invalid Xbox WAV.");
        reader.ReadUInt32();
        if (!reader.ReadBytes(4).AsSpan().SequenceEqual("WAVE"u8)) throw new InvalidDataException("Invalid Xbox WAV.");
        var block = 36 * channels; var format = false;
        while (reader.BaseStream.Position + 8 <= reader.BaseStream.Length)
        {
            var kind = reader.ReadUInt32(); var size = reader.ReadUInt32(); var begin = reader.BaseStream.Position;
            if (size > reader.BaseStream.Length - begin) throw new InvalidDataException("Truncated Xbox WAV.");
            if (kind == 0x20746d66)
            {
                if (size < 20 || reader.ReadUInt16() != 0x69 || reader.ReadUInt16() != channels || reader.ReadUInt32() != rate)
                    throw new InvalidDataException("Unexpected Xbox WAV format.");
                reader.ReadUInt32();
                if (reader.ReadUInt16() != block || reader.ReadUInt16() != 4 || reader.ReadUInt16() != 2 || reader.ReadUInt16() != 64)
                    throw new InvalidDataException("Unexpected Xbox ADPCM block dimensions.");
                format = true;
            }
            if (kind == 0x61746164)
            {
                if (!format || size == 0 || size % block != 0) throw new InvalidDataException("Invalid Xbox WAV frame count.");
                return (double)(size / block) * 64 / rate;
            }
            reader.BaseStream.Position = begin + size + (size % 2);
        }
        throw new InvalidDataException("Xbox WAV has no audio data.");
    }

    // Xbox bank offsets and loop points address 36-byte ADPCM blocks. Each
    // decodes to 64 mono PCM16 frames. Never reinterpret compressed byte offsets
    // as PCM offsets, or change the authored per-effect playback frequencies.
    public static byte[] ConvertTable(byte[] table, long rawBytes)
    {
        if (table.Length != 3032 * 20 || rawBytes <= 0 || rawBytes % 36 != 0)
            throw new InvalidDataException("Invalid Xbox sound bank dimensions.");
        var result = (byte[])table.Clone();
        long expected = 0;
        for (var i = 0; i < table.Length; i += 20)
        {
            var offset = BinaryPrimitives.ReadUInt32LittleEndian(table.AsSpan(i));
            var size = BinaryPrimitives.ReadUInt32LittleEndian(table.AsSpan(i + 4));
            var frequency = BinaryPrimitives.ReadUInt32LittleEndian(table.AsSpan(i + 8));
            var start = BinaryPrimitives.ReadUInt32LittleEndian(table.AsSpan(i + 12));
            var end = BinaryPrimitives.ReadInt32LittleEndian(table.AsSpan(i + 16));
            if (offset != expected || size == 0 || size % 36 != 0 || (long)offset + size > rawBytes ||
                frequency is < 1000 or > 192000 || start % 36 != 0 || start > size ||
                (end != -1 && (end < start || end > size || end % 36 != 0)))
                throw new InvalidDataException("Invalid Xbox sound bank entry.");
            static uint Pcm(uint value) => checked(value / 36 * 128);
            BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(i), Pcm(offset));
            BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(i + 4), Pcm(size));
            BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(i + 12), Pcm(start));
            BinaryPrimitives.WriteInt32LittleEndian(result.AsSpan(i + 16), end == -1 ? -1 : checked((int)Pcm((uint)end)));
            expected += size;
        }
        if (expected != rawBytes) throw new InvalidDataException("Unexpected trailing sound bank data.");
        return result;
    }

    public async Task Prepare(string disc, string stage, string scratch, CancellationToken token)
    {
        await runner.Run(settings.Ffmpeg, ["-version"], showOutput: false, token: token);
        await runner.Run(settings.Ffprobe, ["-version"], showOutput: false, token: token);
        var audio = Path.Combine(stage, "audio"); Directory.CreateDirectory(audio);
        var raw = Path.Combine(disc, "audio/sfx.raw");
        var rawBytes = new FileInfo(raw).Length;
        var table = ConvertTable(File.ReadAllBytes(Path.Combine(disc, "audio/sfx.sdt")), rawBytes);
        var wrapped = Path.Combine(scratch, "sfx.wav");
        using (var writer = new BinaryWriter(File.Create(wrapped)))
        {
            // A neutral container rate only instructs the decoder; output is raw
            // PCM and the converted SDT retains every original playback rate.
            writer.Write("RIFF"u8); writer.Write(checked((uint)(rawBytes + 40)));
            writer.Write("WAVEfmt "u8); writer.Write(20u);
            writer.Write((ushort)0x69); writer.Write((ushort)1); writer.Write(44100u);
            writer.Write(44100u * 36 / 64); writer.Write((ushort)36); writer.Write((ushort)4);
            writer.Write((ushort)2); writer.Write((ushort)64);
            writer.Write("data"u8); writer.Write(checked((uint)rawBytes));
            using var input = File.OpenRead(raw); input.CopyTo(writer.BaseStream);
        }
        log.Write("Decoding Xbox sound effects to PCM with matching sample indices and loop points.");
        var pcm = Path.Combine(audio, "sfx.raw");
        await runner.Run(settings.Ffmpeg, ["-nostdin", "-v", "error", "-xerror", "-i", wrapped,
            "-map", "0:a:0", "-c:a", "pcm_s16le", "-f", "s16le", "-n", pcm], showOutput: false, token: token);
        if (new FileInfo(pcm).Length != rawBytes / 36 * 128)
            throw new InvalidDataException("Xbox sound bank decoder produced an unexpected frame count.");
        File.WriteAllBytes(Path.Combine(audio, "sfx.sdt"), table);
        foreach (var source in OriginalXboxModels.Disc.Files.Keys.Where(p => p.StartsWith("audio/", StringComparison.Ordinal) &&
            (p.EndsWith(".wav", StringComparison.Ordinal) || p.EndsWith(".wma", StringComparison.Ordinal))).Order(StringComparer.Ordinal))
        {
            token.ThrowIfCancellationRequested();
            var input = Paths.Child(disc, source); var output = Paths.Child(stage, ConvertedPath(source));
            log.Write("Preparing Xbox audio: " + Path.GetFileName(source));
            var before = await Probe(input, token);
            if (source.EndsWith(".wav", StringComparison.Ordinal)) before.Seconds = WaveDuration(input, before.Rate, before.Channels);
            await runner.Run(settings.Ffmpeg, ["-nostdin", "-v", "error", "-xerror", "-i", input,
                "-map", "0:a:0", "-vn", "-map_metadata", "-1", "-c:a", "flac", "-sample_fmt", "s16", "-n", output], showOutput: false, token: token);
            var after = await Probe(output, token);
            if (before.Rate != after.Rate || before.Channels != after.Channels ||
                (source.EndsWith(".wav", StringComparison.Ordinal) && Math.Abs(before.Seconds - after.Seconds) > 1.0 / before.Rate))
                throw new InvalidDataException("Converted Xbox audio metadata mismatch: " + source);
            // ASF/WMA header duration can include encoder padding. Validate the
            // entire decoded PCM signal instead: catches truncation and proves
            // FLAC adds no signal changes, including across padding boundaries.
            if (await PcmHash(input, token) != await PcmHash(output, token))
                throw new InvalidDataException("Converted Xbox audio differs from the decoded source: " + source);
        }
        File.WriteAllLines(Path.Combine(audio, "READY.txt"), new[] { Identity }.Concat(RequiredFiles.Where(p => p != "audio/READY.txt").Order(StringComparer.Ordinal)));
    }

    private async Task<string> PcmHash(string path, CancellationToken token)
    {
        var result = await runner.Run(settings.Ffmpeg, ["-nostdin", "-v", "error", "-xerror", "-i", path,
            "-map", "0:a:0", "-c:a", "pcm_s16le", "-f", "hash", "-hash", "sha256", "-"], showOutput: false, token: token);
        var hash = result.StandardOutput.Trim();
        if (!System.Text.RegularExpressions.Regex.IsMatch(hash, @"\ASHA256=[A-Fa-f0-9]{64}\z"))
            throw new InvalidDataException("Unable to validate the decoded Xbox signal.");
        return hash;
    }

    private async Task<(int Rate, int Channels, double Seconds)> Probe(string path, CancellationToken token)
    {
        var result = await runner.Run(settings.Ffprobe, ["-v", "error", "-show_streams", "-show_format", "-of", "json", path], showOutput: false, token: token);
        using var json = JsonDocument.Parse(result.StandardOutput);
        var streams = json.RootElement.GetProperty("streams");
        if (streams.GetArrayLength() != 1) throw new InvalidDataException("Expected one Xbox audio stream.");
        var stream = streams[0];
        var rate = int.Parse(stream.GetProperty("sample_rate").GetString()!, System.Globalization.CultureInfo.InvariantCulture);
        var channels = stream.GetProperty("channels").GetInt32();
        var seconds = double.Parse(json.RootElement.GetProperty("format").GetProperty("duration").GetString()!, System.Globalization.CultureInfo.InvariantCulture);
        if (rate is < 1000 or > 192000 || channels is < 1 or > 2 || !double.IsFinite(seconds) || seconds <= 0)
            throw new InvalidDataException("Invalid Xbox audio metadata.");
        return (rate, channels, seconds);
    }
}
