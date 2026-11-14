using System.Globalization;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace LibertyManager;

public sealed record BenchmarkAsset(string Path, long Size, string Sha256, string Token);
public sealed record BenchmarkAssetDifference(string Path, string Difference);
public sealed record BenchmarkSetup(int Version,string Data,string User,string AssetFingerprint,int LocalCopies,int DeviceCopies);

/// <summary>Local-only, original-preserving benchmark data preparation.</summary>
public static partial class BenchmarkAssets
{
    public static readonly string[] Roots=["data","models","anim","txd","modelsets"];
    [GeneratedRegex(@"^Row:\s*\d+\s+path_b64=([A-Za-z0-9_-]+),\s*_size=(\d+),\s*sha256=([0-9a-f]{64})\s*$",RegexOptions.CultureInvariant)]
    private static partial Regex InventoryRow();
    public static string Key(string path) => new(path.Select(c=>c>='A'&&c<='Z'?(char)(c-'A'+'a'):c).ToArray());
    public static string Token(string path) => Convert.ToBase64String(Encoding.UTF8.GetBytes(path)).TrimEnd('=').Replace('+','-').Replace('/','_');
    public static string ValidatePath(string path)
    {
        var parts=path.Split('/');
        if(path.Length>1024 || parts.Length<2 || parts.Length>18 || !Roots.Contains(parts[0],StringComparer.Ordinal))
            throw new InvalidDataException("Benchmark asset path is outside the supported roots.");
        foreach(var part in parts)
        {
            if(part is "" or "." or ".." || part.EndsWith('.') || part.EndsWith(' ') || part.Any(c=>c<32 || c==127 || "\\:<>|?*".Contains(c)))
                throw new InvalidDataException("Invalid benchmark asset path component.");
            var name=part.Split('.')[0].ToUpperInvariant();
            if(name is "CON" or "PRN" or "AUX" or "NUL" || Regex.IsMatch(name,@"^(COM|LPT)[1-9]$",RegexOptions.CultureInvariant))
                throw new InvalidDataException("Benchmark asset name cannot be represented on Windows.");
        }
        return path;
    }
    public static BenchmarkAsset[] Parse(string text)
    {
        if(text.Length>8*1024*1024) throw new InvalidDataException("Benchmark inventory is too large.");
        var result=new List<BenchmarkAsset>();var names=new HashSet<string>(StringComparer.Ordinal);long total=0;
        foreach(var line in text.Split('\n').Where(l=>!string.IsNullOrWhiteSpace(l)))
        {
            var match=InventoryRow().Match(line.TrimEnd('\r'));
            if(!match.Success) throw new InvalidDataException("Benchmark asset inventory is unavailable or malformed. Update the app and retry locally.");
            var token=match.Groups[1].Value;
            string path;
            try {
                var bytes=Convert.FromBase64String(token.Replace('-','+').Replace('_','/').PadRight((token.Length+3)/4*4,'='));
                path=new UTF8Encoding(false,true).GetString(bytes);
            } catch(Exception error) when(error is FormatException or DecoderFallbackException) { throw new InvalidDataException("Invalid benchmark asset token."); }
            ValidatePath(path);
            if(Token(path)!=token || !names.Add(Key(path)) || !long.TryParse(match.Groups[2].Value,NumberStyles.None,CultureInfo.InvariantCulture,out var size) ||
               size>2147483648L || result.Count>=20000 || (total+=size)>34359738368L)
                throw new InvalidDataException("Invalid, ambiguous or excessive benchmark inventory.");
            result.Add(new(path,size,match.Groups[3].Value,token));
        }
        if(result.Count==0) throw new InvalidDataException("No benchmark assets found.");
        return result.ToArray();
    }
    public static async Task<BenchmarkAsset[]> Inventory(QuestDevice device,CancellationToken token)
    {
        using var limit=CancellationTokenSource.CreateLinkedTokenSource(token);limit.CancelAfter(TimeSpan.FromMinutes(3));
        var result=await device.Shell("content query --uri 'content://"+QuestDevice.Package+".performance/assets' --projection path_b64:_size:sha256",limit.Token);
        return Parse(result.Output);
    }
    public static BenchmarkAsset[] LocalInventory(string root)
    {
        Paths.NoLinks(root);var files=new List<BenchmarkAsset>();
        foreach(var name in Roots) {
            var directory=Path.Combine(root,name);if(!Directory.Exists(directory))continue;
            Paths.NoLinks(directory);
            foreach(var file in Files(directory)) {
                Paths.NoLinks(file);var relative=ValidatePath(Path.GetRelativePath(root,file).Replace('\\','/'));
                files.Add(new(relative,new FileInfo(file).Length,Digests.Sha256(file).ToLowerInvariant(),Token(relative)));
            }
        }
        if(files.Select(f=>Key(f.Path)).Distinct(StringComparer.Ordinal).Count()!=files.Count) throw new InvalidDataException("Ambiguous local benchmark asset case.");
        return files.ToArray();
    }
    private static IEnumerable<string> Files(string directory)
    {
        var pending=new Stack<string>();pending.Push(directory);
        while(pending.TryPop(out var current)) {
            Paths.NoLinks(current);
            foreach(var entry in Directory.EnumerateFileSystemEntries(current)) {
                Paths.NoLinks(entry);
                if(Directory.Exists(entry))pending.Push(entry);
                else if(File.Exists(entry))yield return entry;
                else throw new IOException("Unsupported benchmark asset entry.");
            }
        }
    }
    public static BenchmarkAssetDifference[] Compare(IEnumerable<BenchmarkAsset> reference,IEnumerable<BenchmarkAsset> local)
    {
        var a=reference.ToDictionary(f=>Key(f.Path),StringComparer.Ordinal);var b=local.ToDictionary(f=>Key(f.Path),StringComparer.Ordinal);
        return a.Keys.Union(b.Keys,StringComparer.Ordinal).Order(StringComparer.Ordinal).Select(key=>
            !a.ContainsKey(key)?new BenchmarkAssetDifference(b[key].Path,"PCVR-only file"):
            !b.ContainsKey(key)?new BenchmarkAssetDifference(a[key].Path,"Quest-only file"):
            a[key].Size!=b[key].Size || !a[key].Sha256.Equals(b[key].Sha256,StringComparison.OrdinalIgnoreCase)?new BenchmarkAssetDifference(a[key].Path,"Different content"):null)
            .Where(v=>v is not null).Select(v=>v!).ToArray();
    }
    public static string Fingerprint(string root)
    {
        Paths.NoLinks(root);
        var paths=Roots.SelectMany(name=>Directory.Exists(Path.Combine(root,name))?Files(Path.Combine(root,name)):[])
            .Select(file=>(Name:Key(Path.GetRelativePath(root,file).Replace('\\','/')),File:file))
            .OrderBy(p=>Convert.ToHexString(Encoding.UTF8.GetBytes(p.Name)),StringComparer.Ordinal);
        ulong hash=14695981039346656037UL;
        void Add(ReadOnlySpan<byte> data){foreach(var value in data)hash=unchecked((hash^value)*1099511628211UL);}
        var buffer=new byte[65536];
        foreach(var item in paths){
            Paths.NoLinks(item.File);
            Add(Encoding.UTF8.GetBytes(item.Name+"="+new FileInfo(item.File).Length.ToString(CultureInfo.InvariantCulture)+"\n"));
            using var stream=File.OpenRead(item.File);int count;while((count=stream.Read(buffer))>0)Add(buffer.AsSpan(0,count));
        }
        return hash.ToString("x",CultureInfo.InvariantCulture);
    }
    public static async Task<BenchmarkSetup> Prepare(ManagerSettings settings,string output,BenchmarkAsset[] inventory,
        Func<BenchmarkAsset,string,CancellationToken,Task> copyRemote,CancellationToken token)
    {
        var original=PcvrPaths.From(settings,PcvrWorkflow.HostTarget);original.Validate(settings);PcvrWorkflow.ValidatePreparedData(original.Data);
        output=Path.GetFullPath(output);Paths.NoLinks(output);
        var data=Paths.Child(output,"data");var user=Paths.Child(output,"user");
        (original with { Data=data,User=user }).Validate(settings);
        foreach(var old in new[]{original.Data,original.User,original.Install})
            if(Paths.Within(old,output) || Paths.Within(output,old)) throw new InvalidDataException("Keep benchmark copies separate from existing PCVR directories.");
        if(Directory.Exists(output) || File.Exists(output)) throw new IOException("Choose a new benchmark setup directory; existing data is preserved.");
        // Revalidate supplied records, including programmatic/test callers.
        inventory=Parse(string.Join("\n",inventory.Select((f,i)=>$"Row: {i} path_b64={f.Token}, _size={f.Size.ToString(CultureInfo.InvariantCulture)}, sha256={f.Sha256}")));
        Directory.CreateDirectory(output);File.WriteAllText(Paths.Child(output,"INCOMPLETE.txt"),"Local-only partial benchmark setup; original data is unchanged.\n");
        Directory.CreateDirectory(data);Directory.CreateDirectory(user);
        foreach(var entry in Directory.EnumerateFileSystemEntries(original.Data)) {
            token.ThrowIfCancellationRequested();var name=Path.GetFileName(entry);
            if(Roots.Contains(name,StringComparer.OrdinalIgnoreCase) || name is ".libertyvr-pcvr-data" or ".libertyvr-modelset-staging" or ".libertyvr-modelset-backups")continue;
            Paths.NoLinks(entry);var destination=Paths.Child(data,name);
            if(Directory.Exists(entry))Paths.CopyTree(entry,destination,token);else File.Copy(entry,destination);
        }
        if(Directory.Exists(original.User)) foreach(var entry in Directory.EnumerateFileSystemEntries(original.User)) {
            token.ThrowIfCancellationRequested();var name=Path.GetFileName(entry);
            if(name is "perf-captures" or "renderdoc-captures" or "quest_perf.csv")continue;
            Paths.NoLinks(entry);var destination=Paths.Child(user,name);
            if(Directory.Exists(entry))Paths.CopyTree(entry,destination,token);else File.Copy(entry,destination);
        }
        var reused=0;var downloaded=0;
        foreach(var asset in inventory) {
            token.ThrowIfCancellationRequested();var destination=Paths.Child(data,asset.Path);Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
            var candidate=Paths.Child(original.Data,asset.Path);
            if(File.Exists(candidate) && new FileInfo(candidate).Length==asset.Size && Digests.Sha256(candidate).Equals(asset.Sha256,StringComparison.OrdinalIgnoreCase))
            {File.Copy(candidate,destination);++reused;}
            else {await copyRemote(asset,destination,token);++downloaded;}
            if(!File.Exists(destination) || new FileInfo(destination).Length!=asset.Size || !Digests.Sha256(destination).Equals(asset.Sha256,StringComparison.OrdinalIgnoreCase))
                throw new IOException("Benchmark asset copy failed verification; incomplete copy retained locally, originals unchanged.");
        }
        if(!Directory.Exists(Path.Combine(data,"data")) || !Directory.Exists(Path.Combine(data,"models")))throw new IOException("Benchmark inventory lacks required game roots.");
        var setup=new BenchmarkSetup(1,data,user,Fingerprint(data),reused,downloaded);
        File.WriteAllText(Paths.Child(output,"inventory.json"),JsonSerializer.Serialize(inventory,new JsonSerializerOptions{WriteIndented=true}));
        token.ThrowIfCancellationRequested();
        File.WriteAllText(Paths.Child(data,".libertyvr-pcvr-data"),"Liberty City VR local Quest-aligned benchmark data v1\n");
        File.WriteAllText(Paths.Child(output,"benchmark.json"),JsonSerializer.Serialize(setup,new JsonSerializerOptions{WriteIndented=true}));
        File.Delete(Paths.Child(output,"INCOMPLETE.txt"));return setup;
    }
    public static async Task CopyFromDevice(QuestDevice device,BenchmarkAsset asset,string local,CancellationToken token)
    {
        using var limit=CancellationTokenSource.CreateLinkedTokenSource(token);limit.CancelAfter(TimeSpan.FromMinutes(5));
        var remote="/data/local/tmp/lcvr-benchmark-asset-"+Guid.NewGuid().ToString("N")+".tmp";
        var uri="content://"+QuestDevice.Package+".performance/asset/"+asset.Token;
        try {
            await device.Shell("content read --uri "+QuestDevice.Quote(uri)+" > "+QuestDevice.Quote(remote),limit.Token);
            var size=await device.Shell("wc -c < "+QuestDevice.Quote(remote),limit.Token);
            if(!long.TryParse(size.Output.Trim(),out var length) || length!=asset.Size)throw new IOException("Benchmark asset changed or could not be read.");
            await device.Run(["pull",remote,local],limit.Token);
        } finally {
            using var cleanup=new CancellationTokenSource(TimeSpan.FromSeconds(10));
            try{await device.Shell("rm -f "+QuestDevice.Quote(remote),cleanup.Token,check:false);}catch(OperationCanceledException){}
        }
    }
}
