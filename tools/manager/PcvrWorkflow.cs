using System.Runtime.InteropServices;
using System.Text.Json;
using Microsoft.Win32;

namespace LibertyManager;

public sealed record PcvrPaths(string Install, string Data, string User, GameTarget Target = GameTarget.WindowsPcvr)
{
    public static PcvrPaths From(ManagerSettings settings, GameTarget target = GameTarget.WindowsPcvr)
    {
        var linux = target == GameTarget.LinuxPcvr;
        var root = Path.Combine(settings.Workplace, linux ? "linux-pcvr" : "windows-pcvr");
        string Resolve(string value, string name)
        {
            if (string.IsNullOrWhiteSpace(value)) return Path.GetFullPath(Path.Combine(root, name));
            if (!Path.IsPathFullyQualified(value)) throw new InvalidDataException("PCVR directories must be absolute paths.");
            return Path.GetFullPath(value);
        }
        return new(Resolve(linux ? settings.LinuxPcvrInstallDirectory : settings.PcvrInstallDirectory, "app"), Resolve(linux ? settings.LinuxPcvrDataDirectory : settings.PcvrDataDirectory, "data"), Resolve(linux ? settings.LinuxPcvrUserDirectory : settings.PcvrUserDirectory, "user"), target);
    }
    public void Validate(ManagerSettings settings)
    {
        var targets = new[] { Install, Data, User };
        foreach (var path in targets)
        {
            Paths.NoLinks(path);
            foreach (var source in new[] { settings.Repository, settings.GameDirectory,
                         Path.Combine(settings.Workplace, "re3-public-b050e1c"), Path.Combine(settings.Workplace, "windows-pcvr-builds"), Path.Combine(settings.Workplace, "linux-pcvr-builds") }.Where(p => !string.IsNullOrWhiteSpace(p)))
                if (Paths.Within(source, path) || Paths.Within(path, source))
                    throw new InvalidDataException("Keep PCVR install, data and user directories separate from retail data, source trees and build directories.");
        }
        for (var i=0; i<targets.Length; ++i) for (var j=i+1; j<targets.Length; ++j)
            if (Paths.Within(targets[i], targets[j]) || Paths.Within(targets[j], targets[i]))
                throw new InvalidDataException("PCVR install, data and user directories must not overlap.");
        var other=From(settings,Target==GameTarget.LinuxPcvr?GameTarget.WindowsPcvr:GameTarget.LinuxPcvr);
        foreach(var path in targets)foreach(var alternate in new[]{other.Install,other.Data,other.User})
            if(Paths.Within(path,alternate) || Paths.Within(alternate,path))
                throw new InvalidDataException("Keep Windows and Linux PCVR install, data and save roots separate.");
    }
}

public sealed record PcvrInstallation(int Version, string Directory, GameTarget Target = GameTarget.WindowsPcvr);

public sealed class PcvrWorkflow(ManagerSettings settings, CommandRunner runner, PrivateLog log, GameTarget target = GameTarget.WindowsPcvr)
{
    public static GameTarget HostTarget => OperatingSystem.IsLinux() ? GameTarget.LinuxPcvr : GameTarget.WindowsPcvr;
    private bool Linux => target == GameTarget.LinuxPcvr;
    private string Name => Linux ? "Linux PCVR" : "Windows PCVR";
    private string Executable(string name) => name + (Linux ? "" : ".exe");
    private string Prefix => Path.GetFullPath(string.IsNullOrWhiteSpace(Linux ? settings.LinuxPcvrDependencyPrefix : settings.PcvrDependencyPrefix) ? Path.Combine(settings.Repository,".cache","desktop-deps",Linux ? "linux" : "windows") : (Linux ? settings.LinuxPcvrDependencyPrefix : settings.PcvrDependencyPrefix));
    private const string OpenXrArchive = "https://github.com/KhronosGroup/OpenXR-SDK/archive/refs/tags/release-1.1.58.zip";
    private const string OpenXrHash = "43d75ce00e31be8f662b58e8c74af8c04156bcaa7acbfc19112aa2880f3a3669";
    private void RequireHost()
    {
        if (target is not (GameTarget.WindowsPcvr or GameTarget.LinuxPcvr) || RuntimeInformation.ProcessArchitecture != Architecture.X64 || (Linux ? !OperatingSystem.IsLinux() : !OperatingSystem.IsWindows()))
            throw new PlatformNotSupportedException("Build and run PCVR on its matching Windows/Linux x64 host.");
    }
    private PcvrPaths Layout()
    {
        var paths=PcvrPaths.From(settings,target); paths.Validate(settings); return paths;
    }
    public static void ValidatePreparedData(string path)
    {
        Paths.NoLinks(path);
        if (!File.Exists(Path.Combine(path,".libertyvr-pcvr-data")) ||
            (!Directory.Exists(Path.Combine(path,"variants")) && (!Directory.Exists(Path.Combine(path,"data")) || !Directory.Exists(Path.Combine(path,"models")))))
            throw new IOException("No prepared PCVR data directory. Use Build and install to prepare a PC or Original Xbox variant, or choose an existing prepared directory.");
    }
    // Versioned installs avoid replacing a running EXE. Publish the pointer only
    // after CMake installation and all required files have been verified.
    public static string ResolveInstallation(string root, GameTarget target = GameTarget.WindowsPcvr)
    {
        Paths.NoLinks(root);
        var executable = target == GameTarget.LinuxPcvr ? "libertyvr-game" : "libertyvr-game.exe";
        if (File.Exists(Paths.Child(root, executable))) { ValidateBinaries(root,target); return Path.GetFullPath(root); }
        var physical = Paths.Child(root,"current");
        if (Directory.Exists(physical)) { ValidateBinaries(physical,target); return physical; }
        var manifest=Paths.Child(root,"current.json");
        if (!File.Exists(manifest)) throw new IOException("No PCVR build installed. Select Windows PCVR under Build and install first.");
        var current=JsonSerializer.Deserialize<PcvrInstallation>(File.ReadAllText(manifest)) ?? throw new InvalidDataException("Invalid PCVR installation record.");
        if (current.Version is not (1 or 2) || (current.Version==1 && target!=GameTarget.WindowsPcvr) || (current.Version==2 && current.Target!=target) || !current.Directory.StartsWith("versions/",StringComparison.Ordinal)) throw new InvalidDataException("Unsupported PCVR installation record.");
        var directory=Paths.Child(root,current.Directory);
        ValidateBinaries(directory,target);
        return directory;
    }
    public static void ValidateBinaries(string directory, GameTarget target = GameTarget.WindowsPcvr)
    {
        foreach (var name in target==GameTarget.LinuxPcvr ? new[] { "libertyvr-game", "libertyvr-pcvr", "libopenxr_loader.so.1", "IMGUI_LICENSE.txt", "LIBRW_LICENSE.txt", "desktop-backend.txt" } : new[] { "libertyvr-game.exe", "libertyvr-pcvr.exe", "openxr_loader.dll", "openxr-notices/LICENSE", "openxr-notices/JsonCpp-LICENSE", "IMGUI_LICENSE.txt", "LIBRW_LICENSE.txt" })
            if(name=="libopenxr_loader.so.1") ValidateBundledLibrary(directory,Path.Combine(directory,name));
            else if (!File.Exists(Paths.Child(directory,name))) throw new IOException("PCVR installation is incomplete: " + name);
        if(File.Exists(Path.Combine(directory,"desktop-backend.txt"))) {
            foreach(var (component,major) in new[]{("SDL3","0"),("avformat","63"),("avcodec","63"),("avutil","61"),("swresample","7")}) {
                var required=target==GameTarget.LinuxPcvr?"lib"+component+".so."+major:component=="SDL3"?"SDL3.dll":component+"-"+major+".dll";
                ValidateBundledLibrary(directory,Path.Combine(directory,required));
                var pattern=target==GameTarget.LinuxPcvr?"lib"+component+".so*":component=="SDL3"?"SDL3.dll":component+"-*.dll";
                var libraries=Directory.EnumerateFiles(directory,pattern).ToArray();
                if(libraries.Length==0)throw new IOException("PCVR dependency missing: "+component);
                foreach(var library in libraries)ValidateBundledLibrary(directory,library);
            }
            foreach(var name in new[]{"SDL_LICENSE.txt","FFMPEG_LICENSE.txt","config.mak"})
                if(!File.Exists(Paths.Child(directory,"dependency-notices/"+name)))throw new IOException("PCVR dependency notice missing: "+name);
        }
    }
    private static void ValidateBundledLibrary(string directory,string path)
    {
        Paths.NoLinks(directory);var file=new FileInfo(path);
        if(!file.Exists)throw new IOException("PCVR bundled library is missing.");
        var resolved=file.ResolveLinkTarget(true)?.FullName??file.FullName;
        if(!Paths.Within(directory,resolved))throw new InvalidDataException("PCVR library link escapes its installation.");
        Paths.NoLinks(resolved);
    }
    public static void PublishInstallation(string root, string relative, GameTarget target = GameTarget.WindowsPcvr)
    {
        if (!relative.StartsWith("versions/",StringComparison.Ordinal)) throw new InvalidDataException("Installations must use a versioned subdirectory.");
        var directory=Paths.Child(root,relative); ValidateBinaries(directory,target);
        var destination=Paths.Child(root,"current.json");
        var temporary=Paths.Child(root,".current-"+Guid.NewGuid().ToString("N"));
        try
        {
            File.WriteAllText(temporary,JsonSerializer.Serialize(new PcvrInstallation(File.Exists(Path.Combine(directory,"desktop-backend.txt")) ? 2 : 1,relative,target)));
            File.Move(temporary,destination,true);
        }
        finally { if (File.Exists(temporary)) File.Delete(temporary); }
    }
    private async Task<(string Cmake,string Ninja,string Include,string Library,string Shader)> FindTools(CancellationToken token)
    {
        if(Linux) {
            var cm=Paths.FindExecutable(settings.CMake.Length>0?settings.CMake:"cmake") ?? throw new IOException("Install CMake.");
            var nj=Paths.FindExecutable("ninja") ?? throw new IOException("Install Ninja.");
            var linuxShader=Paths.FindExecutable("glslc") ?? throw new IOException("Install glslc/shaderc.");
            if(Paths.FindExecutable("c++") is null)throw new IOException("Install GCC or Clang C++ tools.");
            return (cm,nj,"","",linuxShader);
        }
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
        var configured=Linux?settings.LinuxPcvrOpenXrSource:settings.PcvrOpenXrSource;
        if (!string.IsNullOrWhiteSpace(configured)) source=Path.GetFullPath(configured);
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
    private async Task EnsureDependencies(CancellationToken token)
    {
        bool Available() => Directory.Exists(Path.Combine(Prefix,"include","SDL3")) &&
            Directory.Exists(Path.Combine(Prefix,"include","libavcodec")) &&
            File.Exists(Path.Combine(Prefix,"share","libertyvr-dependencies","config.mak"));
        if (Available()) return;
        if (!Linux || !string.IsNullOrWhiteSpace(settings.LinuxPcvrDependencyPrefix))
            throw new IOException("Desktop dependencies are missing or incomplete at " + Prefix +
                ". Build them with tools/desktop/build-deps.sh CACHE PREFIX linux (Linux) or tools/desktop/build-deps.ps1 (Windows). On Linux, clear the dependency prefix setting to build them automatically at the default location. See overlay/desktop/README.md for prerequisites.");
        var bash=Paths.FindExecutable("bash") ?? throw new IOException("Install Bash to build Linux desktop dependencies.");
        foreach (var command in new[] { "curl", "tar", "sha256sum", "make", "pkg-config", "nasm" })
            if (Paths.FindExecutable(command) is null)
                throw new IOException("Install " + command + " to build Linux desktop dependencies. See overlay/desktop/README.md for prerequisites.");
        log.Write("Building pinned SDL3 and FFmpeg dependencies. The first build downloads verified source archives and can take several minutes. Linux X11, Wayland/EGL and audio development packages must already be installed; see overlay/desktop/README.md.");
        await runner.Run(bash,[Path.Combine(settings.Repository,"tools","desktop","build-deps.sh"),
            Path.Combine(settings.Repository,".cache","desktop-deps"),Prefix,"linux"],settings.Repository,token:token);
        if (!Available()) throw new IOException("Desktop dependency build did not produce a complete prefix at " + Prefix + ". See the preceding build output.");
    }
    public async Task Build(ActionOptions options,CancellationToken token)
    {
        RequireHost(); var paths=Layout();
        if (!options.PcvrBuildOnly)
        {
            VariantInstaller.ValidateSource(settings,options);
            // An application-only installation is allowed before importing assets.
        }
        var found=await FindTools(token);
        await EnsureDependencies(token);
        var tools=new Toolchain(settings,runner,log);
        var sdk=await OpenXrSource(tools,token);
        var source=await tools.Re3(token); var git=await tools.Git(token);
        var id=DateTime.Now.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N")[..6];
        var work=Path.Combine(settings.Workplace,Linux?"linux-pcvr-builds":"windows-pcvr-builds",id);
        var assembled=Path.Combine(work,"source"); var build=Path.Combine(work,"build");
        await Workflows.Assemble(settings.Repository,source,assembled,git,runner,log,token);
        var variants = !options.PcvrBuildOnly ? await VariantInstaller.Prepare(settings,options,Path.Combine(assembled,"gamefiles"),runner,log,token) : new Dictionary<string,string>();
        await runner.Run(found.Cmake,["-S",Path.Combine(assembled,"desktop"),"-B",build,"-G","Ninja",
            "-DCMAKE_BUILD_TYPE=Release","-DCMAKE_MAKE_PROGRAM="+found.Ninja,"-DLIBERTYVR_RE3_ROOT="+assembled,
            "-DLIBERTYVR_DESKTOP_PREFIX="+Prefix,"-DGLSLC="+found.Shader,
            "-DCMAKE_DISABLE_FIND_PACKAGE_OpenXR=TRUE","-DFETCHCONTENT_SOURCE_DIR_OPENXR_SDK="+sdk,
            "-DFETCHCONTENT_FULLY_DISCONNECTED=ON","-DLIBERTYVR_GPU_TESTS="+(options.PcvrGpuTests ? "ON" : "OFF"),
            "-DLIBERTYVR_AUDIO_TESTS="+(options.PcvrAudioTests ? "ON" : "OFF"),..(Linux?Array.Empty<string>():new[]{"-DVulkan_INCLUDE_DIR="+found.Include,"-DVulkan_LIBRARY="+found.Library})],token:token);
        await runner.Run(found.Cmake,["--build",build,"--parallel","8"],token:token);
        var ctest=Path.Combine(Path.GetDirectoryName(found.Cmake)!,Executable("ctest"));
        await runner.Run(ctest,["--test-dir",build,"--output-on-failure"],token:token);
        log.Write(Name+" Release build and selected checks passed: "+build);
        if (options.PcvrBuildOnly) { log.Write("Build only: installation and game-data preparation skipped."); return; }
        Directory.CreateDirectory(paths.Data);
        Directory.CreateDirectory(Path.Combine(paths.Data,"variants"));
        foreach(var (variant,prepared) in variants)VariantInstaller.InstallLocal(paths.Data,variant,prepared,token);
        File.WriteAllText(Path.Combine(paths.Data,".libertyvr-pcvr-data"),"Liberty City VR local Windows data v1\n");
        Workflows.RefreshWindowsPortFiles(settings.Repository,paths.Data,token);
        Directory.CreateDirectory(paths.Install);
        var install=Path.Combine(work,"package");
        await runner.Run(found.Cmake,["--install",build,"--prefix",install],token:token);
        foreach (var notice in new[] { "LICENSE", "NOTICE.md" })
            File.Copy(Path.Combine(settings.Repository,notice),Path.Combine(install,notice),true);
        File.WriteAllText(Path.Combine(install,PcvrBuilds.Identity),id+"\n");
        PcvrBuilds.WritePaths(install,Path.Combine(paths.Install,"current"),paths.Data,paths.User);
        PcvrBuilds.Publish(paths.Install,install,id,paths,token);
        if (options.PcvrDesktopShortcut)
        {
            try { await PcvrShortcuts.Create(paths,token); }
            catch (Exception error)
            { log.Write("Build installed; desktop shortcut was not updated: "+error.Message); }
        }
        log.Write(Name+" installed: "+Path.Combine(paths.Install,"current")+". Previous versions, game data and saves preserved.");
        log.Write("Use Launch PCVR to play, or PCVR diagnostics to check the active runtime.");
    }
    public static string ValidateRuntimeManifest(string path)
    {
        if (!File.Exists(path)) throw new IOException("The selected OpenXR runtime manifest does not exist. Select a PC OpenXR runtime in its own application.");
        try
        {
            // Linux runtime selection commonly links to a vendor manifest whose
            // library path is relative to that manifest's actual location.
            var manifest=new FileInfo(path);
            path=manifest.ResolveLinkTarget(true)?.FullName ?? manifest.FullName;
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
        var manifest=Environment.GetEnvironmentVariable("XR_RUNTIME_JSON");
        if (string.IsNullOrWhiteSpace(manifest) && Linux)
        {
            manifest=FindLinuxRuntimeManifest(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),Environment.GetEnvironmentVariable("XDG_CONFIG_HOME"),Environment.GetEnvironmentVariable("XDG_CONFIG_DIRS"));
        }
        if (string.IsNullOrWhiteSpace(manifest) && OperatingSystem.IsWindows())
        {
            using var registry=RegistryKey.OpenBaseKey(RegistryHive.LocalMachine,RegistryView.Registry64);
            using var key=registry.OpenSubKey(@"SOFTWARE\Khronos\OpenXR\1");
            manifest=key?.GetValue("ActiveRuntime") as string;
        }
        if (string.IsNullOrWhiteSpace(manifest)) throw new IOException("No active PC OpenXR runtime. Select one in Virtual Desktop, SteamVR or your headset software, then connect the headset.");
        ValidateRuntimeManifest(manifest);
        log.Write("Active x64 OpenXR runtime manifest and library found. Runtime selection was not changed.");
    }
    public static string? FindLinuxRuntimeManifest(string home,string? configHome,string? configDirs)
    {
        var user=string.IsNullOrWhiteSpace(configHome)||!Path.IsPathFullyQualified(configHome)?Path.Combine(home,".config"):configHome;
        var system=(string.IsNullOrWhiteSpace(configDirs)?"/etc/xdg":configDirs).Split(Path.PathSeparator,StringSplitOptions.RemoveEmptyEntries).Where(Path.IsPathFullyQualified);
        return new[]{user}.Concat(system).SelectMany(root=>new[]{Path.Combine(root,"openxr/1/active_runtime.x86_64.json"),Path.Combine(root,"openxr/1/active_runtime.json")}).FirstOrDefault(File.Exists);
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
        RequireHost(); var paths=Layout();
        var install=ResolveInstallation(paths.Install,target);
        Runtime();
        await Probe(runner,Path.Combine(install,Executable("libertyvr-pcvr")),log,token);
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
        RequireHost(); var paths=Layout();
        if (!string.IsNullOrWhiteSpace(options.PcvrLaunchApp)) paths=paths with {Install=Path.GetFullPath(options.PcvrLaunchApp)};
        if (!string.IsNullOrWhiteSpace(options.PcvrLaunchData)) paths=paths with {Data=Path.GetFullPath(options.PcvrLaunchData)};
        if (!string.IsNullOrWhiteSpace(options.PcvrLaunchUser)) paths=paths with {User=Path.GetFullPath(options.PcvrLaunchUser)};
        paths.Validate(settings);
        var arguments=LaunchArguments(paths,options.PcvrFrameLimit,options.PcvrQuestProfile);
        ValidatePreparedData(paths.Data); var install=ResolveInstallation(paths.Install,target); Runtime();
        var engine=PcvrMods.SelectEngine(paths);
        await Probe(runner,Path.Combine(install,Executable("libertyvr-pcvr")),log,token);
        Directory.CreateDirectory(paths.User);
        log.Write("Starting desktop PCVR. Quit in-game to return to the manager. Cancel stops the game process.");
        var previous=runner.CommandTimeout; runner.CommandTimeout=Timeout.InfiniteTimeSpan;
        log.Write(engine==PcvrMods.EngineFor(target) ? "Selected Upstate Liberty engine with separate Upstate saves." : "Selected Classic / Xbox engine.");
        try { await runner.Run(Path.Combine(install,Linux ? Path.GetFileNameWithoutExtension(engine) : engine),arguments,paths.Data,token:token); }
        finally { runner.CommandTimeout=previous; }
        log.Write(Name+" exited successfully.");
    }
}
