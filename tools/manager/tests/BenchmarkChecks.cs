using System.Globalization;
using LibertyManager;

internal static class BenchmarkChecks
{
    public static async Task Run(string root,Func<string,Func<Task>,Task> test)
    {
        void Check(bool ok) { if(!ok) throw new Exception("Benchmark assertion failed."); }
        string Fixture(string name,string target)
        {
            var directory=Path.Combine(root,"benchmark-"+name);Directory.CreateDirectory(directory);
            const string stem="20261109T000000Z-123";
            var run=Path.Combine(directory,"test.run.csv");
            File.WriteAllText(run,"schema,LCVR_SIM_RUN_V1\nresult,COMPLETE\nmatched_workload,1\nghost,1\nauto_profile,1\ntracking,route_assisted\nview_policy,fixed_forward\nrenderdoc_requested,0\ncompatibility_dirty_at_end,0\nenvironment,noon_sunny_each_simulation_step\nprofile,"+stem+"\nplayback_start_monotonic_ns,1000000000\nplayback_end_monotonic_ns,5000000000\nsource_identity,"+new string('a',64)+"\nasset_fingerprint,111\nroute_fingerprint,222\nmodel,90\nhandling,123\nelapsed_us,10000000\nrecording_ghost,1\nghost_policy,traffic_props_preserve_roads\nassets_0,0\nassets_1,0\nassets_2,0\nassets_3,0\nassets_4,0\n");
            File.AppendAllText(run,"handling_match_policy,numeric_abs1e-6_rel1e-5_flags_exact\n");
            File.WriteAllText(Path.Combine(directory,stem+".session.csv"),"execution_target,"+target+"\nclosed,1\nwrite_error,0\ndropped_rows,0\ncompatibility_dirty_at_start,0\nversion,0.8.5\nquest_workload_identity,abc\nenabled_bc,0\nenabled_astc,0\nenabled_etc2,0\n");
            File.WriteAllText(Path.Combine(directory,stem+".quest-profile"),"schema=1\nquest3=1\nhz=72\nwidth=1872\nheight=2016\nsetting_MsaaSamples=1\n");
            var sessionPath=Path.Combine(directory,stem+".session.csv");
            File.WriteAllText(sessionPath,File.ReadAllText(sessionPath).Replace("quest_workload_identity,abc","quest_workload_identity,"+BenchmarkReports.ProfileIdentity(File.ReadLines(Path.Combine(directory,stem+".quest-profile")))));
            var rows=new List<string> {"start_monotonic_ns,cpu_local_ms,frame_interval_ms,dropped_rows,replay_forward_view_active,paused,cutscene,renderdoc_capture_active,render_width,render_height,render_scale_effective,refresh_hz,active_world,active_vegetation,active_vehicles,active_peds,active_weapons,world_lod_mode,world_occlusion_enabled,world_backface_enabled,static_instances_enabled,lod_fade_transition_enabled,fxaa_integer_sampling_enabled,gpu_pass_start_monotonic_ns,gpu_pass_status,gpu_pass_frame_id,gpu_pass_frame_ms"};
            for(var i=0;i<80;++i) {
                var time=(1_000_000_000UL+(ulong)i*40_000_000UL).ToString(CultureInfo.InvariantCulture);
                // Repeated latest GPU rows must be deduplicated by source frame.
                rows.Add($"{time},{(i<25?999:2)},13,0,1,0,0,0,1872,2016,100,72,0,0,0,0,0,1,0,0,0,0,0,2000000000,1,25,3");
            }
            File.WriteAllLines(Path.Combine(directory,stem+".frames.csv"),rows);return run;
        }
        var left=Fixture("left","quest_android");var right=Fixture("right","windows_pcvr");
        await test("Matched benchmark excludes warmup and deduplicates delayed GPU samples",()=>{
            var result=BenchmarkReports.Compare(left,right);
            Check(result.WorkloadMatched && result.Left.Cpu.MeanMs==2 && result.Left.Cpu.Samples==55 && result.Left.SampledGpu?.Samples==1);
            return Task.CompletedTask;
        });
        await test("Benchmark tolerates sub-millihertz period quantization but rejects different refresh targets",()=>{
            var profile=Directory.GetFiles(Path.GetDirectoryName(right)!,"*.quest-profile").Single();var original=File.ReadAllText(profile);
            var session=Directory.GetFiles(Path.GetDirectoryName(right)!,"*.session.csv").Single();var metadata=File.ReadAllText(session);
            void Snapshot(string text){File.WriteAllText(profile,text);File.WriteAllText(session,System.Text.RegularExpressions.Regex.Replace(metadata,@"(?m)^quest_workload_identity,[^\r\n]*","quest_workload_identity,"+BenchmarkReports.ProfileIdentity(File.ReadLines(profile))));}
            Snapshot(original.Replace("hz=72","hz=72.00009"));Check(BenchmarkReports.Compare(left,right).WorkloadMatched);
            Snapshot(original.Replace("hz=72","hz=90"));Check(!BenchmarkReports.Compare(left,right).WorkloadMatched);
            Snapshot(original);return Task.CompletedTask;
        });
        await test("Benchmark rejects a profile snapshot modified after capture",()=>{
            var profile=Directory.GetFiles(Path.GetDirectoryName(right)!,"*.quest-profile").Single();var original=File.ReadAllText(profile);
            File.WriteAllText(profile,original.Replace("hz=72","hz=90"));
            try{BenchmarkReports.Compare(left,right);throw new Exception("Changed profile accepted");}catch(InvalidDataException){}
            File.WriteAllText(profile,original);return Task.CompletedTask;
        });
        await test("Dropped benchmark rows are attributed without claiming settings changed",()=>{
            var frames=Directory.GetFiles(Path.GetDirectoryName(right)!,"*.frames.csv").Single();var original=File.ReadAllText(frames);
            File.WriteAllText(frames,original.Replace(",13,0,1,0,0,0,",",13,1,1,0,0,0,"));
            var result=BenchmarkReports.Compare(left,right);
            Check(!result.WorkloadMatched && result.Rejections.Any(r=>r=="windows_pcvr: dropped recorder rows") && !result.Rejections.Any(r=>r.Contains("changed settings")));
            File.WriteAllText(frames,original);return Task.CompletedTask;
        });
        await test("Benchmark rejects different route/assets, dirty/incomplete captures and changed state",()=>{
            foreach(var change in new[]{("route_fingerprint,222","route_fingerprint,333"),("asset_fingerprint,111","asset_fingerprint,444"),("compatibility_dirty_at_end,0","compatibility_dirty_at_end,1"),("result,COMPLETE","result,ABORTED")}) {
                var text=File.ReadAllText(right);File.WriteAllText(right,text.Replace(change.Item1,change.Item2));
                Check(!BenchmarkReports.Compare(left,right).WorkloadMatched);File.WriteAllText(right,text);
            }
            var frame=Directory.GetFiles(Path.GetDirectoryName(right)!,"*.frames.csv").Single();
            var original=File.ReadAllText(frame);File.WriteAllText(frame,original.Replace(",1872,2016,",",1000,2016,"));
            Check(!BenchmarkReports.Compare(left,right).WorkloadMatched);File.WriteAllText(frame,original);
            var session=Directory.GetFiles(Path.GetDirectoryName(right)!,"*.session.csv").Single();
            File.AppendAllText(session,"write_error,1\n");
            try { BenchmarkReports.Compare(left,right);throw new Exception("Duplicate metadata accepted"); } catch(InvalidDataException) {}
            return Task.CompletedTask;
        });
        await test("PCVR profile is a separate optional launch argument and legacy native remains default",()=>{
            var paths=new PcvrPaths("unused","data","user");var profile=Path.Combine(root,"profile with spaces.quest-profile");File.WriteAllText(profile,"synthetic");
            var args=PcvrWorkflow.LaunchArguments(paths,0,profile);
            Check(args[^2]=="--quest-profile" && args[^1]==Path.GetFullPath(profile) && PcvrWorkflow.LaunchArguments(paths,0).Length==6);
            return Task.CompletedTask;
        });
        await test("Source identity changes with source content but ignores generated identity header",()=>{
            var repo=Path.Combine(root,"fingerprint-source");Directory.CreateDirectory(Path.Combine(repo,"overlay/src/xr"));
            var source=Path.Combine(repo,"overlay/test.cpp");File.WriteAllText(source,"first");
            var first=BenchmarkReports.SourceIdentity(repo);File.WriteAllText(Path.Combine(repo,"overlay/src/xr/SourceIdentity.h"),"generated");
            File.WriteAllText(Path.Combine(repo,"overlay/README.md"),"documentation only");
            Check(first==BenchmarkReports.SourceIdentity(repo));File.WriteAllText(source,"second");Check(first!=BenchmarkReports.SourceIdentity(repo));
            File.WriteAllText(source,"same line\n");var lf=BenchmarkReports.SourceIdentity(repo);
            File.WriteAllText(source,"same line\r\n");Check(lf==BenchmarkReports.SourceIdentity(repo));
            File.WriteAllText(Path.Combine(repo,"overlay/profile.inc"),"shared INI code");Check(lf!=BenchmarkReports.SourceIdentity(repo));
            return Task.CompletedTask;
        });
        await test("Benchmark collection allows only generated diagnostic filenames",()=>{
            Check(BenchmarkTransfer.AllowedName("20261109T123456Z-123.quest-profile"));
            Check(BenchmarkTransfer.AllowedName("20261109T123456Z-123.sim.csv"));
            foreach(var name in new[]{"gta3.img","GTA3sf1.b","../20261109T123456Z-123.sim.csv","device-id.quest-profile","20261109T123456Z-123.draws.csv"}) Check(!BenchmarkTransfer.AllowedName(name));
            var rows=BenchmarkTransfer.Listing("Row: 0 _display_name=20261109T123456Z-123.quest-profile, _size=123\nRow: 1 _display_name=gta3.img, _size=456\nRow: 2 _display_name=../20261109T123456Z-123.sim.csv, _size=1\nRow: 3 _display_name=20261109T123456Z-124.frames.csv, _size=999999999");
            Check(rows.Length==1 && rows[0].Size==123);
            return Task.CompletedTask;
        });
    }
}
