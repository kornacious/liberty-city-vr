using System.Globalization;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;

namespace LibertyManager;

public sealed record TimingSummary(int Samples, double MeanMs, double P50Ms, double P95Ms, double P99Ms);
public sealed record BenchmarkSummary(string Target, TimingSummary Cpu, TimingSummary Interval, TimingSummary? SampledGpu);
public sealed record BenchmarkComparison(bool WorkloadMatched, string[] Rejections, BenchmarkSummary Left, BenchmarkSummary Right, string[] CapabilityDifferences,
    string TimingScope = "Measured on each execution device; desktop timings do not predict Quest performance.",
    string Limits = "Route-assisted workload alignment, not deterministic simulation. Ambient RNG, traffic, particles, audio, thermal state and runtime scheduling differ. Compression fallback differences are reported by session enabled_* fields. Repeat runs on each device; no cross-device speedup is inferred.");

public static partial class BenchmarkReports
{
    public static string ProfileIdentity(IEnumerable<string> lines)
    {
        ulong hash=14695981039346656037UL;
        foreach(var line in lines) foreach(var value in Encoding.UTF8.GetBytes(line+"\n")) hash=unchecked((hash^value)*1099511628211UL);
        return hash.ToString("x",CultureInfo.InvariantCulture);
    }
    [GeneratedRegex("^[0-9TZ-]{10,80}$", RegexOptions.CultureInvariant)] private static partial Regex Stem();
    public static string SourceIdentity(string repository)
    {
        using var hash=IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        hash.AppendData(Encoding.UTF8.GetBytes("LCVR_WORKLOAD_SOURCE_V1;re3="+Toolchain.Re3Commit+"\n"));
        var extensions=new HashSet<string>([".c",".cc",".cpp",".cxx",".h",".hpp",".inc",".inl",".in",".vert",".frag",".comp",".glsl",".patch",".cmake",".kts",".gradle",".java",".xml"],StringComparer.OrdinalIgnoreCase);
        foreach(var directory in new[] {"overlay","librw","patches"})
        {
            var root=Path.Combine(repository,directory);
            if(!Directory.Exists(root)) continue;
            foreach(var file in Directory.EnumerateFiles(root,"*",SearchOption.AllDirectories).Order(StringComparer.Ordinal))
            {
                Paths.NoLinks(file);
                var relative=Path.GetRelativePath(repository,file).Replace('\\','/');
                if(relative.EndsWith("/SourceIdentity.h",StringComparison.Ordinal)) continue;
                if(!extensions.Contains(Path.GetExtension(file)) && Path.GetFileName(file)!="CMakeLists.txt") continue;
                var bytes=File.ReadAllBytes(file);
                if(Array.IndexOf(bytes,(byte)'\r')>=0) {
                    using var normalized=new MemoryStream(bytes.Length);
                    for(var i=0;i<bytes.Length;++i) if(bytes[i]!='\r' || i+1==bytes.Length || bytes[i+1]!='\n') normalized.WriteByte(bytes[i]);
                    bytes=normalized.ToArray();
                }
                hash.AppendData(Encoding.UTF8.GetBytes(relative+"="+bytes.Length.ToString(CultureInfo.InvariantCulture)+"\n"));
                hash.AppendData(bytes);
            }
        }
        return Convert.ToHexStringLower(hash.GetHashAndReset());
    }
    private static Dictionary<string,string> Map(string path)
    {
        Paths.NoLinks(path);
        if(new FileInfo(path).Length>65536) throw new InvalidDataException("Benchmark metadata exceeds 64 KiB.");
        var result=new Dictionary<string,string>(StringComparer.Ordinal);
        foreach(var line in File.ReadLines(path))
        {
            var split=line.IndexOf(',');
            if(split<1 || !result.TryAdd(line[..split],line[(split+1)..])) throw new InvalidDataException("Malformed or duplicate benchmark metadata.");
        }
        return result;
    }
    private static string Value(Dictionary<string,string> map,string key) => map.TryGetValue(key,out var value) ? value : "";
    private static double Number(string text) => double.TryParse(text,NumberStyles.Float,CultureInfo.InvariantCulture,out var value) && double.IsFinite(value)
        ? value : throw new InvalidDataException("Invalid benchmark numeric value.");
    private static TimingSummary Summary(List<double> values)
    {
        if(values.Count==0) throw new InvalidDataException("No valid benchmark timing samples.");
        values.Sort();
        double Percentile(double p) => values[Math.Clamp((int)Math.Ceiling(p*values.Count)-1,0,values.Count-1)];
        return new(values.Count,values.Average(),Percentile(.50),Percentile(.95),Percentile(.99));
    }
    private sealed record Capture(Dictionary<string,string> Run,Dictionary<string,string> Session,Dictionary<string,double> Profile,BenchmarkSummary Summary,HashSet<string> States,bool Invalid,bool Dropped);
    private static Capture Read(string path)
    {
        path=Path.GetFullPath(path); var run=Map(path);
        var stem=Path.GetFileName(Value(run,"profile").Replace('\\','/'));
        if(!Stem().IsMatch(stem)) throw new InvalidDataException("Run does not reference a valid local profiler capture.");
        var directory=Path.GetDirectoryName(path)!;
        var capture=Path.Combine(directory,stem);
        if(!File.Exists(capture+".session.csv")) capture=Path.Combine(directory,"..","perf-captures",stem);
        var session=Map(capture+".session.csv");
        var profilePath=capture+".quest-profile";Paths.NoLinks(profilePath);
        if(new FileInfo(profilePath).Length>32768) throw new InvalidDataException("Benchmark profile exceeds 32 KiB.");
        var profile=new Dictionary<string,double>(StringComparer.Ordinal);
        var profileLines=File.ReadAllLines(profilePath);
        if(ProfileIdentity(profileLines)!=Value(session,"quest_workload_identity"))
            throw new InvalidDataException("Benchmark profile snapshot does not match its recorded identity.");
        foreach(var line in profileLines) {
            var split=line.IndexOf('=');
            if(split<1 || !profile.TryAdd(line[..split],Number(line[(split+1)..]))) throw new InvalidDataException("Malformed or duplicate benchmark profile field.");
        }
        if(!profile.TryGetValue("schema",out var profileVersion) || profileVersion!=1 || !profile.TryGetValue("quest3",out var quest) || quest!=1)
            throw new InvalidDataException("Unsupported benchmark profile snapshot.");
        var start=ulong.Parse(Value(run,"playback_start_monotonic_ns"),CultureInfo.InvariantCulture);
        var end=ulong.Parse(Value(run,"playback_end_monotonic_ns"),CultureInfo.InvariantCulture);
        if(end<=start || end-start<3_000_000_000) throw new InvalidDataException("Benchmark must contain at least three wall-clock seconds.");
        // Exclude the first second in addition to replay's unprofiled preparation.
        start+=1_000_000_000;
        var cpu=new List<double>(); var intervals=new List<double>(); var gpu=new List<double>();
        var states=new HashSet<string>(StringComparer.Ordinal); var gpuIds=new HashSet<string>();
        var invalid=false;var dropped=false;
        using var rows=File.ReadLines(capture+".frames.csv").GetEnumerator();
        if(!rows.MoveNext()) throw new InvalidDataException("Empty benchmark frames.");
        var columns=rows.Current.Split(',');
        if(columns.Distinct().Count()!=columns.Length) throw new InvalidDataException("Duplicate benchmark columns.");
        var indices=columns.Select((name,index)=>(name,index)).ToDictionary(p=>p.name,p=>p.index,StringComparer.Ordinal);
        string Cell(string[] row,string name) => indices.TryGetValue(name,out var i) ? row[i] : throw new InvalidDataException("Missing benchmark column: "+name);
        string[] stateColumns=["render_width","render_height","render_scale_effective","refresh_hz","active_world","active_vegetation","active_vehicles","active_peds","active_weapons","replay_forward_view_active","world_lod_mode","world_occlusion_enabled","world_backface_enabled","static_instances_enabled","lod_fade_transition_enabled","fxaa_integer_sampling_enabled"];
        var count=0;
        while(rows.MoveNext())
        {
            if(++count>1000000 || rows.Current.Length>65536) throw new InvalidDataException("Benchmark frame limit exceeded.");
            var row=rows.Current.Split(','); if(row.Length!=columns.Length) throw new InvalidDataException("Truncated benchmark frame.");
            var time=ulong.Parse(Cell(row,"start_monotonic_ns"),CultureInfo.InvariantCulture);
            if(time<start || time>end) continue;
            var cpuMs=Number(Cell(row,"cpu_local_ms")); var interval=Number(Cell(row,"frame_interval_ms"));
            if(cpuMs<0 || interval<=0) { invalid=true; continue; }
            cpu.Add(cpuMs); intervals.Add(interval);
            dropped |= Number(Cell(row,"dropped_rows"))!=0;
            invalid |= Cell(row,"replay_forward_view_active")!="1" ||
                Cell(row,"paused")!="0" || Cell(row,"cutscene")!="0" || Cell(row,"renderdoc_capture_active")!="0";
            states.Add(string.Join("|",stateColumns.Select(key=>Cell(row,key))));
            var gpuTime=ulong.Parse(Cell(row,"gpu_pass_start_monotonic_ns"),CultureInfo.InvariantCulture);
            if(gpuTime>=start && gpuTime<=end && Cell(row,"gpu_pass_status")=="1" && gpuIds.Add(Cell(row,"gpu_pass_frame_id")))
            {
                var ms=Number(Cell(row,"gpu_pass_frame_ms")); if(ms>=0) gpu.Add(ms);
            }
        }
        if(cpu.Count<30) throw new InvalidDataException("Benchmark needs at least 30 valid frames after warmup.");
        var target=Value(session,"execution_target");
        if(target is not ("windows_pcvr" or "quest_android")) throw new InvalidDataException("Unknown benchmark execution target.");
        return new(run,session,profile,new(target,Summary(cpu),Summary(intervals),gpu.Count==0?null:Summary(gpu)),states,invalid,dropped);
    }
    public static BenchmarkComparison Compare(string left,string right)
    {
        var a=Read(left); var b=Read(right); var reasons=new List<string>();
        void Require(bool ok,string reason) { if(!ok) reasons.Add(reason); }
        foreach(var c in new[]{a,b})
        {
            var target=c.Summary.Target+": ";
            foreach(var pair in new[] {("schema","LCVR_SIM_RUN_V1"),("result","COMPLETE"),("matched_workload","1"),("ghost","1"),("auto_profile","1"),("tracking","route_assisted"),("view_policy","fixed_forward"),("renderdoc_requested","0"),("compatibility_dirty_at_end","0"),("environment","noon_sunny_each_simulation_step")})
                Require(Value(c.Run,pair.Item1)==pair.Item2,target+"run requirement: "+pair.Item1);
            Require(Value(c.Session,"closed")=="1" && Value(c.Session,"write_error")=="0",target+"incomplete or failed profiler capture");
            Require(Value(c.Session,"dropped_rows")=="0" && !c.Dropped,target+"dropped recorder rows");
            Require(Value(c.Session,"compatibility_dirty_at_start")=="0" && !c.Invalid,target+"capture changed settings, paused, lost fixed view or used capture instrumentation");
            Require(Value(c.Run,"source_identity").Length==64,target+"missing verified source identity");
            Require(Value(c.Run,"asset_fingerprint").Length>0 && Value(c.Run,"route_fingerprint").Length>0,target+"missing workload fingerprints");
            Require(c.States.Count==1,target+"rendering state changed during capture");
        }
        foreach(var key in new[]{"route_fingerprint","asset_fingerprint","source_identity","model","handling","handling_match_policy","elapsed_us","recording_ghost","ghost_policy","assets_0","assets_1","assets_2","assets_3","assets_4"})
            Require(Value(a.Run,key)!="" && Value(a.Run,key)==Value(b.Run,key),"Different workload: "+key);
        foreach(var key in new[]{"version","enabled_bc"})
            Require(Value(a.Session,key)!="" && Value(a.Session,key)==Value(b.Session,key),"Different effective profile: "+key);
        Require(Value(a.Session,"quest_workload_identity")!="unavailable","No valid Quest workload profile");
        Require(Value(b.Session,"quest_workload_identity")!="unavailable","No valid Quest workload profile");
        // The observed period has nanosecond quantization. Compare the numeric
        // snapshots rather than rejecting two nominally equal rates by hash.
        Require(a.Profile.Count==b.Profile.Count && a.Profile.All(pair=>b.Profile.TryGetValue(pair.Key,out var other) &&
            (pair.Key=="hz" ? Math.Abs(pair.Value-other)<=.001 : pair.Value==other)),"Different effective profile snapshot");
        Require(a.States.SetEquals(b.States),"Different effective rendering state or refresh rate");
        var differences=new[]{"enabled_astc","enabled_etc2"}.Where(key=>Value(a.Session,key)!=Value(b.Session,key)).ToArray();
        return new(reasons.Count==0,reasons.Distinct().ToArray(),a.Summary,b.Summary,differences);
    }
}
