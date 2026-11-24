using System.Formats.Tar;
using System.IO.Compression;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;

namespace LibertyManager;

public sealed class Toolchain(ManagerSettings settings, CommandRunner runner, PrivateLog log)
{
    public const string Re3Commit = "b050e1cc63b1869e6b514ab2f1a00ef4969e0c84";
    public const string Re3Url = "https://github.com/mrxenginner/re3.git";
    private string Downloads => Path.Combine(settings.Workplace, ".downloads");
    private string Tools => Path.Combine(settings.Workplace, ".tools");
    private static string Exe => OperatingSystem.IsWindows() ? ".exe" : "";
    private string PortableGit => Path.Combine(Tools, "mingit-2.55.0.3", "cmd/git.exe");
    internal bool HasGit => Paths.FindExecutable("git") is not null || (OperatingSystem.IsWindows() && File.Exists(PortableGit));
    public string Sdk => string.IsNullOrWhiteSpace(settings.AndroidSdk) ? Path.Combine(settings.Workplace, ".android-sdk") : Path.GetFullPath(settings.AndroidSdk);
    private string JavaHome => string.IsNullOrWhiteSpace(settings.JavaHome) ? Path.Combine(Tools, "jdk-21.0.11+10") : Path.GetFullPath(settings.JavaHome);
    internal bool HasJava => File.Exists(Path.Combine(JavaHome, "bin", "java" + Exe));
    private static readonly Dictionary<string, string> SdkPackages = new()
    {
        ["platforms;android-35"] = "platforms/android-35/android.jar",
        ["build-tools;34.0.0"] = "build-tools/34.0.0/source.properties",
        ["platform-tools"] = "platform-tools/adb" + Exe,
        ["ndk;27.2.12479018"] = "ndk/27.2.12479018/source.properties",
        ["cmake;3.22.1"] = "cmake/3.22.1/bin/cmake" + Exe
    };
    internal bool NeedsSdkPackages => SdkPackages.Values.Any(path => !File.Exists(Path.Combine(Sdk, path)));
    // The required packages all reference Google's standard Android SDK license
    // with this digest in their SDK Manager metadata.
    private const string AndroidSdkLicenseHash = "24333f8a63b6825ea9c5514f83c2829b004d1fee";
    private string AndroidSdkLicense => Paths.Child(Sdk, "licenses/android-sdk-license");
    internal bool HasAndroidSdkLicense => File.Exists(AndroidSdkLicense) &&
        File.ReadLines(AndroidSdkLicense).Any(line => line.Trim().Equals(AndroidSdkLicenseHash, StringComparison.OrdinalIgnoreCase));
    public string Java { get; private set; } = "";

    public string? FindAdb()
    {
        if (!string.IsNullOrWhiteSpace(settings.Adb)) return Paths.FindExecutable(settings.Adb);
        return new[] { Path.Combine(Sdk, "platform-tools", "adb" + Exe),
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Android/Sdk/platform-tools/adb" + Exe),
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Android/Sdk/platform-tools/adb" + Exe) }
            .FirstOrDefault(File.Exists) ?? Paths.FindExecutable("adb");
    }

    public async Task<string> Git(CancellationToken token)
    {
        var git = Paths.FindExecutable("git");
        if (git is not null) return git;
        if (!OperatingSystem.IsWindows()) throw new IOException("Install Git using your Linux package manager.");
        git = PortableGit;
        if (!File.Exists(git))
        {
            var archive = await Download("https://github.com/git-for-windows/git/releases/download/v2.55.0.windows.3/MinGit-2.55.0.3-64-bit.zip",
                "MinGit-2.55.0.3-64-bit.zip", "F48E2D2DC74A24454ADC6D8FD0AC25BF9C2386F19CFB06202B9465AAAD4F9F05", token);
            ExtractZip(archive, Path.GetDirectoryName(Path.GetDirectoryName(git))!, token);
        }
        runner.EnvironmentOverrides["PATH"] = Path.GetDirectoryName(git) + Path.PathSeparator + Environment.GetEnvironmentVariable("PATH");
        return git;
    }

    public async Task EnsureJava(CancellationToken token)
    {
        var home = JavaHome;
        Java = Path.Combine(home, "bin", "java" + Exe);
        if (!File.Exists(Java))
        {
            if (!string.IsNullOrWhiteSpace(settings.JavaHome)) throw new IOException("Selected JDK is missing bin/java. Choose JDK 21 in Settings.");
            var windows = OperatingSystem.IsWindows();
            var name = windows ? "OpenJDK21U-jdk_x64_windows_hotspot_21.0.11_10.zip" : "OpenJDK21U-jdk_x64_linux_hotspot_21.0.11_10.tar.gz";
            var file = await Download("https://github.com/adoptium/temurin21-binaries/releases/download/jdk-21.0.11%2B10/" + name,
                name, windows ? "D3625E7CADF23787EA540229544B6E2AB494B3B54DA1801879E583E1DFEE0A64" : "4B2220E232A97997B436CA6AB15CBF70171ECFF52958A46159DFA5A8C44CA4DE", token);
            if (windows) ExtractZip(file, Tools, token);
            else
            {
                Directory.CreateDirectory(Tools);
                using var stream = File.OpenRead(file);
                using var gzip = new GZipStream(stream, CompressionMode.Decompress);
                TarFile.ExtractToDirectory(gzip, Tools, overwriteFiles: true);
            }
        }
        var version = await runner.Run(Java, ["-version"], showOutput: false, token: token);
        if (!Regex.IsMatch(version.Output, "version \"21[.\"]")) throw new IOException("JDK 21 is required.");
        runner.EnvironmentOverrides["JAVA_HOME"] = home;
        runner.EnvironmentOverrides["ANDROID_HOME"] = Sdk;
        runner.EnvironmentOverrides["ANDROID_SDK_ROOT"] = Sdk;
    }

    public async Task EnsureSdk(bool acceptLicenses, CancellationToken token)
    {
        token.ThrowIfCancellationRequested();
        var missing = SdkPackages.Where(p => !File.Exists(Path.Combine(Sdk, p.Value))).Select(p => p.Key).ToArray();
        var hasLicense = HasAndroidSdkLicense;
        if ((!hasLicense || missing.Length > 0) && !acceptLicenses)
            throw new IOException("Accept Google’s Android SDK license terms before installing or building for Quest 3.");
        if (!hasLicense)
        {
            // Record only the terms linked in Setup. `sdkmanager --licenses`
            // also offers unrelated add-on licenses such as Google TV.
            var license = AndroidSdkLicense;
            Directory.CreateDirectory(Path.GetDirectoryName(license)!);
            File.AppendAllText(license, "\n" + AndroidSdkLicenseHash + "\n");
        }
        if (missing.Length == 0) return;

        var toolHome = Path.Combine(Sdk, "cmdline-tools/latest");
        var jar = Path.Combine(toolHome, "lib/sdkmanager-classpath.jar");
        // Google's distribution uses the classpath JAR and its manifest dependencies.
        if (!File.Exists(jar))
        {
            var windows = OperatingSystem.IsWindows();
            var name = windows ? "commandlinetools-win-15859902_latest.zip" : "commandlinetools-linux-15859902_latest.zip";
            var zip = await Download("https://dl.google.com/android/repository/" + name, name,
                windows ? "90AE805D20434428BFFCB699C290860F19BB5F66A67E6B330067E3DE801FB04A" : "4E4C464F145A7512B57D088AC6C278C03C9EEA610886B35A5E0804E74EEDF583", token);
            var stage = Path.Combine(Tools, "cmdline-" + Guid.NewGuid().ToString("N"));
            ExtractZip(zip, stage, token);
            Paths.CopyTree(Path.Combine(stage, "cmdline-tools"), toolHome, token);
            Directory.Delete(stage, true);
        }
        log.Write("Installing Android SDK components: " + string.Join(", ", missing));
        string[] command = ["-Dcom.android.sdklib.toolsdir=" + toolHome, "-classpath", jar, "com.android.sdklib.tool.sdkmanager.SdkManagerCli", "--sdk_root=" + Sdk];
        var installed = await runner.Run(Java, command.Concat(missing), check: false, token: token);
        if (installed.Output.Contains("license", StringComparison.OrdinalIgnoreCase) &&
            installed.Output.Contains("not accepted", StringComparison.OrdinalIgnoreCase))
            throw new IOException("SDK Manager requires additional or updated license terms. Review them in Android Studio’s SDK Manager, then try Setup again.");
        if (installed.ExitCode != 0 || NeedsSdkPackages)
            throw new IOException("SDK Manager did not install all required Android components. See the setup log.");
    }

    public async Task<string> Gradle(CancellationToken token)
    {
        var home = Path.Combine(Tools, "gradle-8.13");
        if (!File.Exists(Path.Combine(home, "lib/gradle-gradle-cli-main-8.13.jar")))
        {
            var zip = await Download("https://services.gradle.org/distributions/gradle-8.13-bin.zip", "gradle-8.13-bin.zip",
                "20F1B1176237254A6FC204D8434196FA11A4CFB387567519C61556E8710AED78", token);
            ExtractZip(zip, Tools, token);
        }
        return home;
    }

    public async Task<string> Re3(CancellationToken token)
    {
        var git = await Git(token);
        var source = Path.Combine(settings.Workplace, "re3-public-b050e1c");
        if (!Directory.Exists(source))
        {
            await runner.Run(git, ["clone", "--no-checkout", "-b", "master", Re3Url, source], token: token);
            await runner.Run(git, ["-C", source, "checkout", "--detach", Re3Commit], token: token);
        }
        var head = await runner.Run(git, ["-C", source, "rev-parse", "HEAD"], showOutput: false, token: token);
        var dirty = await runner.Run(git, ["-C", source, "status", "--porcelain"], showOutput: false, token: token);
        if (head.Output.Trim() != Re3Commit || dirty.Output.Trim().Length != 0)
            throw new IOException("Cached re3 source is modified or at another revision. Preserve it and choose another workplace.");
        return source;
    }

    private const string AstcVersion = "5.7.1";
    private static string AstcArchive => $"astcenc-{AstcVersion}-{(OperatingSystem.IsWindows() ? "windows" : "linux")}-x64.zip";
    private string AstcDirectory => Path.Combine(Tools, Path.GetFileNameWithoutExtension(AstcArchive));
    private string AstcFallback => Path.Combine(AstcDirectory, "bin", "astcenc-sse2" + Exe);

    internal bool CanUseAstcEncoder => AstcTextureCache.FindEncoder(settings) is not null ||
        (string.IsNullOrWhiteSpace(settings.AstcEncoder) &&
            (File.Exists(AstcFallback) || File.Exists(Path.Combine(Downloads, AstcArchive)) || settings.AllowToolDownloads));

    public async Task<string> AstcEncoder(CancellationToken token)
    {
        if (AstcTextureCache.FindEncoder(settings) is { } executable)
            return executable;
        if (!string.IsNullOrWhiteSpace(settings.AstcEncoder))
            throw new IOException("The configured Arm ASTC encoder was not found.");
        if (RuntimeInformation.ProcessArchitecture != Architecture.X64)
            throw new IOException("Install a native Arm astcenc or choose its executable in Manager Settings.");
        var name = AstcArchive;
        var directory = AstcDirectory;
        var fallback = AstcFallback;
        if (!File.Exists(fallback))
        {
            var digest = OperatingSystem.IsWindows() ?
                "117ED7CC2B0CC4F3D5774EAA1BDE3A2BA75695C2F490E84E4A08D1726C963DB8" :
                "F1D576F0801E7701548DA78D782F4BF70DFDE3673A238D1A2AC0EBD3511B2F37";
            var zip = await Download($"https://github.com/ARM-software/astc-encoder/releases/download/{AstcVersion}/{name}",
                name, digest, token);
            ExtractZip(zip, directory, token);
        }
        if (!File.Exists(fallback))
            throw new IOException("Arm ASTC encoder was not found in the verified tool archive.");
        var avx2 = Path.Combine(directory, "bin", "astcenc-avx2" + Exe);
        var useAvx2 = System.Runtime.Intrinsics.X86.Avx2.IsSupported &&
            System.Runtime.Intrinsics.X86.Popcnt.IsSupported &&
            (System.Runtime.Intrinsics.X86.X86Base.CpuId(1, 0).Ecx & (1 << 29)) != 0;
        return useAvx2 && File.Exists(avx2) ? avx2 : fallback;
    }

    public string FindCMake() => !string.IsNullOrWhiteSpace(settings.CMake)
        ? Paths.FindExecutable(settings.CMake) ?? throw new IOException("Configured CMake was not found.")
        : Paths.FindExecutable("cmake") ?? (File.Exists(Path.Combine(Sdk, "cmake/3.22.1/bin/cmake" + Exe))
            ? Path.Combine(Sdk, "cmake/3.22.1/bin/cmake" + Exe) : FindCompilerTool("cmake") ??
                throw new IOException("Install CMake 3.22+ or configure its executable in Settings."));

    internal string? FindCompilerTool(string name)
    {
        if (runner.EnvironmentOverrides.TryGetValue("PATH", out var search))
            foreach (var directory in search.Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries))
            {
                var path = Path.Combine(directory.Trim('"'), name + Exe);
                if (File.Exists(path)) return path;
            }
        return Paths.FindExecutable(name);
    }

    internal string? FindNinja(string cmake)
    {
        var directory = Path.GetDirectoryName(cmake)!;
        return FindCompilerTool("ninja") ?? new[]
        {
            Path.Combine(directory, "ninja" + Exe),
            Path.GetFullPath(Path.Combine(directory, "../../Ninja/ninja" + Exe))
        }.FirstOrDefault(File.Exists);
    }

    public async Task<string> Download(string url, string name, string sha256, CancellationToken token)
    {
        if (RuntimeInformation.ProcessArchitecture != Architecture.X64) throw new PlatformNotSupportedException("Automatic tool downloads support x64. Configure native tools on this platform.");
        var path = Paths.Child(Downloads, name);
        Directory.CreateDirectory(Downloads);
        if (!File.Exists(path))
        {
            if (!settings.AllowToolDownloads) throw new IOException("Tool downloads are disabled in Settings.");
            log.Write("Downloading " + name);
            var incoming = path + ".verified-" + Guid.NewGuid().ToString("N");
            try
            {
                await DownloadFile(url, incoming, log, token);
                if (!Digests.Sha256(incoming).Equals(sha256, StringComparison.OrdinalIgnoreCase))
                    throw new InvalidDataException("Download checksum mismatch: " + name);
                try { File.Move(incoming, path); }
                catch (IOException) when (File.Exists(path)) { }
            }
            finally { File.Delete(incoming); }
        }
        if (!Digests.Sha256(path).Equals(sha256, StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException("Download checksum mismatch. Remove the cached archive and retry: " + name);
        return path;
    }

    public static async Task DownloadFile(string url, string path, PrivateLog log, CancellationToken token)
    {
        using var client = new HttpClient { Timeout = Timeout.InfiniteTimeSpan };
        using var limit = CancellationTokenSource.CreateLinkedTokenSource(token);
        limit.CancelAfter(TimeSpan.FromHours(1));
        using var response = await client.GetAsync(url, HttpCompletionOption.ResponseHeadersRead, limit.Token);
        response.EnsureSuccessStatusCode();
        var partial = path + ".partial-" + Guid.NewGuid().ToString("N");
        try
        {
            await using (var output = File.Create(partial))
            await using (var source = await response.Content.ReadAsStreamAsync(limit.Token))
            {
                var buffer = new byte[1024 * 1024];
                long total = 0;
                var last = DateTime.UtcNow;
                int bytes;
                while ((bytes = await source.ReadAsync(buffer, limit.Token)) > 0)
                {
                    await output.WriteAsync(buffer.AsMemory(0, bytes), limit.Token);
                    total += bytes;
                    if ((DateTime.UtcNow - last).TotalSeconds > 2)
                    { log.Write($"Downloaded {total / 1048576} MiB"); last = DateTime.UtcNow; }
                }
            }
            File.Move(partial, path);
        }
        finally { if (File.Exists(partial)) File.Delete(partial); }
    }

    public static void ExtractZip(string zip, string destination, CancellationToken token)
    {
        using var archive = ZipFile.OpenRead(zip);
        Directory.CreateDirectory(destination);
        foreach (var entry in archive.Entries)
        {
            token.ThrowIfCancellationRequested();
            var rel = entry.FullName.TrimEnd('/');
            if (rel.Length == 0) continue;
            var target = Paths.Child(destination, rel);
            if (((entry.ExternalAttributes >> 16) & 0xF000) == 0xA000) throw new InvalidDataException("Archive contains a symbolic link.");
            if (entry.FullName.EndsWith('/')) { Directory.CreateDirectory(target); continue; }
            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            entry.ExtractToFile(target, true);
            if (!OperatingSystem.IsWindows())
            {
                var mode = (UnixFileMode)((entry.ExternalAttributes >> 16) & 0x1FF);
                File.SetUnixFileMode(target, mode == 0 ? UnixFileMode.UserRead | UnixFileMode.UserWrite : mode);
            }
        }
    }
}
