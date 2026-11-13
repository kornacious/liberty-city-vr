using System.Buffers.Binary;
using System.Text;

namespace LibertyManager;

public static class GxtMerge
{
    public static SortedDictionary<string,byte[]> Read(byte[] bytes)
    {
        byte[]? keys=null,text=null;
        for(int p=0;p<bytes.Length;) {
            if(p+8>bytes.Length)throw new InvalidDataException("Truncated GXT header.");
            int size=BinaryPrimitives.ReadInt32LittleEndian(bytes.AsSpan(p+4));var type=Encoding.ASCII.GetString(bytes,p,4);p+=8;
            if(size<0||size>bytes.Length-p)throw new InvalidDataException("Invalid GXT section.");
            if(type=="TKEY")keys=bytes.AsSpan(p,size).ToArray();if(type=="TDAT")text=bytes.AsSpan(p,size).ToArray();p+=size;
        }
        if(keys is null||text is null||keys.Length%12!=0)throw new InvalidDataException("Invalid GXT sections.");
        var result=new SortedDictionary<string,byte[]>(StringComparer.Ordinal);
        for(int p=0;p<keys.Length;p+=12) {
            int offset=BinaryPrimitives.ReadInt32LittleEndian(keys.AsSpan(p));
            if(offset<0||offset>=text.Length||offset%2!=0)throw new InvalidDataException("Invalid GXT offset.");
            int end=offset;while(end+1<text.Length&&(text[end]!=0||text[end+1]!=0))end+=2;
            if(end+1>=text.Length)throw new InvalidDataException("Unterminated GXT string.");
            var key=Encoding.ASCII.GetString(keys,p+4,8).Split('\0')[0];if(!result.TryAdd(key,text.AsSpan(offset,end+2-offset).ToArray()))throw new InvalidDataException("Duplicate GXT key.");
        }
        return result;
    }
    public static void Merge(string target,string additions)
    {
        var values=Read(File.ReadAllBytes(target));foreach(var (key,value) in Read(File.ReadAllBytes(additions)))values.TryAdd(key,value);
        using var data=new MemoryStream();using var keys=new MemoryStream();using(var writer=new BinaryWriter(keys,Encoding.ASCII,true))
        foreach(var (key,value) in values) {writer.Write(checked((int)data.Length));var name=new byte[8];Encoding.ASCII.GetBytes(key).CopyTo(name,0);writer.Write(name);data.Write(value);}
        using var result=new MemoryStream();using(var writer=new BinaryWriter(result,Encoding.ASCII,true)) {
            writer.Write("TKEY"u8);writer.Write(checked((int)keys.Length));writer.Write(keys.ToArray());writer.Write("TDAT"u8);writer.Write(checked((int)data.Length));writer.Write(data.ToArray());
        }
        if(result.Length>0x40000)throw new InvalidDataException("Merged GXT exceeds the engine text buffer.");
        File.WriteAllBytes(target,result.ToArray());
    }
}
