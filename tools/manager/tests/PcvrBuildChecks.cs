using System.Text.Json;
using LibertyManager;

internal static class PcvrBuildChecks
{
    private static void Check(bool value,[System.Runtime.CompilerServices.CallerArgumentExpression(nameof(value))] string expression="") { if(!value) throw new Exception("PCVR current-build assertion failed: "+expression); }
    private static void Reject(Action action) { try { action(); } catch(IOException) {return;} throw new Exception("Expected PCVR publication rejection."); }
    internal static string Package(string root,string id,GameTarget target=GameTarget.WindowsPcvr)
    {
        var package=Path.Combine(root,"packages",id);
        var names=target==GameTarget.LinuxPcvr
            ? new[]{"libertyvr-game","libertyvr-pcvr","libopenxr_loader.so.1","IMGUI_LICENSE.txt","LIBRW_LICENSE.txt","desktop-backend.txt","libSDL3.so.0","libavformat.so.63","libavcodec.so.63","libavutil.so.61","libswresample.so.7","dependency-notices/SDL_LICENSE.txt","dependency-notices/FFMPEG_LICENSE.txt","dependency-notices/config.mak"}
            : new[]{"libertyvr-game.exe","libertyvr-pcvr.exe","openxr_loader.dll","openxr-notices/LICENSE","openxr-notices/JsonCpp-LICENSE","IMGUI_LICENSE.txt","LIBRW_LICENSE.txt"};
        foreach(var name in names)
        { var file=Path.Combine(package,name); Directory.CreateDirectory(Path.GetDirectoryName(file)!); File.WriteAllText(file,"synthetic"); }
        File.WriteAllText(Path.Combine(package,PcvrBuilds.Identity),id+"\n");
        return package;
    }
    public static async Task Run(string root,Func<string,Func<Task>,Task> test)
    {
        await test("Renderer viewer installation does not require an OpenXR loader or game executable",()=>
        {
            foreach(var target in new[]{GameTarget.WindowsPcvr,GameTarget.LinuxPcvr})
            {
                var directory=Path.Combine(root,"viewer-only-"+target);Directory.CreateDirectory(directory);
                var names=target==GameTarget.LinuxPcvr
                    ? new[]{"libertyvr-render-debug","libSDL3.so.0","libavformat.so.63","libavcodec.so.63","libavutil.so.61","libswresample.so.7"}
                    : new[]{"libertyvr-render-debug.exe","SDL3.dll","avformat-63.dll","avcodec-63.dll","avutil-61.dll","swresample-7.dll"};
                foreach(var name in names.Concat(new[]{"IMGUI_LICENSE.txt","LIBRW_LICENSE.txt","desktop-backend.txt","dependency-notices/SDL_LICENSE.txt","dependency-notices/FFMPEG_LICENSE.txt","dependency-notices/config.mak"}))
                {var file=Path.Combine(directory,name);Directory.CreateDirectory(Path.GetDirectoryName(file)!);File.WriteAllText(file,"synthetic");}
                Check(PcvrWorkflow.ResolveInstallation(directory,target,viewer:true)==Path.GetFullPath(directory));
                Reject(()=>PcvrWorkflow.ResolveInstallation(directory,target));
            }
            return Task.CompletedTask;
        });
        await test("Physical current publication preserves archives, relative paths, saves and legacy builds",()=>
        {
            var fixture=Path.GetFullPath(Path.Combine(root,"physical-current"));
            var paths=new PcvrPaths(Path.Combine(fixture,"app"),Path.Combine(fixture,"data with spaces é"),Path.Combine(fixture,"user"));
            Directory.CreateDirectory(paths.User); File.WriteAllText(Path.Combine(paths.User,"save"),"preserved");
            var legacy=Package(fixture,"legacy");
            File.Delete(Path.Combine(legacy,PcvrBuilds.Identity));
            Directory.CreateDirectory(Path.Combine(paths.Install,"versions"));
            Directory.Move(legacy,Path.Combine(paths.Install,"versions/legacy"));
            PcvrWorkflow.PublishInstallation(paths.Install,"versions/legacy");
            var first=Package(fixture,"first");
            PcvrBuilds.Publish(paths.Install,first,"first",paths,default);
            var current=PcvrWorkflow.ResolveInstallation(paths.Install);
            Check(current==Path.Combine(paths.Install,"current"));
            Check(!File.Exists(Path.Combine(paths.Install,"current.json")));
            Check(PcvrBuilds.ReadPaths(current)==(paths.Data,paths.User));
            Check(!File.ReadAllText(Path.Combine(current,PcvrBuilds.Config)).Contains(fixture));
            var broken=Package(fixture,"broken"); File.Delete(Path.Combine(broken,"openxr_loader.dll"));
            Reject(()=>PcvrBuilds.Publish(paths.Install,broken,"broken",paths,default));
            Check(PcvrBuilds.ReadId(current)=="first");
            using var cancel=new CancellationTokenSource(); cancel.Cancel();
            try {PcvrBuilds.Publish(paths.Install,first,"first",paths,cancel.Token); throw new Exception("Ignored cancellation");} catch(OperationCanceledException) {}
            var second=Package(fixture,"second"); PcvrBuilds.Publish(paths.Install,second,"second",paths,default);
            var archived=Path.Combine(paths.Install,"versions/first");
            Check(PcvrBuilds.ReadPaths(archived)==(paths.Data,paths.User));
            Check(File.ReadAllText(Path.Combine(paths.User,"save"))=="preserved");
            var builds=PcvrBuilds.Discover(paths);
            Check(builds.Count==3 && builds[0].Label=="Current — second" && builds.All(b=>b.Error is null));
            Directory.CreateDirectory(Path.Combine(paths.Install,"versions/second"));
            Reject(()=>PcvrBuilds.Publish(paths.Install,Package(fixture,"third"),"third",paths,default));
            Check(PcvrBuilds.ReadId(current)=="second");
            Directory.Delete(Path.Combine(paths.Install,"versions/second"));
            // Reproduce a power interruption after moving current, before promotion.
            var previous=File.ReadAllText(Path.Combine(current,PcvrBuilds.Config));
            Directory.Move(current,Path.Combine(paths.Install,"versions/second"));
            Directory.CreateDirectory(Path.Combine(paths.Install,".incoming-interrupted"));
            File.WriteAllText(Path.Combine(paths.Install,".pcvr-publication.json"),JsonSerializer.Serialize(new {Incoming=".incoming-interrupted",Id="interrupted",Archive="second",PreviousConfig=previous}));
            PcvrBuilds.Publish(paths.Install,Package(fixture,"fourth"),"fourth",paths,default);
            Check(PcvrBuilds.ReadId(current)=="fourth" && PcvrBuilds.ReadPaths(Path.Combine(paths.Install,"versions/second"))==(paths.Data,paths.User));
            // Relocating the whole workspace keeps both current and archived paths local.
            var moved=fixture+"-moved"; Directory.Move(fixture,moved);
            Check(PcvrBuilds.ReadPaths(Path.Combine(moved,"app/current")).Data==Path.Combine(moved,"data with spaces é"));
            Check(PcvrBuilds.ReadPaths(Path.Combine(moved,"app/versions/first")).User==Path.Combine(moved,"user"));
            File.Delete(Path.Combine(moved,"app/versions/first",PcvrBuilds.Config));
            var relocated=paths with {Install=Path.Combine(moved,"app"),Data=Path.Combine(moved,"data with spaces é"),User=Path.Combine(moved,"user")};
            Check(PcvrBuilds.Discover(relocated).Single(b=>b.Directory.EndsWith("first")).Error is not null);
            return Task.CompletedTask;
        });
        await test("Desktop shortcuts target current and Linux Exec escapes literal paths",async()=>
        {
            var fixture=Path.Combine(root,"shortcut fixture"); var paths=new PcvrPaths(Path.Combine(fixture,"app"),"unused","unused",PcvrWorkflow.HostTarget);
            var desktop=Path.Combine(fixture,"Desktop");
            await PcvrShortcuts.Create(paths,default,desktop);
            Check(File.Exists(Path.Combine(desktop,OperatingSystem.IsWindows()?"Liberty City VR.lnk":"Liberty City VR.desktop")));
            var entry=PcvrShortcuts.LinuxEntry("/tmp/a $b %c/libertyvr-game");
            Check(entry.Contains("%%c") && entry.Contains("\\\\$b") && !entry.Contains("--game-data"));
        });
        await test("Linux physical packages retain target identity, paths and bundled library links",()=>
        {
            var fixture=Path.GetFullPath(Path.Combine(root,"linux-current"));
            var paths=new PcvrPaths(Path.Combine(fixture,"app"),Path.Combine(fixture,"data"),Path.Combine(fixture,"user"),GameTarget.LinuxPcvr);
            var package=Package(fixture,"linux-first",GameTarget.LinuxPcvr);
            if(OperatingSystem.IsLinux())
            {
                var library=Path.Combine(package,"libavcodec.so.63");
                File.Move(library,library+".1"); File.CreateSymbolicLink(library,"libavcodec.so.63.1");
                File.SetUnixFileMode(Path.Combine(package,"libertyvr-game"),UnixFileMode.UserRead|UnixFileMode.UserExecute);
            }
            PcvrBuilds.Publish(paths.Install,package,"linux-first",paths,default);
            var current=PcvrWorkflow.ResolveInstallation(paths.Install,GameTarget.LinuxPcvr);
            Check(PcvrBuilds.ReadPaths(current)==(paths.Data,paths.User));
            Reject(()=>PcvrWorkflow.ResolveInstallation(paths.Install));
            if(OperatingSystem.IsLinux())
            {
                Check(new FileInfo(Path.Combine(current,"libavcodec.so.63")).LinkTarget=="libavcodec.so.63.1");
                Check(File.GetUnixFileMode(Path.Combine(current,"libertyvr-game")).HasFlag(UnixFileMode.UserExecute));
            }
            PcvrBuilds.Publish(paths.Install,Package(fixture,"linux-second",GameTarget.LinuxPcvr),"linux-second",paths,default);
            Check(PcvrBuilds.ReadPaths(Path.Combine(paths.Install,"versions/linux-first"))==(paths.Data,paths.User));
            return Task.CompletedTask;
        });
        await test("Cross-volume staging and copy failures preserve published directories",()=>
        {
            var fixture=Path.Combine(Path.GetTempPath(),"pcvr-copy-"+Guid.NewGuid().ToString("N"));
            var paths=new PcvrPaths(Path.Combine(fixture,"app"),Path.Combine(fixture,"data"),Path.Combine(fixture,"user"));
            try
            {
                var package=Package(root,"cross-volume");
                PcvrBuilds.Publish(paths.Install,package,"cross-volume",paths,default);
                if(OperatingSystem.IsWindows())
                {
                    var next=Package(root,"locked-copy");
                    using var locked=File.Open(Path.Combine(next,"libertyvr-game.exe"),FileMode.Open,FileAccess.Read,FileShare.None);
                    Reject(()=>PcvrBuilds.Publish(paths.Install,next,"locked-copy",paths,default));
                }
                Check(PcvrBuilds.ReadId(Path.Combine(paths.Install,"current"))=="cross-volume");
                Check(!Directory.Exists(Path.Combine(paths.Install,"versions")));
            }
            finally {if(Directory.Exists(fixture))Directory.Delete(fixture,true);}
            return Task.CompletedTask;
        });
    }
}
