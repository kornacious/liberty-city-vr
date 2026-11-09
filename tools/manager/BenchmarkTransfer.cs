using System.Text.RegularExpressions;

namespace LibertyManager;

public static partial class BenchmarkTransfer
{
    [GeneratedRegex("^[0-9]{8}T[0-9]{6}Z-[0-9]+\\.(quest-profile|frames\\.csv|session\\.csv|sim\\.csv|run\\.csv)$",RegexOptions.CultureInvariant)]
    private static partial Regex DiagnosticName();
    public static bool AllowedName(string name) => DiagnosticName().IsMatch(name);
    public static (string Name,long Size)[] Listing(string text)
    {
        var rows=new List<(string Name,long Size)>();
        foreach(var line in text.Split('\n'))
        {
            var match=Regex.Match(line,@"_display_name=([^,\r\n]+),\s*_size=(\d+)\s*$",RegexOptions.CultureInvariant);
            if(!match.Success || !AllowedName(match.Groups[1].Value) || !long.TryParse(match.Groups[2].Value,out var size) || size<1 || size>128*1024*1024) continue;
            rows.Add((match.Groups[1].Value,size));
        }
        return rows.Distinct().OrderByDescending(p=>p.Name,StringComparer.Ordinal).Take(12).ToArray();
    }
    public static async Task Collect(QuestDevice device,string output,PrivateLog log,CancellationToken token)
    {
        output=Path.GetFullPath(output);Paths.NoLinks(output);
        if(Directory.Exists(output) || File.Exists(output)) throw new IOException("Choose a new benchmark collection directory; existing captures are preserved.");
        Directory.CreateDirectory(output);var count=0;
        foreach(var directory in new[]{"perf-captures","simulation-replays"})
        {
            var authority="content://"+QuestDevice.Package+".performance/";
            var replay=directory=="simulation-replays";
            var listing=await device.Shell("content query --uri "+QuestDevice.Quote(authority+(replay?"replays":"captures"))+" --projection _display_name:_size",token);
            if(listing.Output.Contains("Error",StringComparison.OrdinalIgnoreCase) || listing.Output.Contains("Exception",StringComparison.Ordinal))
                throw new IOException("Cannot query the installed app's diagnostics provider. Update the app before collecting.");
            var names=Listing(listing.Output);
            var destination=Path.Combine(output,directory);Directory.CreateDirectory(destination);
            foreach(var (name,size) in names)
            {
                var uri=authority+(replay?"replay/":"capture/")+name;
                var remote="/data/local/tmp/lcvr-benchmark-"+Guid.NewGuid().ToString("N")+".tmp";
                var local=Paths.Child(destination,name);
                try {
                    await device.Shell("content read --uri "+QuestDevice.Quote(uri)+" > "+QuestDevice.Quote(remote),token);
                    await device.Run(["pull",remote,local],token);
                    var digest=await device.Shell("sha256sum "+QuestDevice.Quote(remote),token);
                    if(!File.Exists(local) || new FileInfo(local).Length!=size || !string.Equals(digest.Output.Split((char[]?)null,StringSplitOptions.RemoveEmptyEntries).FirstOrDefault(),Digests.Sha256(local),StringComparison.OrdinalIgnoreCase))
                        throw new IOException("Benchmark diagnostic copy failed verification; partial collection retained locally.");
                } finally { await device.Shell("rm -f "+QuestDevice.Quote(remote),CancellationToken.None,check:false); }
                ++count;
            }
        }
        if(count==0) throw new IOException("No completed benchmark diagnostics found. Record/play a route and stop profiling before collecting.");
        log.Write($"Collected {count} recent benchmark diagnostic files locally. No game assets or saves were copied.");
    }
}
