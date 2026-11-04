using LibertyManager;

internal static class RadioChecks
{
    private static void Check(bool condition) { if (!condition) throw new Exception("Radio assertion failed."); }
    public static async Task Run(string root, Func<string, Func<Task>, Task> test)
    {
        var audio = Path.Combine(root, "radio-input", "Audio"); Directory.CreateDirectory(audio);
        File.WriteAllText(Path.Combine(audio, "HEAD.WAV"), "synthetic wave");
        File.WriteAllText(Path.Combine(audio, "HEAD.m4a"), "stale compressed override");
        File.WriteAllText(Path.Combine(audio, "sfx.RAW"), "synthetic sfx");
        File.WriteAllText(Path.Combine(audio, "mission.wav"), "synthetic dialogue");
        var settings = new ManagerSettings { Workplace = Path.Combine(root, "radio-work") };
        foreach (var copy in new[] { false, true })
        {
            await test(copy ? "Copy and convert sends M4A without station WAVs" : "Conversion-only pulls installed station WAVs and removes them after verification", async () =>
            {
                using var log = new PrivateLog(); var runner = new RadioRunner();
                var device = new QuestDevice(runner, log, "adb", "synthetic-target");
                await new RadioStations(settings, runner, log).ConvertAndInstall(device, copy ? audio : null, default);
                if (copy) await Workflows.PushGameTree(device, audio, "Audio", default, convertedRadio: true);
                Check(runner.Commands.Any(c => c.Contains(".m4a.incoming-")));
                Check(!runner.Pushes.Any(p => RadioStations.IsStation(p, ".wav")));
                Check(runner.Commands.Any(c => c.Contains("&& rm -f") && c.Contains("HEAD.WAV")));
                Check(copy ? runner.Pulls == 0 : runner.Pulls == 1);
                Check(File.ReadAllText(Path.Combine(audio, "HEAD.WAV")) == "synthetic wave");
                Check(!Directory.EnumerateDirectories(settings.Workplace, "radio-conversion-*").Any());
                if (copy) Check(runner.Pushes.Any(p => p.EndsWith("sfx.RAW")) && runner.Pushes.Any(p => p.EndsWith("mission.wav")));
            });
        }
        await test("Copy without conversion copies WAV and clears stale compressed override", async () =>
        {
            using var log = new PrivateLog(); var runner = new RadioRunner();
            await Workflows.PushGameTree(new(runner, log, "adb", "synthetic-target"), audio, "Audio", default);
            Check(runner.Pushes.Any(p => p.EndsWith("HEAD.WAV")));
            Check(!runner.Pushes.Any(p => p.EndsWith("HEAD.m4a")));
            Check(runner.Commands.Any(c => c.StartsWith("rm -f") && c.EndsWith("HEAD.m4a'")));
            Check(runner.Pulls == 0);
        });
        foreach (var failure in new[] { "encode", "probe", "duration", "decode", "hash", "cancel" })
            await test("Radio " + failure + " failure preserves installed WAV", async () =>
            {
                using var log = new PrivateLog(); var runner = new RadioRunner { Failure = failure };
                try
                {
                    await new RadioStations(settings, runner, log).ConvertAndInstall(new(runner, log, "adb", "synthetic-target"), audio, default);
                    throw new Exception("Expected conversion failure.");
                }
                catch (IOException) { }
                catch (OperationCanceledException) when (failure == "cancel") { }
                Check(!runner.Commands.Any(c => c.Contains("&& rm -f")));
                Check(!Directory.EnumerateDirectories(settings.Workplace, "radio-conversion-*").Any());
            });
        await test("Conversion ignores dialogue and is safe when stations are already converted", async () =>
        {
            using var log = new PrivateLog(); var runner = new RadioRunner { Empty = true };
            await new RadioStations(settings, runner, log).ConvertAndInstall(new(runner, log, "adb", "synthetic-target"), null, default);
            Check(runner.Pulls == 0 && runner.Pushes.Count == 0 && !runner.Commands.Any(c => c.Contains("rm -f")));
        });
        if (Paths.FindExecutable("ffmpeg") is not null && Paths.FindExecutable("ffprobe") is not null)
            await test("Real FFmpeg converts synthetic IMA ADPCM station and validates AAC playback data", async () =>
            {
                using var log = new PrivateLog(); var codecRunner = new CommandRunner(log);
                var fixture = Path.Combine(root, "ima-radio"); Directory.CreateDirectory(fixture);
                await codecRunner.Run("ffmpeg", ["-nostdin", "-v", "error", "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=44100", "-t", "2", "-ac", "2", "-c:a", "adpcm_ima_wav", Path.Combine(fixture, "HEAD.WAV")], showOutput: false);
                var runner = new RadioRunner { CodecRunner = codecRunner };
                await new RadioStations(settings, runner, log).ConvertAndInstall(new(runner, log, "adb", "synthetic-target"), fixture, default);
                Check(runner.Commands.Any(c => c.Contains("&& rm -f")));
            });
    }

    private sealed class RadioRunner : ICommandRunner
    {
        public string Failure { get; init; } = "";
        public bool Empty { get; init; }
        public ICommandRunner? CodecRunner { get; init; }
        public List<string> Commands { get; } = [];
        public List<string> Pushes { get; } = [];
        public int Pulls { get; private set; }
        private string hash = "";
        public Task<CommandResult> Run(string executable, IEnumerable<string> args, string? workingDirectory = null,
            bool showOutput = true, bool check = true, CancellationToken token = default, string? input = null)
        {
            var a = args.ToArray();
            if (executable != "adb" && CodecRunner is not null)
                return CodecRunner.Run(executable, a, workingDirectory, showOutput, check, token, input);
            if (a.Contains("-version")) return Task.FromResult(new CommandResult(0, "fixture"));
            if (executable == "ffprobe")
            {
                if (Failure == "probe" && a[^1].EndsWith(".m4a")) throw new IOException("Invalid output.");
                if (Failure == "duration" && a[^1].EndsWith(".m4a")) return Task.FromResult(new CommandResult(0, """{"streams":[{"codec_type":"audio","codec_name":"aac","channels":2,"sample_rate":"48000"}],"format":{"duration":"1"}}"""));
                return Task.FromResult(new CommandResult(0, """{"streams":[{"codec_type":"audio","codec_name":"aac","channels":2,"sample_rate":"48000"}],"format":{"duration":"10"}}"""));
            }
            if (executable == "ffmpeg")
            {
                if (Failure == "cancel") throw new OperationCanceledException();
                if ((Failure == "encode" && a.Contains("aac")) || (Failure == "decode" && a.Contains("null"))) throw new IOException("Conversion failed.");
                if (a[^1] != "-") File.WriteAllText(a[^1], "synthetic AAC");
            }
            if (executable == "adb")
            {
                Check(!showOutput && a[0] == "-s");
                if (a[2] == "pull") { File.WriteAllText(a[^1], "synthetic wave"); Pulls++; }
                if (a[2] == "push") { Pushes.Add(a[^2]); hash = Digests.Sha256(a[^2]); }
                if (a[2] == "shell")
                {
                    Commands.Add(a[3]);
                    if (a[3].StartsWith("find")) return Task.FromResult(new CommandResult(0, QuestDevice.GameData + (Empty ? "/Audio/HEAD.m4a" : "/Audio/HEAD.WAV") + "\n" + QuestDevice.GameData + "/Audio/mission.wav\n"));
                    if (a[3].StartsWith("sha256sum")) return Task.FromResult(new CommandResult(0, Failure == "hash" ? "bad" : hash));
                }
            }
            return Task.FromResult(new CommandResult(0, ""));
        }
    }
}
