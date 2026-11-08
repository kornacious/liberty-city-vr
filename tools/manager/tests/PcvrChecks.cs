using System.Text.Json;
using LibertyManager;

internal static class PcvrChecks
{
    static void Check(bool value) { if (!value) throw new Exception("PCVR assertion failed."); }
    static void Reject(Action action)
    {
        try { action(); } catch (Exception error) when (error is IOException or InvalidDataException or ArgumentException or JsonException) { return; }
        throw new Exception("Unsafe PCVR operation accepted.");
    }
    static void Package(string root,string version)
    {
        foreach (var name in new[] { "libertyvr-game.exe", "libertyvr-pcvr.exe", "openxr_loader.dll", "openxr-notices/LICENSE", "openxr-notices/JsonCpp-LICENSE", "IMGUI_LICENSE.txt", "LIBRW_LICENSE.txt" })
        { var path=Path.Combine(root,"versions",version,name); Directory.CreateDirectory(Path.GetDirectoryName(path)!); File.WriteAllText(path,"synthetic"); }
    }
    public static async Task Run(string root,Func<string,Func<Task>,Task> test)
    {
        var fixture=Path.Combine(root,"pcvr"); Directory.CreateDirectory(fixture);
        var settings=new ManagerSettings { Repository=Path.Combine(fixture,"repository"), Workplace=Path.Combine(fixture,"work"), GameDirectory=Path.Combine(fixture,"retail") };
        Directory.CreateDirectory(Path.Combine(settings.Repository,"patches"));
        Directory.CreateDirectory(Path.Combine(settings.Repository,"overlay/android/app"));
        File.WriteAllText(Path.Combine(settings.Repository,"overlay/android/app/build.gradle.kts"),"versionCode = 805");
        await test("Legacy settings retain Quest defaults and PCVR options remain separate",()=>
        {
            var old=JsonSerializer.Deserialize<ActionOptions>("{\"BuildOnly\":true,\"Release\":true,\"CopyGameData\":true}")!;
            Check(old.Target==GameTarget.Quest && !old.PcvrBuildOnly && !old.PcvrPrepareData);
            old.Target=GameTarget.WindowsPcvr; old.PcvrBuildOnly=true;
            var copy=JsonSerializer.Deserialize<ActionOptions>(JsonSerializer.Serialize(old))!;
            Check(copy.Target==GameTarget.WindowsPcvr && copy.BuildOnly && copy.Release && copy.CopyGameData && copy.PcvrBuildOnly);
            return Task.CompletedTask;
        });
        await test("PCVR directories reject retail/source overlap and shared live saves",()=>
        {
            var paths=PcvrPaths.From(settings); paths.Validate(settings);
            Check(paths.Install!=paths.Data && paths.Data!=paths.User);
            Reject(()=>(paths with { Data=settings.GameDirectory }).Validate(settings));
            Reject(()=>(paths with { User=Path.Combine(paths.Install,"saves") }).Validate(settings));
            Reject(()=>(paths with { Install=settings.Repository }).Validate(settings));
            Reject(()=>PcvrPaths.From(settings with { PcvrDataDirectory="relative" }));
            return Task.CompletedTask;
        });
        await test("Failed PCVR install publication preserves the current version and data",()=>
        {
            var paths=PcvrPaths.From(settings); Directory.CreateDirectory(paths.User);
            File.WriteAllText(Path.Combine(paths.User,"save.fixture"),"unchanged");
            Package(paths.Install,"first"); PcvrWorkflow.PublishInstallation(paths.Install,"versions/first");
            var manifest=File.ReadAllText(Path.Combine(paths.Install,"current.json"));
            Package(paths.Install,"broken"); File.Delete(Path.Combine(paths.Install,"versions/broken/openxr_loader.dll"));
            Reject(()=>PcvrWorkflow.PublishInstallation(paths.Install,"versions/broken"));
            Check(File.ReadAllText(Path.Combine(paths.Install,"current.json"))==manifest);
            Check(PcvrWorkflow.ResolveInstallation(paths.Install).EndsWith("first"));
            Package(paths.Install,"second"); PcvrWorkflow.PublishInstallation(paths.Install,"versions/second");
            Check(PcvrWorkflow.ResolveInstallation(paths.Install).EndsWith("second"));
            Check(File.Exists(Path.Combine(paths.Install,"versions/first/libertyvr-game.exe")));
            Check(File.ReadAllText(Path.Combine(paths.User,"save.fixture"))=="unchanged");
            Reject(()=>PcvrWorkflow.PublishInstallation(paths.Install,"versions/../../outside"));
            return Task.CompletedTask;
        });
        await test("PCVR launch retains exact path arguments and validates frame bounds",()=>
        {
            var paths=new PcvrPaths("unused","data with spaces & literal $()","user with spaces");
            var arguments=PcvrWorkflow.LaunchArguments(paths,45);
            Check(arguments.SequenceEqual(new[] {"--game-data",paths.Data,"--user-data",paths.User,"--frames","45"}));
            Reject(()=>PcvrWorkflow.LaunchArguments(paths,-1)); Reject(()=>PcvrWorkflow.LaunchArguments(paths,1000001));
            Reject(()=>PcvrWorkflow.ValidatePreparedData(Path.Combine(fixture,"unprepared")));
            return Task.CompletedTask;
        });
        await test("PCVR asset refresh refuses a linked destination without modifying its target",()=>
        {
            var source=Path.Combine(fixture,"authored"); var data=Path.Combine(fixture,"linked-data");
            var target=Path.Combine(fixture,"protected-original");
            Directory.CreateDirectory(source); Directory.CreateDirectory(data); Directory.CreateDirectory(target);
            File.WriteAllText(Path.Combine(source,"asset.fixture"),"replacement");
            File.WriteAllText(Path.Combine(target,"asset.fixture"),"original");
            var link=Path.Combine(data,"models");
            if (OperatingSystem.IsWindows())
            {
                // Junctions exercise the same reparse-point guard without requiring
                // administrator symbolic-link privileges on Windows test machines.
                var script="$ErrorActionPreference='Stop'; New-Item -ItemType Junction -Path '"+link.Replace("'","''")+"' -Target '"+target.Replace("'","''")+"' | Out-Null";
                var start=new System.Diagnostics.ProcessStartInfo(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),"WindowsPowerShell/v1.0/powershell.exe"))
                    { UseShellExecute=false, CreateNoWindow=true, RedirectStandardOutput=true, RedirectStandardError=true };
                foreach(var argument in new[] { "-NoProfile","-NonInteractive","-EncodedCommand",Convert.ToBase64String(System.Text.Encoding.Unicode.GetBytes(script)) }) start.ArgumentList.Add(argument);
                using var process=System.Diagnostics.Process.Start(start)!;
                var output=process.StandardOutput.ReadToEndAsync(); var error=process.StandardError.ReadToEndAsync();
                if(!process.WaitForExit(10000)) { process.Kill(true); throw new Exception("Junction fixture timed out."); }
                Task.WhenAll(output,error).GetAwaiter().GetResult(); Check(process.ExitCode==0);
            }
            else Directory.CreateSymbolicLink(link,target);
            Directory.CreateDirectory(Path.Combine(source,"models"));
            File.WriteAllText(Path.Combine(source,"models/asset.fixture"),"replacement");
            Reject(()=>Paths.CopyTree(source,data,default));
            Check(File.ReadAllText(Path.Combine(target,"asset.fixture"))=="original");
            return Task.CompletedTask;
        });
        await test("OpenXR manifest resolves a relative runtime library without selecting a runtime",()=>
        {
            var runtime=Path.Combine(fixture,"runtime"); Directory.CreateDirectory(runtime);
            var dll=Path.Combine(runtime,"runtime.dll"); File.WriteAllText(dll,"synthetic");
            var manifest=Path.Combine(runtime,"active.json");
            File.WriteAllText(manifest,"{\"runtime\":{\"library_path\":\"runtime.dll\"}}");
            var before=File.ReadAllText(manifest);
            Check(PcvrWorkflow.ValidateRuntimeManifest(manifest)==Path.GetFullPath(dll) && File.ReadAllText(manifest)==before);
            File.Delete(dll); Reject(()=>PcvrWorkflow.ValidateRuntimeManifest(manifest));
            File.WriteAllText(manifest,"invalid"); Reject(()=>PcvrWorkflow.ValidateRuntimeManifest(manifest));
            File.WriteAllText(manifest,"{}"); Reject(()=>PcvrWorkflow.ValidateRuntimeManifest(manifest));
            return Task.CompletedTask;
        });
        await test("PCVR diagnostics report an unavailable headset without game or ADB calls",async()=>
        {
            using var log=new PrivateLog(); var commands=new ProbeRunner { MissingHeadset=true };
            try { await PcvrWorkflow.Probe(commands,"diagnostic.exe",log,default); throw new Exception("Expected absent headset failure."); }
            catch(IOException error) { Check(error.Message.Contains("No PCVR headset")); }
            Check(commands.Modes.SequenceEqual(new[] {"--vulkan-probe","--probe"}));
        });
        await test("PCVR diagnostics honor cancellation and distinguish successful negotiation",async()=>
        {
            using var log=new PrivateLog(); var commands=new ProbeRunner();
            await PcvrWorkflow.Probe(commands,"diagnostic.exe",log,default);
            using var cancelled=new CancellationTokenSource(); cancelled.Cancel();
            try { await PcvrWorkflow.Probe(commands,"diagnostic.exe",log,cancelled.Token); throw new Exception("Cancellation ignored."); }
            catch(OperationCanceledException) { }
        });
        await test("Compiler environment lookup respects host variable-name case rules",()=>
        {
            using var log=new PrivateLog(); var runner=new CommandRunner(log);
            runner.EnvironmentOverrides["Path"]="synthetic";
            if(OperatingSystem.IsWindows()) Check(runner.EnvironmentOverrides["PATH"]=="synthetic");
            else Check(!runner.EnvironmentOverrides.ContainsKey("PATH"));
            return Task.CompletedTask;
        });
        await test("PCVR requests cannot enter the Quest updater or an undefined build target",async()=>
        {
            using var log=new PrivateLog(); var workflow=new Workflows(settings,new CommandRunner(log),log);
            try { await workflow.Perform(ManagerAction.Update,new ActionOptions { Target=GameTarget.WindowsPcvr },default); throw new Exception("PCVR entered the Quest updater."); }
            catch(InvalidDataException error) { Check(error.Message.Contains("updater targets Quest")); }
            try { await workflow.Perform(ManagerAction.BuildAndInstall,new ActionOptions { Target=(GameTarget)99 },default); throw new Exception("Undefined target accepted."); }
            catch(InvalidDataException error) { Check(error.Message.Contains("valid game target")); }
        });
    }
    sealed class ProbeRunner : ICommandRunner
    {
        public bool MissingHeadset {get;init;}
        public List<string> Modes {get;}=[];
        public Task<CommandResult> Run(string executable,IEnumerable<string> args,string? workingDirectory=null,bool showOutput=true,bool check=true,CancellationToken token=default,string? input=null)
        {
            token.ThrowIfCancellationRequested(); var mode=args.Single(); Modes.Add(mode);
            return Task.FromResult(new CommandResult(MissingHeadset && mode=="--probe" ? 1 : 0,mode=="--probe" ? "system failed (OpenXR -35)" : "Vulkan multiview available"));
        }
    }
}
