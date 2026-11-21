using System.Text;
using System.Text.Json;

namespace LibertyManager;

public static class VariantInstaller
{
    public const string Marker = "VARIANT_INFO.txt";
    public static bool WantsPc(ActionOptions o) => o.InstallPcVariant || o.CopyGameData || o.PcvrPrepareData;
    public static bool WantsAny(ActionOptions o) => WantsPc(o) || o.InstallXboxVariant;
    public static string Identity(string id) => id == "pc" ? "LCVR_VARIANT_PC_1" : id == "xbox" ? "LCVR_VARIANT_XBOX_1" : throw new InvalidDataException("Unknown variant.");
    public static readonly string[] Required = ["data/main.scm", "data/default.dat", "data/gta3.dat", "data/handling.cfg", "data/weapon.dat",
        "models/gta3.img", "models/gta3.dir", "models/frontend.txd", "models/menu.txd", "models/fonts.txd",
        "anim/ped.ifp", "anim/cuts.img", "anim/cuts.dir", "text/american.gxt", "audio/sfx.raw", "audio/sfx.sdt"];

    public static void ValidateSource(ManagerSettings s, ActionOptions o)
    {
        if (WantsPc(o)) Workflows.ValidateGameDirectory(s.GameDirectory);
        if (o.InstallXboxVariant && (string.IsNullOrWhiteSpace(o.XboxIsoPath) || !File.Exists(o.XboxIsoPath)))
            throw new IOException("Choose your local Original Xbox GTA III US 1.0 ISO.");
    }
    public static Dictionary<string,string> Seal(string root, string id)
    {
        var files = OriginalXboxModels.SafeFiles(root).Where(p => Path.GetFileName(p) != Marker && Path.GetFileName(p) != ".variant-hashes.json")
            .ToDictionary(p => Path.GetRelativePath(root,p).Replace('\\','/'), p => p, StringComparer.Ordinal);
        if(files.Count>20000 || files.Keys.Any(p=>p.Contains("..",StringComparison.Ordinal)||p.Contains(':')||p.IndexOfAny(['\t','\r','\n'])>=0))throw new InvalidDataException("Unsafe or excessive variant inventory.");
        if (Required.Any(p => !files.ContainsKey(p))) throw new InvalidDataException("Incomplete " + id + " variant: missing " + string.Join(", ", Required.Where(p => !files.ContainsKey(p))));
        File.WriteAllText(Path.Combine(root, Marker), string.Join("\n", new[] { Identity(id) }.Concat(files.OrderBy(p => p.Key,StringComparer.Ordinal).Select(p => new FileInfo(p.Value).Length + "\t" + p.Key)))+"\n", new UTF8Encoding(false));
        var hashes = files.ToDictionary(p => p.Key,p => Digests.Sha256(p.Value),StringComparer.Ordinal);
        hashes[Marker] = Digests.Sha256(Path.Combine(root,Marker));
        File.WriteAllText(Path.Combine(root,".variant-hashes.json"),JsonSerializer.Serialize(hashes));
        return hashes;
    }
    public static Dictionary<string,string> Validate(string root,string id)
    {
        Paths.NoLinks(root);
        var hashes=JsonSerializer.Deserialize<Dictionary<string,string>>(File.ReadAllText(Paths.Child(root,".variant-hashes.json"))) ?? throw new InvalidDataException("Invalid variant manifest.");
        if(File.ReadLines(Paths.Child(root,Marker)).FirstOrDefault()!=Identity(id) || Required.Any(p=>!hashes.ContainsKey(p)))throw new InvalidDataException("Incomplete variant manifest.");
        var actual=OriginalXboxModels.SafeFiles(root).Select(p=>Path.GetRelativePath(root,p).Replace('\\','/')).ToHashSet(StringComparer.Ordinal);
        if(!actual.SetEquals(hashes.Keys.Append(".variant-hashes.json")))throw new InvalidDataException("Missing or unmanifested variant content.");
        foreach(var (p,h) in hashes)if(!Digests.Sha256(Paths.Child(root,p)).Equals(h,StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("Variant checksum mismatch: "+p);
        return hashes;
    }
    public static async Task<Dictionary<string,string>> Prepare(ManagerSettings s,ActionOptions o,string portFiles,ICommandRunner runner,PrivateLog log,CancellationToken token,string? astcEncoder = null)
    {
        ValidateSource(s,o); var prepared=new Dictionary<string,string>();
        foreach(var id in new[]{"pc","xbox"}.Where(id=>id=="pc"?WantsPc(o):o.InstallXboxVariant)) {
            var parent=Path.Combine(s.Workplace,"prepared-variants",Guid.NewGuid().ToString("N"));Directory.CreateDirectory(parent);
            var stage=Path.Combine(parent,id);
            if(id=="pc") {
                Directory.CreateDirectory(stage);
                foreach(var dir in Workflows.ValidateGameDirectory(s.GameDirectory).Where(p=>Path.GetFileName(p).ToLowerInvariant() is not ("mp3" or "skins"))) CopyLower(dir,Path.Combine(stage,Path.GetFileName(dir).ToLowerInvariant()),token);
                if(o.ConvertRadioStations)await new RadioStations(s,runner,log).ConvertLocal(Path.Combine(stage,"audio"),token);
            } else {
                await new OriginalXboxModels(s,runner,log).Prepare(o with { CompleteVariant=true,ModelArchive=o.XboxIsoPath,ModelOutput=stage,ModelSource="",DownloadModels=false },token);
            }
            foreach(var p in Workflows.PortAssets(true)) {
                var source=Path.Combine(portFiles,p);if(!File.Exists(source))throw new IOException("Missing bundled port asset: "+p);
                var relative=p.Replace('\\','/').ToLowerInvariant();var target=Paths.Child(stage,relative);
                if(id=="xbox" && relative.StartsWith("text/",StringComparison.Ordinal)) {
                    if(File.Exists(target))GxtMerge.Merge(target,source);
                    continue;
                }
                if(id=="xbox" && relative is "models/generic.txd" or "models/particle.txd")continue;
                Directory.CreateDirectory(Path.GetDirectoryName(target)!);File.Copy(source,target,true);
            }
            Workflows.RefreshWindowsPortFiles(s.Repository,stage,token);
            if (astcEncoder is not null)
                await AstcTextureCache.Build(stage, astcEncoder, runner, log, token);
            // Normalize overlay file casing too, so runtime inventories are portable.
            Seal(stage,id); Validate(stage,id);prepared[id]=stage;
            log.Write("Complete "+id+" variant prepared and checksummed locally.");
        }
        return prepared;
    }
    private static void CopyLower(string source,string destination,CancellationToken token)
    {
        Paths.NoLinks(source); Directory.CreateDirectory(destination);
        foreach(var entry in Directory.EnumerateFileSystemEntries(source)) {
            token.ThrowIfCancellationRequested();Paths.NoLinks(entry);var target=Path.Combine(destination,Path.GetFileName(entry).ToLowerInvariant());
            if(Directory.Exists(entry))CopyLower(entry,target,token);else File.Copy(entry,target,false);
        }
    }
    public static void InstallLocal(string root,string id,string source,CancellationToken token)
    {
        if(OperatingSystem.IsWindows())foreach(var name in new[]{"libertyvr-game","libertyvr-upstate"}) {
            var processes=System.Diagnostics.Process.GetProcessesByName(name);
            try{if(processes.Length>0)throw new IOException("Quit PCVR before installing complete variants.");}
            finally{foreach(var process in processes)process.Dispose();}
        }
        Validate(source,id);Paths.NoLinks(root);var variants=Paths.Child(root,"variants");Directory.CreateDirectory(variants);
        var stage=Path.Combine(variants,".stage-"+id+"-"+Guid.NewGuid().ToString("N"));Paths.CopyTree(source,stage,token);Validate(stage,id);
        var final=Path.Combine(variants,id);var backup=Path.Combine(variants,".backup-"+id+"-"+Guid.NewGuid().ToString("N"));
        if(Directory.Exists(final)&&!File.Exists(Path.Combine(final,Marker)))throw new IOException("Refusing to replace unmarked variant data.");
        var existed=Directory.Exists(final);if(existed)Directory.Move(final,backup);
        try{Directory.Move(stage,final);}catch{if(existed)Directory.Move(backup,final);throw;}
        File.WriteAllText(Path.Combine(variants,"MANAGED.txt"),"Liberty City VR complete variants v1\n");
        File.WriteAllText(Path.Combine(root,".libertyvr-pcvr-data"),"Liberty City VR local Windows data v1\n");
    }
    public static async Task InstallQuest(QuestDevice device,string id,string source,CancellationToken token)
    {
        var hashes=Validate(source,id);hashes[".variant-hashes.json"]=Digests.Sha256(Path.Combine(source,".variant-hashes.json"));
        var root=QuestDevice.GameData+"/variants";var stamp=Guid.NewGuid().ToString("N");
        var stage=root+"/.stage-"+id+"-"+stamp;
        var created=new HashSet<string>(StringComparer.Ordinal);
        foreach(var (p,h) in hashes) {var remote=stage+"/"+p;
            var directory="variants/.stage-"+id+"-"+stamp+"/"+(p.Contains('/')?p[..p.LastIndexOf('/')]:"");
            if(created.Add(directory))await device.Run(["shell","content","call","--uri",QuestDevice.SaveUri,"--method","mkdir","--arg",QuestDevice.Quote(directory.TrimEnd('/'))],token);
            await device.Push(Paths.Child(source,p),remote,token,h);
        }
        await ActivateQuest(device,id,".stage-"+id+"-"+stamp,token);
    }
    public static async Task ActivateQuest(QuestDevice device,string id,string stagingName,CancellationToken token)
    {
        Identity(id);
        if(!System.Text.RegularExpressions.Regex.IsMatch(stagingName,"\\A\\.stage-"+id+"-[a-f0-9]{32}\\z"))throw new InvalidDataException("Invalid variant staging name.");
        var result=await device.Run(["shell","content","call","--uri",QuestDevice.SaveUri,"--method","activate-variant","--arg",QuestDevice.Quote(id+":"+stagingName)],token);
        if(!result.Output.Contains("status=activated",StringComparison.Ordinal))throw new IOException("Application could not activate the staged variant: "+result.Output.Trim());
    }
}
