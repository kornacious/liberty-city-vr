using System.Runtime.InteropServices;
using System.Text.Json;
using Microsoft.Win32;

namespace LibertyManager;

public sealed record PcvrPaths(string Install, string Data, string User)
{
    public static PcvrPaths From(ManagerSettings settings)
    {
        var root = Path.Combine(settings.Workplace, "windows-pcvr");
        string Resolve(string value, string name)
        {
            if (string.IsNullOrWhiteSpace(value)) return Path.GetFullPath(Path.Combine(root, name));
            if (!Path.IsPathFullyQualified(value)) throw new InvalidDataException("PCVR directories must be absolute paths.");
            return Path.GetFullPath(value);
        }
        return new(Resolve(settings.PcvrInstallDirectory, "app"), Resolve(settings.PcvrDataDirectory, "data"), Resolve(settings.PcvrUserDirectory, "user"));
    }
    public void Validate(ManagerSettings settings)
    {
        var targets = new[] { Install, Data, User };
        foreach (var path in targets)
        {
            Paths.NoLinks(path);
            foreach (var source in new[] { settings.Repository, settings.GameDirectory,
                         Path.Combine(settings.Workplace, "re3-public-b050e1c"), Path.Combine(settings.Workplace, "windows-pcvr-builds") }.Where(p => !string.IsNullOrWhiteSpace(p)))
                if (Paths.Within(source, path) || Paths.Within(path, source))
                    throw new InvalidDataException("Keep PCVR install, data and user directories separate from retail data, source trees and build directories.");
        }
        for (var i=0; i<targets.Length; ++i) for (var j=i+1; j<targets.Length; ++j)
            if (Paths.Within(targets[i], targets[j]) || Paths.Within(targets[j], targets[i]))
                throw new InvalidDataException("PCVR install, data and user directories must not overlap.");
    }
}

public sealed record PcvrInstallation(int Version, string Directory);

public sealed class PcvrWorkflow(ManagerSettings settings, CommandRunner runner, PrivateLog log)
{
    private const string OpenXrArchive = "https://github.com/KhronosGroup/OpenXR-SDK/archive/refs/tags/release-1.1.58.zip";
    private const string OpenXrHash = "43d75ce00e31be8f662b58e8c74af8c04156bcaa7acbfc19112aa2880f3a3669";
    private static void RequireWindows()
    {
        if (!OperatingSystem.IsWindows() || RuntimeInformation.ProcessArchitecture != Architecture.X64)
            throw new PlatformNotSupportedException("Windows PCVR requires a Windows x64 host. Quest and other host tools remain available separately.");
    }
    private PcvrPaths Layout()
    {
        var paths=PcvrPaths.From(settings); paths.Validate(settings); return paths;
    }
    public static void ValidatePreparedData(string path)
    {
        Paths.NoLinks(path);
        if (!File.Exists(Path.Combine(path,".libertyvr-pcvr-data")) ||
            !Directory.Exists(Path.Combine(path,"data")) || !Directory.Exists(Path.Combine(path,"models")))
            throw new IOException("No prepared PCVR data copy. Enable Prepare local PCVR data under Build and install, or choose an existing prepared directory.");
    }
    // Versioned installs avoid replacing a running EXE. Publish the pointer only
    // after CMake installation and all required files have been verified.
    public static string ResolveInstallation(string root)
    {
        Paths.NoLinks(root);
        var manifest=Paths.Child(root,"current.json");
        if (!File.Exists(manifest)) throw new IOException("No PCVR build installed. Select Windows PCVR under Build and install first.");
        var current=JsonSerializer.Deserialize<PcvrInstallation>(File.ReadAllText(manifest)) ?? throw new InvalidDataException("Invalid PCVR installation record.");
        if (current.Version!=1 || !current.Directory.StartsWith("versions/",StringComparison.Ordinal)) throw new InvalidDataException("Unsupported PCVR installation record.");
        var directory=Paths.Child(root,current.Directory);
        ValidateBinaries(directory);
        return directory;
    }
    private static void ValidateBinaries(string directory)
    {
        foreach (var name in new[] { "libertyvr-game.exe", "libertyvr-pcvr.exe", "openxr_loader.dll", "openxr-notices/LICENSE", "openxr-notices/JsonCpp-LICENSE", "IMGUI_LICENSE.txt", "LIBRW_LICENSE.txt" })
            if (!File.Exists(Paths.Child(directory,name))) throw new IOException("PCVR installation is incomplete: " + name);
    }
    public static void PublishInstallation(string root, string relative)
    {
        if (!relative.StartsWith("versions/",StringComparison.Ordinal)) throw new InvalidDataException("Installations must use a versioned subdirectory.");
        var directory=Paths.Child(root,relative); ValidateBinaries(directory);
        var destination=Paths.Child(root,"current.json");
        var temporary=Paths.Child(root,".current-"+Guid.NewGuid().ToString("N"));
        try
        {
            File.WriteAllText(temporary,JsonSerializer.Serialize(new PcvrInstallation(1,relative)));
            File.Move(temporary,destination,true);
        }
        finally { if (File.Exists(temporary)) File.Delete(temporary); }
    }
    private async Task<(string Cmake,string Ninja,string Include,string Library,string Shader)> FindTools(CancellationToken token)
    {
        await WindowsCompiler.Configure(runner,token);
        string? Find(string name)
        {
            if (runner.EnvironmentOverrides.TryGetValue("PATH",out var search))
                foreach (var dir in search.Split(Path.PathSeparator))
                { var path=Path.Combine(dir,name+".exe"); if (File.Exists(path)) return path; }
            return Paths.FindExecutable(name);
        }
        if (Find("cl") is null) throw new IOException("Install Visual Studio C++ x64 build tools.");
        var cmake=string.IsNullOrWhiteSpace(settings.CMake) ? Find("cmake") : Paths.FindExecutable(settings.CMake);
        if (cmake is null) throw new IOException("Install CMake and configure its executable in Settings.");
        var ninja=Find("ninja") ?? new[] { Path.Combine(Path.GetDirectoryName(cmake)!,"ninja.exe"),
            Path.GetFullPath(Path.Combine(Path.GetDirectoryName(cmake)!,"../../Ninja/ninja.exe")) }.FirstOrDefault(File.Exists) ?? "";
        if (!File.Exists(ninja)) throw new IOException("Install Ninja (included with Visual Studio CMake tools) and add it to PATH.");
        var sdk=settings.VulkanSdk;
        if (string.IsNullOrWhiteSpace(sdk)) throw new IOException("Install a Vulkan SDK and select its directory in Settings (or set VULKAN_SDK).");
        var include=Path.Combine(sdk,"Include");
        var library=new[] { Path.Combine(sdk,"Lib/vulkan-1.lib"), Path.Combine(sdk,"vulkan-1.lib") }.FirstOrDefault(File.Exists);
        var shader=string.IsNullOrWhiteSpace(settings.PcvrShaderCompiler) ? Path.Combine(sdk,"Bin/glslc.exe") : settings.PcvrShaderCompiler;
        if (!File.Exists(Path.Combine(include,"vulkan/vulkan.h")) || library is null || !File.Exists(shader))
            throw new IOException("Vulkan SDK headers, x64 vulkan-1.lib and glslc are required. Configure the SDK and optional shader compiler in Settings.");
        await runner.Run(cmake,["--version"],token:token);
        await runner.Run(ninja,["--version"],token:token);
        await runner.Run(shader,["--version"],token:token);
        log.Write("Windows x64 C++ tools, CMake, Ninja and Vulkan SDK found. No Android tools or headset deployment required.");
        return (cmake,ninja,Path.GetFullPath(include),Path.GetFullPath(library),Path.GetFullPath(shader));
    }
    private async Task<string> OpenXrSource(Toolchain tools,CancellationToken token)
    {
        string source;
        if (!string.IsNullOrWhiteSpace(settings.PcvrOpenXrSource)) source=Path.GetFullPath(settings.PcvrOpenXrSource);
        else
        {
            source=Path.Combine(settings.Workplace,".tools/OpenXR-SDK-release-1.1.58");
            if (!Directory.Exists(source))
            {
                var zip=await tools.Download(OpenXrArchive,"openxr-sdk-1.1.58.zip",OpenXrHash,token);
                var extracted=Path.Combine(settings.Workplace,".tools/openxr-extract-"+Guid.NewGuid().ToString("N"));
                Toolchain.ExtractZip(zip,extracted,token);
                var unpacked=Path.Combine(extracted,"OpenXR-SDK-release-1.1.58");
                Directory.Move(unpacked,source);
            }
        }
        Paths.NoLinks(source);
        foreach (var file in new[] { "CMakeLists.txt", "LICENSE", "COPYING.adoc", "src/loader/xr_generated_loader.cpp", "src/external/jsoncpp/LICENSE" })
            if (!File.Exists(Paths.Child(source,file))) throw new IOException("Choose the complete OpenXR-SDK 1.1.58 source directory, or leave it blank for the pinned download.");
        return source;
    }
    public async Task Build(ActionOptions options,CancellationToken token)
    {
        RequireWindows(); var paths=Layout();
        if (!options.PcvrBuildOnly)
        {
            if (options.PcvrPrepareData && !Directory.Exists(paths.Data) && !File.Exists(paths.Data)) Workflows.ValidateGameDirectory(settings.GameDirectory);
            else ValidatePreparedData(paths.Data);
        }
        var found=await FindTools(token);
        var tools=new Toolchain(settings,runner,log);
        var sdk=await OpenXrSource(tools,token);
        var source=await tools.Re3(token); var git=await tools.Git(token);
        var id=DateTime.Now.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N")[..6];
        var work=Path.Combine(settings.Workplace,"windows-pcvr-builds",id);
        var assembled=Path.Combine(work,"source"); var build=Path.Combine(work,"build");
        await Workflows.Assemble(settings.Repository,source,assembled,git,runner,log,token);
        await runner.Run(found.Cmake,["-S",Path.Combine(assembled,"windows"),"-B",build,"-G","Ninja",
            "-DCMAKE_BUILD_TYPE=Release","-DCMAKE_MAKE_PROGRAM="+found.Ninja,"-DLIBERTYVR_RE3_ROOT="+assembled,
            "-DVulkan_INCLUDE_DIR="+found.Include,"-DVulkan_LIBRARY="+found.Library,"-DGLSLC="+found.Shader,
            "-DCMAKE_DISABLE_FIND_PACKAGE_OpenXR=TRUE","-DFETCHCONTENT_SOURCE_DIR_OPENXR_SDK="+sdk,
            "-DFETCHCONTENT_FULLY_DISCONNECTED=ON","-DLIBERTYVR_GPU_TESTS="+(options.PcvrGpuTests ? "ON" : "OFF"),
            "-DLIBERTYVR_AUDIO_TESTS="+(options.PcvrAudioTests ? "ON" : "OFF")],token:token);
        await runner.Run(found.Cmake,["--build",build,"--parallel","8"],token:token);
        var ctest=Path.Combine(Path.GetDirectoryName(found.Cmake)!,"ctest.exe");
        await runner.Run(ctest,["--test-dir",build,"--output-on-failure"],token:token);
        log.Write("Windows Release build and selected checks passed: "+build);
        if (options.PcvrBuildOnly) { log.Write("Build only: installation and game-data preparation skipped."); return; }
        if (options.PcvrPrepareData && !Directory.Exists(paths.Data))
            Workflows.PrepareWindowsData(settings.Repository,settings.GameDirectory,paths.Data,token);
        ValidatePreparedData(paths.Data);
        Workflows.RefreshWindowsPortFiles(settings.Repository,paths.Data,token);
        Directory.CreateDirectory(paths.Install);
        var relative="versions/"+id; var install=Paths.Child(paths.Install,relative);
        await runner.Run(found.Cmake,["--install",build,"--prefix",install],token:token);
        foreach (var notice in new[] { "LICENSE", "NOTICE.md" })
            File.Copy(Path.Combine(settings.Repository,notice),Path.Combine(install,notice),true);
        token.ThrowIfCancellationRequested(); PublishInstallation(paths.Install,relative);
        log.Write("Windows PCVR installed: "+install+". Previous versions, game data and saves preserved.");
        log.Write("Use Launch PCVR to play, or PCVR diagnostics to check the active runtime.");
    }
    public static string ValidateRuntimeManifest(string path)
    {
        if (!File.Exists(path)) throw new IOException("The selected OpenXR runtime manifest does not exist. Select a PC OpenXR runtime in its own application.");
        try
        {
            using var document=JsonDocument.Parse(File.ReadAllText(path));
            var library=document.RootElement.GetProperty("runtime").GetProperty("library_path").GetString();
            if (string.IsNullOrWhiteSpace(library)) throw new InvalidDataException("OpenXR runtime manifest has no library path.");
            var absolute=Path.GetFullPath(library,Path.GetDirectoryName(Path.GetFullPath(path))!);
            if (!File.Exists(absolute)) throw new IOException("The selected OpenXR runtime library is missing. Repair or select the runtime in its own application.");
            return absolute;
        }
        catch(Exception error) when (error is JsonException or KeyNotFoundException or InvalidOperationException)
        { throw new InvalidDataException("The selected OpenXR runtime manifest is malformed. Repair or select the runtime in its own application."); }
    }
    private void Runtime()
    {
        if (!OperatingSystem.IsWindows()) throw new PlatformNotSupportedException("PCVR runtime checks require Windows.");
        var manifest=Environment.GetEnvironmentVariable("XR_RUNTIME_JSON");
        if (string.IsNullOrWhiteSpace(manifest))
        {
            using var registry=RegistryKey.OpenBaseKey(RegistryHive.LocalMachine,RegistryView.Registry64);
            using var key=registry.OpenSubKey(@"SOFTWARE\Khronos\OpenXR\1");
            manifest=key?.GetValue("ActiveRuntime") as string;
        }
        if (string.IsNullOrWhiteSpace(manifest)) throw new IOException("No active PC OpenXR runtime. Select one in Virtual Desktop, SteamVR or your headset software, then connect the headset.");
        ValidateRuntimeManifest(manifest);
        log.Write("Active x64 OpenXR runtime manifest and library found. Runtime selection was not changed.");
    }
    public static async Task Probe(ICommandRunner commands,string executable,PrivateLog log,CancellationToken token)
    {
        foreach (var mode in new[] { "--vulkan-probe", "--probe" })
        {
            using var limit=CancellationTokenSource.CreateLinkedTokenSource(token); limit.CancelAfter(TimeSpan.FromSeconds(60));
            CommandResult result;
            try { result=await commands.Run(executable,[mode],check:false,token:limit.Token); }
            catch (OperationCanceledException) when (!token.IsCancellationRequested) { throw new IOException("PCVR runtime probe timed out. Restart the runtime and reconnect the headset."); }
            if (result.ExitCode!=0)
            {
                if (mode=="--probe" && result.Output.Contains("OpenXR -35",StringComparison.Ordinal))
                    throw new IOException("No PCVR headset available to the active OpenXR runtime. Connect it through your PCVR software; ADB alone is insufficient.");
                throw new IOException(mode=="--vulkan-probe" ? "Vulkan probe failed. Check GPU driver and Vulkan 1.1 multiview support." : "OpenXR/Vulkan session probe failed. Check the active runtime, headset connection and Vulkan support; see the action log.");
            }
        }
        log.Write("Vulkan and OpenXR device/session probes passed. This does not verify stereo presentation or gameplay.");
    }
    public async Task Diagnose(CancellationToken token)
    {
        RequireWindows(); var paths=Layout(); Runtime();
        var install=ResolveInstallation(paths.Install);
        await Probe(runner,Path.Combine(install,"libertyvr-pcvr.exe"),log,token);
    }
    public static string[] LaunchArguments(PcvrPaths paths,int frames,string questProfile="")
    {
        if (frames<0 || frames>1000000) throw new InvalidDataException("PCVR frame limit must be between 0 and 1000000 (0 means unlimited).");
        string[] arguments=["--game-data",paths.Data,"--user-data",paths.User,"--frames",frames.ToString(System.Globalization.CultureInfo.InvariantCulture)];
        if(string.IsNullOrWhiteSpace(questProfile)) return arguments;
        Paths.NoLinks(questProfile);
        if(!File.Exists(questProfile) || new FileInfo(questProfile).Length>32768) throw new InvalidDataException("Choose a local Quest profile exported by a profiler capture (maximum 32 KiB).");
        return [..arguments,"--quest-profile",Path.GetFullPath(questProfile)];
    }
    public async Task Launch(ActionOptions options,CancellationToken token)
    {
        RequireWindows(); var paths=Layout(); var arguments=LaunchArguments(paths,options.PcvrFrameLimit,options.PcvrQuestProfile);
        ValidatePreparedData(paths.Data); var install=ResolveInstallation(paths.Install); Runtime();
        var engine=PcvrMods.SelectEngine(paths);
        await Probe(runner,Path.Combine(install,"libertyvr-pcvr.exe"),log,token);
        Directory.CreateDirectory(paths.User);
        log.Write("Starting Windows PCVR. Quit in-game to return to the manager. Cancel stops the game process.");
        var previous=runner.CommandTimeout; runner.CommandTimeout=Timeout.InfiniteTimeSpan;
        log.Write(engine==PcvrMods.Engine ? "Selected Upstate Liberty engine with separate Upstate saves." : "Selected Classic / Xbox engine.");
        try { await runner.Run(Path.Combine(install,engine),arguments,paths.Data,token:token); }
        finally { runner.CommandTimeout=previous; }
        log.Write("Windows PCVR exited successfully.");
    }
}
