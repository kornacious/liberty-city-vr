using System.Text;
using LibertyManager;

internal static class VariantChecks
{
    public static async Task Run(Func<string,Func<Task>,Task> test,Action<bool> check,string root)
    {
        var fixture=Path.Combine(root,"complete-variants");Directory.CreateDirectory(fixture);
        string Create(string id,string name) {
            var path=Path.Combine(fixture,name);Directory.CreateDirectory(path);
            foreach(var relative in VariantInstaller.Required) {var p=Paths.Child(path,relative);Directory.CreateDirectory(Path.GetDirectoryName(p)!);File.WriteAllText(p,id+relative);}
            VariantInstaller.Seal(path,id);
            check(!File.ReadAllBytes(Path.Combine(path,VariantInstaller.Marker)).Contains((byte)'\r'));
            return path;
        }
        await test("Complete variants coexist and replacement preserves other assets and personal state",()=>{
            var pc=Create("pc","source-pc");var xbox=Create("xbox","source-xbox");var destination=Path.Combine(fixture,"installed");Directory.CreateDirectory(destination);
            File.WriteAllText(Path.Combine(destination,"autosave.b"),"save");File.WriteAllText(Path.Combine(destination,"vr_settings.ini"),"selection");
            VariantInstaller.InstallLocal(destination,"pc",pc,default);VariantInstaller.InstallLocal(destination,"xbox",xbox,default);
            var oldXbox=File.ReadAllText(Path.Combine(destination,"variants/xbox/data/main.scm"));
            VariantInstaller.InstallLocal(destination,"pc",pc,default);
            check(File.ReadAllText(Path.Combine(destination,"variants/xbox/data/main.scm"))==oldXbox);
            check(File.ReadAllText(Path.Combine(destination,"autosave.b"))=="save"&&File.ReadAllText(Path.Combine(destination,"vr_settings.ini"))=="selection");return Task.CompletedTask;
        });
        await test("Variant checksum failure and cancellation cannot replace an installed variant",()=>{
            var source=Create("pc","bad-source");var destination=Path.Combine(fixture,"protected");VariantInstaller.InstallLocal(destination,"pc",source,default);
            var installed=Path.Combine(destination,"variants/pc/data/main.scm");var before=File.ReadAllText(installed);
            File.AppendAllText(Path.Combine(source,"data/main.scm"),"changed");
            try{VariantInstaller.InstallLocal(destination,"pc",source,default);throw new Exception("Accepted damaged variant");}catch(InvalidDataException){}
            check(File.ReadAllText(installed)==before);
            var valid=Create("pc","cancel-source");using var stop=new CancellationTokenSource();stop.Cancel();
            try{VariantInstaller.InstallLocal(destination,"pc",valid,stop.Token);throw new Exception("Ignored cancellation");}catch(OperationCanceledException){}
            check(File.ReadAllText(installed)==before);return Task.CompletedTask;
        });
        await test("Variant manifests reject missing required and unmanifested files",()=>{
            var source=Create("xbox","inventory-source");File.WriteAllText(Path.Combine(source,"extra.txt"),"extra");
            try{VariantInstaller.Validate(source,"xbox");throw new Exception("Accepted extra file");}catch(InvalidDataException){}
            File.Delete(Path.Combine(source,"models/frontend.txd"));
            try{VariantInstaller.Seal(source,"xbox");throw new Exception("Accepted missing frontend");}catch(InvalidDataException){}
            return Task.CompletedTask;
        });
        byte[] Gxt(params (string Key,string Value)[] entries) {
            using var keys=new MemoryStream();using var data=new MemoryStream();using var k=new BinaryWriter(keys,Encoding.ASCII,true);
            foreach(var (key,value) in entries){k.Write((int)data.Length);var n=new byte[8];Encoding.ASCII.GetBytes(key).CopyTo(n,0);k.Write(n);data.Write(Encoding.Unicode.GetBytes(value+"\0"));}
            using var result=new MemoryStream();using var w=new BinaryWriter(result);w.Write("TKEY"u8);w.Write((int)keys.Length);w.Write(keys.ToArray());w.Write("TDAT"u8);w.Write((int)data.Length);w.Write(data.ToArray());return result.ToArray();
        }
        await test("GXT merge preserves Xbox strings while adding missing port menu labels",()=>{
            var target=Path.Combine(fixture,"xbox.gxt");var port=Path.Combine(fixture,"port.gxt");File.WriteAllBytes(target,Gxt(("HELLO","Xbox text")));File.WriteAllBytes(port,Gxt(("HELLO","PC text"),("FE_TEST","Port setting")));
            GxtMerge.Merge(target,port);var result=GxtMerge.Read(File.ReadAllBytes(target));check(result.Count==2);
            check(Encoding.Unicode.GetString(result["HELLO"])=="Xbox text\0"&&Encoding.Unicode.GetString(result["FE_TEST"])=="Port setting\0");
            var before=File.ReadAllBytes(target);File.WriteAllBytes(port,[1,2,3]);
            try{GxtMerge.Merge(target,port);throw new Exception("Accepted malformed GXT");}catch(InvalidDataException){}
            check(File.ReadAllBytes(target).SequenceEqual(before));return Task.CompletedTask;
        });
    }
}
