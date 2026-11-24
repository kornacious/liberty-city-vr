using System.ComponentModel;

namespace LibertyManager;

internal sealed class FfmpegTools(ManagerSettings settings, ICommandRunner runner, PrivateLog log)
{
    private const string Build = "ffmpeg-n9.0.2-62-gac1c6e003f";
    private const string Release = "https://github.com/FFmpeg-Builds/FFmpeg-Builds/releases/download/autobuild-2026-11-23-12-43/";
    private static bool Windows => OperatingSystem.IsWindows();
    private static string Package => Build + (Windows ? "-win64-lgpl-9.0" : "-linux64-lgpl-9.0");
    private static string Archive => Package + (Windows ? ".zip" : ".tar.xz");
    private static string Hash => Windows
        ? "e174af1bc75771db087a1e9ab789d1f004215ca5b545d5ce826e2c8eec873efb"
        : "63648ca32d6f831b6829f60d9fef3c9745f4dc4ad33d9ddd98ba2b5bca87e9e3";
    private string Cache => Paths.Child(settings.Workplace, ".tools/" + Package);
    private static string Executable(string name) => name + (Windows ? ".exe" : "");
    private string CachedTool(string name) => Paths.Child(Cache, "bin/" + Executable(name));

    private async Task<bool> Works(string executable, CancellationToken token)
    {
        using var timeout = CancellationTokenSource.CreateLinkedTokenSource(token);
        timeout.CancelAfter(TimeSpan.FromSeconds(10));
        try
        {
            return (await runner.Run(executable, ["-version"], showOutput: false, check: false, token: timeout.Token)).ExitCode == 0;
        }
        catch (OperationCanceledException) when (!token.IsCancellationRequested) { return false; }
        catch (Exception error) when (error is IOException or Win32Exception or UnauthorizedAccessException) { return false; }
    }

    private async Task<string?> Find(string? configured, string name, CancellationToken token)
    {
        token.ThrowIfCancellationRequested();
        var preferred = string.IsNullOrWhiteSpace(configured) ? null : Paths.FindExecutable(configured);
        foreach (var candidate in new[] { preferred, Paths.FindExecutable(name), CachedTool(name) }.Distinct())
            if (candidate is not null && File.Exists(candidate) && await Works(candidate, token)) return candidate;
        return null;
    }

    private void RequirePackage()
    {
        if (!settings.AllowToolDownloads && !File.Exists(Paths.Child(settings.Workplace, ".downloads/" + Archive)))
            throw new IOException("Select working FFmpeg and ffprobe executables or allow verified tool downloads in Setup help.");
        if (!Windows && Paths.FindExecutable("tar") is null)
            throw new IOException("Install tar to unpack the verified FFmpeg tools, or select working executables in Setup help.");
    }

    public async Task<bool> Check(CancellationToken token)
    {
        var ffmpeg = await Find(settings.Ffmpeg, "ffmpeg", token);
        var ffprobe = await Find(settings.Ffprobe, "ffprobe", token);
        if (ffmpeg is not null && ffprobe is not null) return true;
        RequirePackage();
        return false;
    }

    public async Task<ManagerSettings> Resolve(CancellationToken token)
    {
        var ffmpeg = await Find(settings.Ffmpeg, "ffmpeg", token);
        var ffprobe = await Find(settings.Ffprobe, "ffprobe", token);
        if (ffmpeg is not null && ffprobe is not null) return settings with { Ffmpeg = ffmpeg, Ffprobe = ffprobe };

        RequirePackage();
        var archive = await new Toolchain(settings, runner as CommandRunner ?? new CommandRunner(log), log)
            .Download(Release + Archive, Archive, Hash, token);
        var stage = Paths.Child(settings.Workplace, ".tools/.ffmpeg-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(stage);
        try
        {
            if (Windows) Toolchain.ExtractZip(archive, stage, token);
            else await runner.Run(Paths.FindExecutable("tar")!, ["-xJf", archive, "-C", stage], token: token);
            var unpacked = Paths.Child(stage, Package);
            foreach (var name in new[] { "ffmpeg", "ffprobe" })
            {
                var path = Paths.Child(unpacked, "bin/" + Executable(name));
                if (!File.Exists(path) || !await Works(path, token))
                    throw new IOException($"The downloaded {name} could not start on this computer.");
            }
            var previous = Paths.Child(stage, "previous");
            if (Directory.Exists(Cache)) Directory.Move(Cache, previous);
            try { Directory.Move(unpacked, Cache); }
            catch
            {
                if (Directory.Exists(previous)) Directory.Move(previous, Cache);
                throw;
            }
        }
        finally { Directory.Delete(stage, recursive: true); }
        return settings with { Ffmpeg = ffmpeg ?? CachedTool("ffmpeg"), Ffprobe = ffprobe ?? CachedTool("ffprobe") };
    }
}
