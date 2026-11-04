using System.Globalization;
using System.Text.Json;

namespace LibertyManager;

// Retail audio and all conversion products remain in the local workplace/headset.
public sealed class RadioStations(ManagerSettings settings, ICommandRunner runner, PrivateLog log)
{
    private static readonly HashSet<string> Stations = new(StringComparer.OrdinalIgnoreCase)
        { "HEAD", "CLASS", "KJAH", "RISE", "LIPS", "GAME", "MSX", "FLASH", "CHAT" };

    public static bool IsStation(string name, string extension) =>
        Path.GetExtension(name).Equals(extension, StringComparison.OrdinalIgnoreCase) && Stations.Contains(Path.GetFileNameWithoutExtension(name));

    public static string CompressedPath(string path) => path[..^4] + ".m4a";

    public async Task ConvertAndInstall(QuestDevice device, string? localAudio, CancellationToken token)
    {
        // Fail before touching headset audio if either local dependency is missing.
        await runner.Run(settings.Ffmpeg, ["-version"], showOutput: false, token: token);
        await runner.Run(settings.Ffprobe, ["-version"], showOutput: false, token: token);
        var scratch = Path.Combine(settings.Workplace, "radio-conversion-" + Guid.NewGuid().ToString("N"));
        Paths.NoLinks(scratch);
        Directory.CreateDirectory(scratch);
        try
        {
            var sources = new List<(string Local, string Remote)>();
            if (localAudio is not null)
            {
                Paths.NoLinks(localAudio);
                foreach (var file in Directory.GetFiles(localAudio).Where(f => IsStation(f, ".wav")))
                {
                    Paths.NoLinks(file);
                    sources.Add((file, QuestDevice.GameData + "/" + Path.GetFileName(localAudio) + "/" + Path.GetFileName(file)));
                }
            }
            else
            {
                var listing = await device.Shell("find " + QuestDevice.Quote(QuestDevice.GameData) + " -maxdepth 2 -type f", token);
                foreach (var remote in listing.Output.Split('\n').Select(s => s.TrimEnd('\r')))
                {
                    if (!remote.StartsWith(QuestDevice.GameData + "/", StringComparison.Ordinal)) continue;
                    var parts = remote[(QuestDevice.GameData.Length + 1)..].Split('/');
                    if (parts.Length != 2 || !parts[0].Equals("Audio", StringComparison.OrdinalIgnoreCase) || !IsStation(parts[1], ".wav")) continue;
                    var local = Path.Combine(scratch, sources.Count + ".wav");
                    await device.Run(["pull", remote, local], token);
                    sources.Add((local, remote));
                }
            }
            if (sources.Count == 0)
            {
                log.Write("No native radio station WAVs found. Existing compressed stations were left unchanged.");
                return;
            }
            // Finish and validate every conversion before replacing any headset station.
            var converted = new List<(string Local, string Remote, string Wave)>();
            foreach (var (local, remote) in sources)
            {
                var output = Path.Combine(scratch, converted.Count + ".m4a");
                log.Write("Converting radio station " + Path.GetFileNameWithoutExtension(remote));
                var original = await runner.Run(settings.Ffprobe, ["-v", "error", "-show_format", "-show_streams", "-of", "json", local], showOutput: false, token: token);
                using var metadata = JsonDocument.Parse(original.StandardOutput);
                var originalSeconds = double.Parse(metadata.RootElement.GetProperty("format").GetProperty("duration").GetString()!, CultureInfo.InvariantCulture);
                if (!double.IsFinite(originalSeconds) || originalSeconds <= 0) throw new IOException("Invalid radio source duration.");
                await runner.Run(settings.Ffmpeg, ["-nostdin", "-v", "error", "-xerror", "-i", local, "-map", "0:a:0", "-vn",
                    "-map_metadata", "-1", "-c:a", "aac", "-profile:a", "aac_low", "-b:a", "128k", "-ar", "48000", "-ac", "2",
                    "-movflags", "+faststart", "-n", output], showOutput: false, token: token);
                var probe = await runner.Run(settings.Ffprobe, ["-v", "error", "-show_format", "-show_streams", "-of", "json", output], showOutput: false, token: token);
                var duration = MusicImporter.ValidateProbe(".m4a", new FileInfo(output).Length, probe.StandardOutput);
                if (Math.Abs(duration - originalSeconds * 1000) > 250) throw new IOException("Converted radio duration differs from source; WAV preserved.");
                // Decode the complete result to detect corrupt/truncated packets before deleting WAVs.
                await runner.Run(settings.Ffmpeg, ["-nostdin", "-v", "error", "-xerror", "-i", output, "-f", "null", "-"], showOutput: false, token: token);
                converted.Add((output, CompressedPath(remote), remote));
            }
            foreach (var (local, remote, wave) in converted)
            {
                var incoming = remote + ".incoming-" + Guid.NewGuid().ToString("N");
                try
                {
                    await device.Shell("mkdir -p " + QuestDevice.Quote(remote[..remote.LastIndexOf('/')]), token);
                    await device.Push(local, incoming, token);
                    // Rename verified file first. A failed upload never removes the original WAV.
                    await device.Shell("mv -f " + QuestDevice.Quote(incoming) + " " + QuestDevice.Quote(remote) +
                        " && rm -f " + QuestDevice.Quote(wave), token);
                }
                finally
                {
                    using var cleanup = new CancellationTokenSource(TimeSpan.FromSeconds(10));
                    try { await device.Shell("rm -f " + QuestDevice.Quote(incoming), cleanup.Token, check: false); }
                    catch (Exception e) when (e is IOException or OperationCanceledException) { log.Write("Temporary radio upload cleanup failed."); }
                }
            }
            log.Write($"Converted and verified {converted.Count} radio stations. Headset WAVs removed; PC originals preserved.");
        }
        finally { Directory.Delete(scratch, recursive: true); }
    }
}
