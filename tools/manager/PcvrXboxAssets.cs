using System.Diagnostics;

namespace LibertyManager;

public static class PcvrXboxAssets
{
    public static void ValidateOutput(ManagerSettings settings,string output)
    {
        var paths=PcvrPaths.From(settings);paths.Validate(settings);
        foreach(var target in new[]{paths.Data,paths.User,paths.Install})
            if(Paths.Within(target,output) || Paths.Within(output,target))
                throw new IOException("Prepare Xbox assets outside the live PCVR data, saves and installation directories.");
    }
    public static void Install(ManagerSettings settings,string output,CancellationToken token)
    {
        ValidateOutput(settings,output);
        var paths=PcvrPaths.From(settings);PcvrWorkflow.ValidatePreparedData(paths.Data);
        var hashes=OriginalXboxModels.ValidateOverlay(output);
        if(OperatingSystem.IsWindows()) {
            var processes=Process.GetProcessesByName("libertyvr-game");
            try {if(processes.Length>0)throw new IOException("Quit PCVR before installing Xbox assets.");}
            finally {foreach(var process in processes)process.Dispose();}
        }
        InstallFiles(paths.Data,output,hashes,token,verifyOverlay:true);
    }
    // Hash-verified staging, then same-volume directory renames. Old overlays
    // remain as backups; templates, personal placements and saves are untouched.
    public static string? InstallFiles(string data,string source,IReadOnlyDictionary<string,string> hashes,CancellationToken token,bool verifyOverlay=false)
    {
        Paths.NoLinks(data);Paths.NoLinks(source);
        var root=Paths.Child(data,"modelsets");Directory.CreateDirectory(root);
        var destination=Paths.Child(root,"modern");
        if(Directory.Exists(destination) && !File.Exists(Paths.Child(destination,"BUILD_INFO.txt")))
            throw new IOException("Refusing to replace an unmarked modelset directory.");
        var stamp=Guid.NewGuid().ToString("N");
        var stagingRoot=Paths.Child(data,".libertyvr-modelset-staging");Directory.CreateDirectory(stagingRoot);
        var backupRoot=Paths.Child(data,".libertyvr-modelset-backups");Directory.CreateDirectory(backupRoot);
        var stage=Paths.Child(stagingRoot,"modern-"+stamp);
        var backup=Paths.Child(backupRoot,"modern-"+stamp);
        Directory.CreateDirectory(stage);
        foreach(var (relative,hash) in hashes.Append(new KeyValuePair<string,string>("BUILD_INFO.txt",Digests.Sha256(Paths.Child(source,"BUILD_INFO.txt"))))) {
            token.ThrowIfCancellationRequested();var target=Paths.Child(stage,relative);
            Directory.CreateDirectory(Path.GetDirectoryName(target)!);File.Copy(Paths.Child(source,relative),target);
            if(!Digests.Sha256(target).Equals(hash,StringComparison.OrdinalIgnoreCase))
                throw new IOException("Xbox staging verification failed; existing assets preserved. Partial staging retained locally.");
        }
        if(verifyOverlay)OriginalXboxModels.ValidateOverlay(stage);
        token.ThrowIfCancellationRequested();var existed=Directory.Exists(destination);
        Paths.NoLinks(destination);
        if(existed)Directory.Move(destination,backup);
        try {Directory.Move(stage,destination);}
        catch {if(existed && !Directory.Exists(destination))Directory.Move(backup,destination);throw;}
        return existed?backup:null;
    }
}
