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
    public static string? SetupBundle(string repository, GameTarget target)
    {
        var folder = target switch
        {
            GameTarget.WindowsPcvr => "win-x64",
            GameTarget.LinuxPcvr => "linux-x64",
            _ => null
        };
        if (folder is null) return null;
        var bundle = Path.Combine(repository, "PCVR", folder);
        return Directory.Exists(bundle) ? bundle : null;
    }

    internal static string ValidateSetupBundle(string bundle, GameTarget target)
    {
        var app = Paths.Child(bundle, "app");
        ValidateBinaries(app, target);
        var converter = Paths.Child(bundle, target == GameTarget.WindowsPcvr ? "xbox-convert.exe" : "xbox-convert");
        Paths.NoLinks(converter);
        if (!File.Exists(converter)) throw new IOException("PCVR package is missing the Original Xbox converter. Extract the complete Setup kit again.");
        return converter;
    }
    private bool Linux => target == GameTarget.LinuxPcvr;
    private string Name => Linux ? "Linux PCVR" : "Windows PCVR";
    private string Executable(string name) => name + (Linux ? "" : ".exe");
    private string ConfiguredPrefix => Linux ? settings.LinuxPcvrDependencyPrefix : settings.PcvrDependencyPrefix;
    private string LocalPrefix => Path.GetFullPath(string.IsNullOrWhiteSpace(ConfiguredPrefix)
        ? Path.Combine(settings.Repository, ".cache", "desktop-deps", Linux ? "linux" : "windows") : ConfiguredPrefix);
    private string Prefix => resolvedDependencyPrefix ?? LocalPrefix;
    private string? resolvedDependencyPrefix;
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
    public static string ResolveInstallation(string root, GameTarget target = GameTarget.WindowsPcvr, bool viewer = false)
    {
        Paths.NoLinks(root);
        var executable = (viewer ? "libertyvr-render-debug" : "libertyvr-game") + (target == GameTarget.LinuxPcvr ? "" : ".exe");
        if (File.Exists(Paths.Child(root, executable))) { ValidateBinaries(root,target,viewer); return Path.GetFullPath(root); }
        var physical = Paths.Child(root,"current");
        if (Directory.Exists(physical)) { ValidateBinaries(physical,target,viewer); return physical; }
        var manifest=Paths.Child(root,"current.json");
        if (!File.Exists(manifest)) throw new IOException("No PCVR build installed. Select Windows PCVR under Build and install first.");
        var current=JsonSerializer.Deserialize<PcvrInstallation>(File.ReadAllText(manifest)) ?? throw new InvalidDataException("Invalid PCVR installation record.");
        if (current.Version is not (1 or 2) || (current.Version==1 && target!=GameTarget.WindowsPcvr) || (current.Version==2 && current.Target!=target) || !current.Directory.StartsWith("versions/",StringComparison.Ordinal)) throw new InvalidDataException("Unsupported PCVR installation record.");
        var directory=Paths.Child(root,current.Directory);
        ValidateBinaries(directory,target,viewer);
        return directory;
    }
    public static void ValidateBinaries(string directory, GameTarget target = GameTarget.WindowsPcvr, bool viewer = false, bool portable = false)
    {
        var requiredFiles=viewer
            ? new[] {target==GameTarget.LinuxPcvr?"libertyvr-render-debug":"libertyvr-render-debug.exe","IMGUI_LICENSE.txt","LIBRW_LICENSE.txt","desktop-backend.txt"}
            : target==GameTarget.LinuxPcvr ? new[] { "libertyvr-game", "libertyvr-pcvr", "libopenxr_loader.so.1", "IMGUI_LICENSE.txt", "LIBRW_LICENSE.txt", "desktop-backend.txt" } : new[] { "libertyvr-game.exe", "libertyvr-pcvr.exe", "openxr_loader.dll", "openxr-notices/LICENSE", "openxr-notices/JsonCpp-LICENSE", "IMGUI_LICENSE.txt", "LIBRW_LICENSE.txt" };
        foreach (var name in requiredFiles)
            if (portable && name is "libertyvr-pcvr" or "libertyvr-pcvr.exe") continue;
            else if(name=="libopenxr_loader.so.1") ValidateBundledLibrary(directory,Path.Combine(directory,name));
            else if (!File.Exists(Paths.Child(directory,name))) throw new IOException("PCVR installation is incomplete: " + name);
        if (portable)
            foreach (var mode in new[] { "classic", "upstate" })
                if (!File.Exists(Paths.Child(directory, PcvrMods.PortableEngine(target, mode))))
                    throw new IOException("The portable " + mode + " engine library is missing.");
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
        var tools = new Toolchain(settings, runner, log);
        if (tools.FindCompilerTool("cl") is null) throw new IOException("Install Visual Studio C++ x64 build tools.");
        var cmake=string.IsNullOrWhiteSpace(settings.CMake) ? tools.FindCompilerTool("cmake") : Paths.FindExecutable(settings.CMake);
        if (cmake is null) throw new IOException("Install CMake and configure its executable in Settings.");
        var ninja = tools.FindNinja(cmake);
        if (ninja is null) throw new IOException("Install Ninja (included with Visual Studio CMake tools) and add it to PATH.");
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
    internal async Task<string> BuildPortablePackage(string package, CancellationToken token, bool runChecks = true)
    {
        RequireHost();
        var found = await FindTools(token);
        await EnsureDependencies(token);
        var tools = new Toolchain(settings, runner, log);
        var sdk = await OpenXrSource(tools, token);
        var source = await tools.Re3(token);
        var assembled = Paths.Child(settings.Workplace, "portable-source");
        var build = Paths.Child(settings.Workplace, "portable-build");
        await Workflows.Assemble(settings.Repository, source, assembled, await tools.Git(token), runner, log, token);
        await runner.Run(found.Cmake, ["-S", Path.Combine(assembled, "desktop"), "-B", build, "-G", "Ninja",
            "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_MAKE_PROGRAM=" + found.Ninja,
            "-DLIBERTYVR_RE3_ROOT=" + assembled, "-DLIBERTYVR_DESKTOP_PREFIX=" + Prefix, "-DGLSLC=" + found.Shader,
            "-DCMAKE_DISABLE_FIND_PACKAGE_OpenXR=TRUE", "-DFETCHCONTENT_SOURCE_DIR_OPENXR_SDK=" + sdk,
            "-DFETCHCONTENT_FULLY_DISCONNECTED=ON", "-DLIBERTYVR_PORTABLE_SETUP=ON", "-DBUILD_TESTING=" + (runChecks ? "ON" : "OFF"),
            .. (Linux ? Array.Empty<string>() : new[] { "-DVulkan_INCLUDE_DIR=" + found.Include, "-DVulkan_LIBRARY=" + found.Library })], token: token);
        await runner.Run(found.Cmake, ["--build", build, "--parallel", "8"], token: token);
        if (runChecks)
            await runner.Run(Path.Combine(Path.GetDirectoryName(found.Cmake)!, Executable("ctest")),
                ["--test-dir", build, "--output-on-failure"], token: token);
        await runner.Run(found.Cmake, ["--install", build, "--prefix", package], token: token);
        if (!Linux)
        {
            var redist = runner.EnvironmentOverrides.GetValueOrDefault("VCToolsRedistDir")
                ?? Environment.GetEnvironmentVariable("VCToolsRedistDir");
            var x64 = string.IsNullOrWhiteSpace(redist) ? "" : Path.Combine(redist, "x64");
            var crt = Directory.Exists(x64)
                ? Directory.EnumerateDirectories(x64, "Microsoft.VC*.CRT").SingleOrDefault() : null;
            if (crt is null) throw new IOException("Install the Visual Studio x64 C++ redistributable files, then retry.");
            foreach (var library in Directory.EnumerateFiles(crt, "*.dll"))
                File.Copy(library, Paths.Child(package, Path.GetFileName(library)), overwrite: true);
        }
        return Path.Combine(source, "gamefiles");
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
    internal static bool HasDependencies(string prefix, bool linux)
    {
        string[] required = ["include/SDL3/SDL.h", "include/libavcodec/avcodec.h", "include/libavformat/avformat.h",
            "include/libavutil/avutil.h", "include/libswresample/swresample.h",
            "share/libertyvr-dependencies/config.mak", "share/libertyvr-dependencies/SDL_LICENSE.txt",
            "share/libertyvr-dependencies/FFMPEG_LICENSE.txt"];
        if (required.Any(file => !File.Exists(Path.Combine(prefix, file)))) return false;
        foreach (var (name, major) in new[] { ("SDL3", "0"), ("avcodec", "63"), ("avformat", "63"), ("avutil", "61"), ("swresample", "7") })
        {
            if (linux)
            {
                var link = Path.Combine(prefix, "lib", "lib" + name + ".so");
                var runtime = link + "." + major;
                if (!File.Exists(link) || !File.Exists(runtime)) return false;
                ValidateBundledLibrary(prefix, link);
                ValidateBundledLibrary(prefix, runtime);
            }
            else
            {
                var library = "lib/" + name + ".lib";
                var runtime = name == "SDL3" ? "SDL3.dll" : name + "-" + major + ".dll";
                if ((!File.Exists(Paths.Child(prefix, library)) && !File.Exists(Paths.Child(prefix, "bin/" + name + ".lib"))) ||
                    !File.Exists(Paths.Child(prefix, "bin/" + runtime))) return false;
            }
        }
        return true;
    }

    private sealed record DependencyBundle(string Url, string Sha256);
    private DependencyBundle? PinnedDependencies()
    {
        if (!string.IsNullOrWhiteSpace(ConfiguredPrefix)) return null;
        var manifest = Paths.Child(settings.Repository, $"Setup/tools/{SetupKitTools.HostRid}/desktop-deps.json");
        if (!File.Exists(manifest)) return null;
        var bundle = JsonSerializer.Deserialize<DependencyBundle>(File.ReadAllText(manifest));
        var extension = Linux ? ".tar.gz" : ".zip";
        if (bundle is null || !Uri.TryCreate(bundle.Url, UriKind.Absolute, out var uri) ||
            uri.Scheme != "https" || uri.Host != "github.com" ||
            !uri.AbsolutePath.StartsWith("/kornacious/liberty-city-vr/releases/download/", StringComparison.Ordinal) ||
            !uri.AbsolutePath.EndsWith(extension, StringComparison.Ordinal) ||
            bundle.Sha256 is not { Length: 64 } || !bundle.Sha256.All(Uri.IsHexDigit))
            throw new InvalidDataException("Invalid pinned desktop dependency bundle in the Setup kit.");
        return bundle;
    }

    private string BundlePrefix(DependencyBundle bundle) => Paths.Child(settings.Workplace, ".tools/desktop-deps-" + bundle.Sha256.ToLowerInvariant());
    private string BundleArchive(DependencyBundle bundle) => "desktop-deps-" + bundle.Sha256.ToLowerInvariant() + (Linux ? ".tar.gz" : ".zip");

    private bool CheckDependencyBundle(DependencyBundle bundle)
    {
        if (HasDependencies(BundlePrefix(bundle), Linux)) return true;
        if (!settings.AllowToolDownloads && !File.Exists(Paths.Child(settings.Workplace, ".downloads/" + BundleArchive(bundle))))
            throw new IOException("Choose a complete desktop dependency prefix or allow verified tool downloads in Setup help.");
        if (Linux && Paths.FindExecutable("tar") is null)
            throw new IOException("Install tar to unpack the desktop libraries.");
        return false;
    }

    private bool CanBuildDependencies => Linux && string.IsNullOrWhiteSpace(settings.LinuxPcvrDependencyPrefix);

    private static readonly string[] DependencyTools = ["curl", "tar", "sha256sum", "make", "pkg-config", "nasm"];

    internal async Task<bool> CheckSetupTools(CancellationToken token)
    {
        RequireHost();
        var tools = await FindTools(token);
        if (!File.Exists(Path.Combine(Path.GetDirectoryName(tools.Cmake)!, Executable("ctest"))))
            throw new IOException("Install CMake with CTest.");
        if (HasDependencies(Prefix, Linux)) return true;
        if (PinnedDependencies() is { } bundle) return CheckDependencyBundle(bundle);
        if (!CanBuildDependencies)
            throw new IOException("Prepare the desktop libraries using the PCVR build guide, then check again.");
        var missing = DependencyTools.Prepend("bash").Where(name => Paths.FindExecutable(name) is null).ToArray();
        if (missing.Length > 0)
            throw new IOException("Install these Linux build tools: " + string.Join(", ", missing) + ".");
        return false;
    }

    private async Task EnsureDependencies(CancellationToken token)
    {
        if (HasDependencies(Prefix, Linux)) return;
        if (PinnedDependencies() is { } bundle)
        {
            var prefix = BundlePrefix(bundle);
            if (!CheckDependencyBundle(bundle))
            {
                var tools = new Toolchain(settings, runner, log);
                var archive = await tools.Download(bundle.Url, BundleArchive(bundle), bundle.Sha256, token);
                var stage = Paths.Child(settings.Workplace, ".tools/.desktop-deps-" + Guid.NewGuid().ToString("N"));
                Directory.CreateDirectory(stage);
                try
                {
                    if (Linux) await runner.Run(Paths.FindExecutable("tar")!, ["-xzf", archive, "-C", stage], token: token);
                    else Toolchain.ExtractZip(archive, stage, token);
                    if (!HasDependencies(stage, Linux)) throw new InvalidDataException("The desktop dependency bundle is incomplete.");
                    token.ThrowIfCancellationRequested();
                    if (Directory.Exists(prefix))
                        throw new IOException("The cached desktop dependency prefix is incomplete. Move it aside, then retry: " + prefix);
                    Directory.Move(stage, prefix);
                }
                finally { if (Directory.Exists(stage)) Directory.Delete(stage, recursive: true); }
            }
            resolvedDependencyPrefix = prefix;
            return;
        }
        if (!CanBuildDependencies)
            throw new IOException("Desktop dependencies are missing or incomplete at " + Prefix +
                ". Build them with tools/desktop/build-deps.sh CACHE PREFIX linux (Linux) or tools/desktop/build-deps.ps1 (Windows). On Linux, clear the dependency prefix setting to build them automatically at the default location. See overlay/desktop/README.md for prerequisites.");
        var bash=Paths.FindExecutable("bash") ?? throw new IOException("Install Bash to build Linux desktop dependencies.");
        foreach (var command in DependencyTools)
            if (Paths.FindExecutable(command) is null)
                throw new IOException("Install " + command + " to build Linux desktop dependencies. See overlay/desktop/README.md for prerequisites.");
        log.Write("Building pinned SDL3 and FFmpeg dependencies. The first build downloads verified source archives and can take several minutes. Linux X11, Wayland/EGL and audio development packages must already be installed; see overlay/desktop/README.md.");
        await runner.Run(bash,[Path.Combine(settings.Repository,"tools","desktop","build-deps.sh"),
            Path.Combine(settings.Repository,".cache","desktop-deps"),Prefix,"linux"],settings.Repository,token:token);
        if (!HasDependencies(Prefix, Linux)) throw new IOException("Desktop dependency build did not produce a complete prefix at " + Prefix + ". See the preceding build output.");
    }
    public Task Build(ActionOptions options, CancellationToken token) => Build(options, token, null);

    internal async Task Build(ActionOptions options, CancellationToken token, InstallRun? progress, string? setupBundle = null,
        string? xboxConverter = null, string? preparedUpstate = null)
    {
        RequireHost(); var paths=Layout();
        if (preparedUpstate is not null) PcvrMods.RequireStopped();
        if (!options.PcvrBuildOnly)
        {
            VariantInstaller.ValidateSource(settings,options);
            // An application-only installation is allowed before importing assets.
        }
        var converter = xboxConverter;
        if (setupBundle is not null)
        {
            ValidateBinaries(Paths.Child(setupBundle, "app"), target);
            if (options.InstallXboxVariant && converter is null) converter = ValidateSetupBundle(setupBundle, target);
        }
        var found = setupBundle is null ? await FindTools(token) : default;
        if (setupBundle is null) await EnsureDependencies(token);
        var tools=new Toolchain(settings,runner,log);
        var sdk = setupBundle is null ? await OpenXrSource(tools,token) : "";
        var source=await tools.Re3(token);
        var id=DateTime.Now.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N")[..6];
        var work=Path.Combine(settings.Workplace,Linux?"linux-pcvr-builds":"windows-pcvr-builds",id);
        var assembled=Path.Combine(work,"source"); var build=Path.Combine(work,"build");
        progress?.Report(InstallStage.PreparingGameFiles);
        if (setupBundle is null)
            await Workflows.Assemble(settings.Repository,source,assembled,await tools.Git(token),runner,log,token);
        var portFiles = Path.Combine(setupBundle is null ? assembled : source, "gamefiles");
        var variants = !options.PcvrBuildOnly ? await VariantInstaller.Prepare(settings,options,portFiles,runner,log,token,xboxConverter:converter) : new Dictionary<string,string>();
        if (setupBundle is null)
        {
            progress?.Report(InstallStage.BuildingGame);
            await runner.Run(found.Cmake,["-S",Path.Combine(assembled,"desktop"),"-B",build,"-G","Ninja",
                "-DCMAKE_BUILD_TYPE=Release","-DCMAKE_MAKE_PROGRAM="+found.Ninja,"-DLIBERTYVR_RE3_ROOT="+assembled,
                "-DLIBERTYVR_DESKTOP_PREFIX="+Prefix,"-DGLSLC="+found.Shader,
                "-DCMAKE_DISABLE_FIND_PACKAGE_OpenXR=TRUE","-DFETCHCONTENT_SOURCE_DIR_OPENXR_SDK="+sdk,
                "-DFETCHCONTENT_FULLY_DISCONNECTED=ON","-DLIBERTYVR_GPU_TESTS="+(options.PcvrGpuTests ? "ON" : "OFF"),
                "-DLIBERTYVR_AUDIO_TESTS="+(options.PcvrAudioTests ? "ON" : "OFF"),..(Linux?Array.Empty<string>():new[]{"-DVulkan_INCLUDE_DIR="+found.Include,"-DVulkan_LIBRARY="+found.Library})],token:token);
            await runner.Run(found.Cmake,["--build",build,"--parallel","8"],token:token);
            var ctest=Path.Combine(Path.GetDirectoryName(found.Cmake)!,Executable("ctest"));
            await runner.Run(ctest,["--test-dir",build,"--output-on-failure"],token:token);
            if (progress is not null) progress.Built = true;
            log.Write(Name+" Release build and selected checks passed: "+build);
        }
        if (options.PcvrBuildOnly) { log.Write("Build only: installation and game-data preparation skipped."); return; }
        Directory.CreateDirectory(paths.Data);
        Directory.CreateDirectory(Path.Combine(paths.Data,"variants"));
        var installedVariants = 0;
        foreach (var (variant, prepared) in variants)
        {
            progress?.Report(InstallStage.InstallingGameFiles, installedVariants, variants.Count);
            VariantInstaller.InstallLocal(paths.Data, variant, prepared, token);
            progress?.Report(InstallStage.InstallingGameFiles, ++installedVariants, variants.Count);
        }
        File.WriteAllText(Path.Combine(paths.Data,".libertyvr-pcvr-data"),"Liberty City VR local Windows data v1\n");
        Workflows.RefreshWindowsPortFiles(settings.Repository,paths.Data,token);
        Directory.CreateDirectory(paths.Install);
        var install=Path.Combine(work,"package");
        progress?.Report(InstallStage.InstallingApplication);
        if (setupBundle is null)
            await runner.Run(found.Cmake,["--install",build,"--prefix",install],token:token);
        else
            PcvrBuilds.CopyPackage(Paths.Child(setupBundle, "app"), install, token);
        foreach (var notice in new[] { "LICENSE", "NOTICE.md" })
            File.Copy(Path.Combine(settings.Repository,notice),Path.Combine(install,notice),true);
        File.WriteAllText(Path.Combine(install,PcvrBuilds.Identity),id+"\n");
        PcvrBuilds.WritePaths(install,Path.Combine(paths.Install,"current"),paths.Data,paths.User);
        if (preparedUpstate is not null && !PcvrMods.Supports(install, target, CuratedMods.Upstate))
            throw new IOException("The PCVR build does not include Upstate Liberty runtime support.");
        PcvrBuilds.Publish(paths.Install,install,id,paths,token);
        if (preparedUpstate is not null)
        {
            progress?.Report(InstallStage.InstallingGameFiles);
            var status = PcvrMods.Install(paths, CuratedMods.Upstate, preparedUpstate, token);
            if (!status.Installed || !status.Enabled || !status.Supported)
                throw new IOException("Setup could not verify Upstate Liberty activation on PCVR.");
        }
        if (progress is not null)
        {
            progress.Installed = true;
            progress.OutputPath = Path.Combine(paths.Install, "current");
        }
        if (options.PcvrDesktopShortcut)
        {
            try { await PcvrShortcuts.Create(paths,token); }
            catch (Exception error)
            {
                if (progress is not null) progress.ShortcutFailed = true;
                log.Write("Build installed; desktop shortcut was not updated: " + error.Message);
            }
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
    public async Task LaunchRenderDebug(ActionOptions options,CancellationToken token)
    {
        RequireHost();var paths=Layout();
        if(!string.IsNullOrWhiteSpace(options.PcvrLaunchApp))paths=paths with {Install=Path.GetFullPath(options.PcvrLaunchApp)};
        if(!string.IsNullOrWhiteSpace(options.PcvrLaunchData))paths=paths with {Data=Path.GetFullPath(options.PcvrLaunchData)};
        if(!string.IsNullOrWhiteSpace(options.PcvrLaunchUser))paths=paths with {User=Path.GetFullPath(options.PcvrLaunchUser)};
        paths.Validate(settings);ValidatePreparedData(paths.Data);var install=ResolveInstallation(paths.Install,target,viewer:true);
        var executable=Path.Combine(install,Executable("libertyvr-render-debug"));
        if(!File.Exists(executable))throw new IOException("This installation predates the renderer viewer. Build and install the desktop target first.");
        var arguments=LaunchArguments(paths,options.PcvrFrameLimit,options.PcvrQuestProfile).ToList();
        if(!string.IsNullOrWhiteSpace(options.RenderDebugScenario)) {Paths.NoLinks(options.RenderDebugScenario);if(!File.Exists(options.RenderDebugScenario))throw new IOException("Scenario file is missing.");arguments.AddRange(["--scenario",Path.GetFullPath(options.RenderDebugScenario)]);}
        if(!string.IsNullOrWhiteSpace(options.RenderDebugOutput)) {Paths.NoLinks(options.RenderDebugOutput);arguments.AddRange(["--output",Path.GetFullPath(options.RenderDebugOutput)]);}
        log.Write("Starting renderer debug viewer. No runtime probe or headset connection is required.");
        var previous=runner.CommandTimeout;runner.CommandTimeout=Timeout.InfiniteTimeSpan;
        try {await runner.Run(executable,arguments,paths.Data,token:token);}finally {runner.CommandTimeout=previous;}
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
