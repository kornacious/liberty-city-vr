using System.Security.Cryptography;
using System.Text;
using LibertyManager;

internal static class BenchmarkAssetChecks
{
    public static async Task Run(string root,Func<string,Func<Task>,Task> test)
    {
        void Check(bool value){if(!value)throw new Exception("Benchmark asset assertion failed.");}
        BenchmarkAsset Asset(string path,string value)=>new(path,Encoding.UTF8.GetByteCount(value),Convert.ToHexStringLower(SHA256.HashData(Encoding.UTF8.GetBytes(value))),BenchmarkAssets.Token(path));
        await test("Benchmark asset inventory rejects escapes, ambiguous case and malformed records",()=>{
            var asset=Asset("data/handling.cfg","abc");
            var row=$"Row: 0 path_b64={asset.Token}, _size={asset.Size}, sha256={asset.Sha256}";
            Check(BenchmarkAssets.Parse(row).Single()==asset);
            foreach(var bad in new[]{"../saves/GTA3sf1.b","data/../models/test.dff","data/CON.txt","data/file.","audio/test.wav","data/file:stream","data\\file"}) {
                try{BenchmarkAssets.ValidatePath(bad);throw new Exception("Invalid path accepted");}catch(InvalidDataException){}
            }
            foreach(var bad in new[]{row+"\n"+row,"Error accessing provider",row.Replace("_size=3","_size=999999999999")}) {
                try{BenchmarkAssets.Parse(bad);throw new Exception("Invalid inventory accepted");}catch(InvalidDataException){}
            }
            return Task.CompletedTask;
        });
        var baseRoot=Path.Combine(root,"benchmark-alignment");Directory.CreateDirectory(baseRoot);
        var settings=new ManagerSettings {Workplace=Path.Combine(baseRoot,"work"),Repository=Path.Combine(baseRoot,"repo"),GameDirectory=Path.Combine(baseRoot,"retail")};
        var paths=PcvrPaths.From(settings);Directory.CreateDirectory(Path.Combine(paths.Data,"data"));Directory.CreateDirectory(Path.Combine(paths.Data,"models"));Directory.CreateDirectory(paths.User);
        File.WriteAllText(Path.Combine(paths.Data,".libertyvr-pcvr-data"),"fixture");
        File.WriteAllText(Path.Combine(paths.Data,"data/handling.cfg"),"old");File.WriteAllText(Path.Combine(paths.Data,"models/test.dff"),"same");
        File.WriteAllText(Path.Combine(paths.Data,"models/pc-only.dff"),"extra");File.WriteAllText(Path.Combine(paths.User,"GTA3sf1.b"),"save unchanged");
        var reference=new[]{Asset("data/handling.cfg","new"),Asset("models/test.dff","same")};
        await test("Quest-aligned benchmark copies are verified and preserve original data and saves",async()=>{
            var local=BenchmarkAssets.LocalInventory(paths.Data);Check(BenchmarkAssets.Compare(reference,local).Length==2);
            var calls=0;
            var setup=await BenchmarkAssets.Prepare(settings,Path.Combine(baseRoot,"aligned"),reference,(asset,path,token)=>{
                ++calls;File.WriteAllText(path,"new");return Task.CompletedTask;
            },CancellationToken.None);
            Check(calls==1 && setup.LocalCopies==1 && setup.DeviceCopies==1);
            Check(BenchmarkAssets.Compare(reference,BenchmarkAssets.LocalInventory(setup.Data)).Length==0);
            Check(!File.Exists(Path.Combine(setup.Data,"models/pc-only.dff")));
            Check(File.ReadAllText(Path.Combine(paths.Data,"data/handling.cfg"))=="old" && File.Exists(Path.Combine(paths.Data,"models/pc-only.dff")));
            Check(File.ReadAllText(Path.Combine(paths.User,"GTA3sf1.b"))=="save unchanged" && File.ReadAllText(Path.Combine(setup.User,"GTA3sf1.b"))=="save unchanged");
            Check(File.Exists(Path.Combine(baseRoot,"aligned/benchmark.json")) && !File.Exists(Path.Combine(baseRoot,"aligned/INCOMPLETE.txt")));
        });
        await test("Failed benchmark copying never publishes prepared data or replaces existing outputs",async()=>{
            var failed=Path.Combine(baseRoot,"failed");
            try{await BenchmarkAssets.Prepare(settings,failed,reference,(asset,path,token)=>{File.WriteAllText(path,"wrong");return Task.CompletedTask;},CancellationToken.None);throw new Exception("Bad copy accepted");}
            catch(IOException){}
            Check(File.Exists(Path.Combine(failed,"INCOMPLETE.txt")) && !File.Exists(Path.Combine(failed,"data/.libertyvr-pcvr-data")));
            foreach(var target in new[]{Path.Combine(baseRoot,"aligned"),paths.Data,Path.Combine(settings.Repository,"benchmark")}) {
                try{await BenchmarkAssets.Prepare(settings,target,reference,(_,_,_)=>throw new Exception("Unexpected transfer"),CancellationToken.None);throw new Exception("Unsafe destination accepted");}
                catch(Exception error) when(error is IOException or InvalidDataException){}
            }
        });
    }
}
