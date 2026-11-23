using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text;

namespace LibertyManager;

public sealed record XboxIsoFile(string Name, long Offset, long Length);

/// <summary>Bounded, read-only XDVDFS reader for full Xbox DVDs and trimmed XISOs.</summary>
public sealed class XboxIso : IDisposable
{
    private readonly FileStream stream;
    private readonly long partition;
    public Dictionary<string, XboxIsoFile> Files { get; } = new(StringComparer.OrdinalIgnoreCase);
    private readonly HashSet<long> directories = [];
    public XboxIso(string path)
    {
        Paths.NoLinks(path);
        if (!path.EndsWith(".iso", StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Choose a local .iso or .xiso.iso file.");
        stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
        try
        {
            foreach (var candidate in new long[] { 0, 0x18300000, 0xFD90000, 0x2080000 })
            {
                if (candidate + 65536 + 2048 > stream.Length) continue;
                var descriptor = Read(candidate + 65536, 2048);
                if (!descriptor.AsSpan(0, 20).SequenceEqual("MICROSOFT*XBOX*MEDIA"u8) ||
                    !descriptor.AsSpan(2028, 20).SequenceEqual("MICROSOFT*XBOX*MEDIA"u8)) continue;
                partition = candidate;
                Directory(U32(descriptor, 20), U32(descriptor, 24), "", 0);
                if (Files.Count == 0) throw new InvalidDataException("Xbox image contains no files.");
                return;
            }
            throw new InvalidDataException("No supported Xbox game partition found in the supplied image.");
        }
        catch { stream.Dispose(); throw; }
    }
    private static uint U32(byte[] bytes, int offset) => BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(offset, 4));
    private void Bounds(long offset, long length)
    {
        if (offset < 0 || length < 0 || offset > stream.Length || length > stream.Length - offset)
            throw new InvalidDataException("Xbox image extent lies outside the file.");
    }
    private byte[] Read(long offset, int length)
    {
        Bounds(offset, length); stream.Position = offset;
        var bytes = new byte[length]; stream.ReadExactly(bytes); return bytes;
    }
    private void Directory(uint sector, uint size, string parent, int depth)
    {
        var offset = partition + sector * 2048L;
        if (depth > 16 || size > 8 * 1024 * 1024 || !directories.Add(offset))
            throw new InvalidDataException("Invalid or recursive Xbox directory.");
        if (size == 0) return;
        var bytes = Read(offset, (int)size); var visited = new HashSet<int>();
        void Node(int entry, int treeDepth)
        {
            if (treeDepth > 256 || entry < 0 || entry > bytes.Length - 14 || !visited.Add(entry))
                throw new InvalidDataException("Invalid Xbox directory tree.");
            var nameLength = bytes[entry + 13];
            if (nameLength == 0 || entry + 14 + nameLength > bytes.Length) throw new InvalidDataException("Truncated Xbox filename.");
            var nameBytes = bytes.AsSpan(entry + 14, nameLength);
            foreach (var c in nameBytes)
                if (c < 32 || c > 126 || c is (byte)'/' or (byte)'\\' or (byte)':') throw new InvalidDataException("Unsafe Xbox filename.");
            var name = Encoding.ASCII.GetString(nameBytes).ToLowerInvariant();
            if (name is "." or ".." || name.EndsWith('.') || name.EndsWith(' ')) throw new InvalidDataException("Unsafe Xbox filename.");
            var relative = parent.Length == 0 ? name : parent + "/" + name;
            var left = BinaryPrimitives.ReadUInt16LittleEndian(bytes.AsSpan(entry, 2));
            var right = BinaryPrimitives.ReadUInt16LittleEndian(bytes.AsSpan(entry + 2, 2));
            if (left != 0) Node(left * 4, treeDepth + 1);
            var start = partition + U32(bytes, entry + 4) * 2048L;
            var length = U32(bytes, entry + 8); Bounds(start, length);
            if ((bytes[entry + 12] & 16) != 0) Directory(U32(bytes, entry + 4), length, relative, depth + 1);
            else if (Files.Count >= 20000 || !Files.TryAdd(relative, new(relative, start, length)))
                throw new InvalidDataException("Duplicate or excessive Xbox files.");
            if (right != 0) Node(right * 4, treeDepth + 1);
        }
        Node(0, 0);
    }
    public void RequireNorthAmericanGta3()
    {
        const int headerBytes = 0x11C;
        const int certificateBytes = 0xA4;
        const uint gta3TitleId = 0x5454000E;
        const uint northAmerica = 1;
        const string error = "Unverified import requires an Original Xbox GTA III executable marked for the US / Canada region.";
        if (!Files.TryGetValue("default.xbe", out var file) || file.Length < headerBytes)
            throw new InvalidDataException(error);
        var header = Read(file.Offset, headerBytes);
        var headersSize = U32(header, 0x108);
        var certificateOffset = (long)U32(header, 0x118) - U32(header, 0x104);
        if (!header.AsSpan(0, 4).SequenceEqual("XBEH"u8) || headersSize > file.Length ||
            certificateOffset < headerBytes || certificateOffset + certificateBytes > headersSize)
            throw new InvalidDataException(error);
        var certificate = Read(file.Offset + certificateOffset, certificateBytes);
        var size = U32(certificate, 0);
        if (size < certificateBytes || certificateOffset + size > headersSize ||
            U32(certificate, 8) != gta3TitleId || U32(certificate, 0xA0) != northAmerica)
            throw new InvalidDataException(error);
    }

    public void Extract(string relative, string target, CancellationToken token)
    {
        var file = Files[relative];
        Paths.NoLinks(target);
        System.IO.Directory.CreateDirectory(Path.GetDirectoryName(target)!);
        using var output = new FileStream(target, FileMode.CreateNew, FileAccess.Write);
        ReadFile(file, output, token);
    }

    public string Sha256(XboxIsoFile file, CancellationToken token)
    {
        using var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        ReadFile(file, Stream.Null, token, hash);
        return Convert.ToHexString(hash.GetHashAndReset());
    }

    private void ReadFile(XboxIsoFile file, Stream output, CancellationToken token, IncrementalHash? hash = null)
    {
        token.ThrowIfCancellationRequested();
        Bounds(file.Offset, file.Length);
        stream.Position = file.Offset;
        var remaining = file.Length;
        var buffer = System.Buffers.ArrayPool<byte>.Shared.Rent((int)Math.Clamp(remaining, 1, 1024 * 1024));
        try
        {
            while (remaining > 0)
            {
                token.ThrowIfCancellationRequested();
                var n = stream.Read(buffer, 0, (int)Math.Min(buffer.Length, remaining));
                if (n == 0) throw new EndOfStreamException("Truncated Xbox image.");
                output.Write(buffer, 0, n);
                hash?.AppendData(buffer, 0, n);
                remaining -= n;
            }
        }
        finally { System.Buffers.ArrayPool<byte>.Shared.Return(buffer); }
    }
    public void Dispose() => stream.Dispose();
}
