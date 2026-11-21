using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text;

namespace LibertyManager;

internal static class AstcTextureCache
{
    private const int Block = 6;
    private const int MaxDimension = 4096;
    private const int MaxDictionaryBytes = 64 * 1024 * 1024;
    private sealed record Texture(string Key, byte[] Source);
    private sealed record Encoded(int Width, int Height, bool Alpha, List<byte[]> Levels);

    public static string? FindEncoder(ManagerSettings settings)
    {
        var names = string.IsNullOrWhiteSpace(settings.AstcEncoder)
            ? new[] { "astcenc", "astcenc-sse2", "astcenc-neon" }
            : new[] { settings.AstcEncoder };
        return names.Select(Paths.FindExecutable).FirstOrDefault(path => path is not null);
    }

    public static async Task Build(string root, string encoder, ICommandRunner runner, PrivateLog log, CancellationToken token)
    {
        log.Write("Encoding Quest ASTC 6x6 textures locally. This can take several minutes.");
        using var scratch = new TemporaryDirectory(Path.GetDirectoryName(root)!);
        var seen = new HashSet<string>(StringComparer.Ordinal);
        var pending = new List<Texture>();
        var entries = new List<(string Key, long Offset, int Width, int Height, int Levels, bool Alpha)>();
        int nextProgress = 500;
        using (var writer = new BinaryWriter(File.Create(Path.Combine(scratch.Path, "cache.bin"))))
        {
            writer.Write("LCVASTC1"u8);
            writer.Write(0L);
            writer.Write(0);
            async Task Flush()
            {
                var encoded = new Encoded?[pending.Count];
                await Parallel.ForEachAsync(Enumerable.Range(0, pending.Count),
                    new ParallelOptions { MaxDegreeOfParallelism = Math.Clamp(Environment.ProcessorCount / 2, 1, 8), CancellationToken = token },
                    async (index, cancellation) =>
                    {
                        encoded[index] = await Encode(pending[index].Source, encoder, runner,
                            Path.Combine(scratch.Path, index.ToString()), cancellation);
                    });
                for (int i = 0; i < pending.Count; i++)
                {
                    if (encoded[i] is not { } texture) continue;
                    entries.Add((pending[i].Key, writer.BaseStream.Position, texture.Width, texture.Height, texture.Levels.Count, texture.Alpha));
                    foreach (var level in texture.Levels) writer.Write(level);
                }
                pending.Clear();
                if (entries.Count >= nextProgress)
                {
                    log.Write($"Encoded {entries.Count} unique Quest textures.");
                    nextProgress = entries.Count + 500;
                }
            }
            foreach (var dictionary in Dictionaries(root, token))
            {
                foreach (var texture in ReadTextures(dictionary))
                {
                    token.ThrowIfCancellationRequested();
                    if (!seen.Add(texture.Key)) continue;
                    pending.Add(texture);
                    if (pending.Count == 16) await Flush();
                }
            }
            await Flush();
            if (entries.Count == 0) throw new InvalidDataException("No compatible D3D8 textures were found for the ASTC cache.");
            if (entries.Count > 65536) throw new InvalidDataException("Quest ASTC cache has too many textures.");
            var indexOffset = writer.BaseStream.Position;
            foreach (var entry in entries)
            {
                writer.Write(Encoding.ASCII.GetBytes(entry.Key));
                writer.Write(entry.Offset);
                writer.Write((ushort)entry.Width);
                writer.Write((ushort)entry.Height);
                writer.Write((byte)entry.Levels);
                writer.Write(entry.Alpha);
            }
            writer.BaseStream.Position = 8;
            writer.Write(indexOffset);
            writer.Write(entries.Count);
        }
        token.ThrowIfCancellationRequested();
        File.Move(Path.Combine(scratch.Path, "cache.bin"), Path.Combine(root, "texture-cache-astc.bin"));
        log.Write($"Quest ASTC cache prepared with {entries.Count} unique textures. {seen.Count - entries.Count} unsupported textures will use the original loader.");
    }

    private sealed class TemporaryDirectory : IDisposable
    {
        public string Path { get; }
        public TemporaryDirectory(string parent)
        {
            Path = System.IO.Path.Combine(parent, ".astc-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(Path);
        }
        public void Dispose() => Directory.Delete(Path, recursive: true);
    }

    private static IEnumerable<byte[]> Dictionaries(string root, CancellationToken token)
    {
        foreach (var path in OriginalXboxModels.SafeFiles(root).Order(StringComparer.Ordinal))
        {
            token.ThrowIfCancellationRequested();
            var extension = Path.GetExtension(path);
            if (extension.Equals(".txd", StringComparison.OrdinalIgnoreCase))
            {
                using var file = File.OpenRead(path);
                if (file.Length <= MaxDictionaryBytes) yield return ReadBytes(file, (int)file.Length);
            }
            if (!extension.Equals(".img", StringComparison.OrdinalIgnoreCase)) continue;
            var directory = Path.ChangeExtension(path, ".dir");
            if (!File.Exists(directory)) continue;
            using var archive = File.OpenRead(path);
            using var index = File.OpenRead(directory);
            if (index.Length % 32 != 0) throw new InvalidDataException("Invalid IMG directory: " + Path.GetFileName(directory));
            var entry = new byte[32];
            while (index.Position < index.Length)
            {
                token.ThrowIfCancellationRequested();
                index.ReadExactly(entry);
                var name = Encoding.ASCII.GetString(entry, 8, 24).TrimEnd('\0');
                if (!name.EndsWith(".txd", StringComparison.OrdinalIgnoreCase)) continue;
                var offset = U32(entry, 0) * 2048L;
                var size = U32(entry, 4) * 2048L;
                if (size == 0 || offset + size > archive.Length)
                    throw new InvalidDataException("Invalid IMG texture entry: " + name);
                if (size > MaxDictionaryBytes) continue;
                archive.Position = offset;
                yield return ReadBytes(archive, (int)size);
            }
        }
    }

    private static byte[] ReadBytes(Stream file, int size)
    {
        var bytes = new byte[size];
        file.ReadExactly(bytes);
        return bytes;
    }

    // RenderWare texture dictionary -> texture native -> struct, each with a 12-byte chunk header.
    private static IEnumerable<Texture> ReadTextures(byte[] dictionary)
    {
        if (dictionary.Length < 12 || U32(dictionary, 0) != 0x16) yield break;
        var end = ChunkEnd(dictionary, 0, dictionary.Length);
        for (int offset = 12; offset < end;)
        {
            var next = ChunkEnd(dictionary, offset, end);
            if (U32(dictionary, offset) == 0x15)
            {
                for (int child = offset + 12; child < next;)
                {
                    var childEnd = ChunkEnd(dictionary, child, next);
                    if (U32(dictionary, child) == 1 && childEnd - child >= 100 && U32(dictionary, child + 12) == 8)
                    {
                        var bytes = dictionary[(child + 12)..childEnd];
                        yield return new Texture(Convert.ToHexString(SHA256.HashData(bytes)), bytes);
                        break;
                    }
                    child = childEnd;
                }
            }
            offset = next;
        }
    }

    private static int ChunkEnd(byte[] bytes, int offset, int end)
    {
        if (end - offset < 12 || U32(bytes, offset + 4) > (uint)(end - offset - 12))
            throw new InvalidDataException("Truncated RenderWare texture chunk.");
        return offset + 12 + (int)U32(bytes, offset + 4);
    }

    private static async Task<Encoded?> Encode(byte[] source, string encoder, ICommandRunner runner, string scratch, CancellationToken token)
    {
        int format = (int)U32(source, 72), width = U16(source, 80), height = U16(source, 82);
        int levels = source[85], compression = source[87], layout = format & 0xf00;
        bool paletted = (format & 0x6000) != 0, formatAlpha = layout is 0x100 or 0x300 or 0x500;
        int offset = 88 + ((format & 0x4000) != 0 ? 128 : paletted ? 1024 : 0);
        if (width is < 1 or > MaxDimension || height is < 1 or > MaxDimension || levels < 1 ||
            levels > System.Numerics.BitOperations.Log2((uint)Math.Max(width, height)) + 1 ||
            compression is not (0 or 1 or 3 or 5) ||
            (!paletted && compression == 0 && layout is not (0x100 or 0x200 or 0x300 or 0x400 or 0x500 or 0x600)))
            return null;

        var output = new List<byte[]>(levels);
        var alpha = compression != 0 && U32(source, 76) != 0;
        for (int level = 0; level < levels; level++)
        {
            token.ThrowIfCancellationRequested();
            if (source.Length - offset < 4) return null;
            var size = U32(source, offset);
            int w = Math.Max(1, width >> level), h = Math.Max(1, height >> level);
            int bpp = paletted || layout == 0x400 ? 1 : layout is 0x500 or 0x600 ? 4 : 2;
            int expected = compression == 0 ? w * h * bpp : ((w + 3) / 4) * ((h + 3) / 4) * (compression == 1 ? 8 : 16);
            offset += 4;
            if (size < expected || size > source.Length - offset) return null;
            var tga = new byte[18 + w * h * 4];
            tga[2] = 2;
            BinaryPrimitives.WriteUInt16LittleEndian(tga.AsSpan(12), (ushort)w);
            BinaryPrimitives.WriteUInt16LittleEndian(tga.AsSpan(14), (ushort)h);
            tga[16] = 32;
            tga[17] = 0x28; // top-left origin, eight alpha bits
            if (compression == 0) DecodeRaw(source, offset, tga, w * h, layout, bpp, paletted, formatAlpha);
            else DecodeDxt(source, offset, tga, w, h, compression);
            for (int i = 21; !alpha && i < tga.Length; i += 4) alpha = tga[i] != 255;
            offset += (int)size;
            var input = scratch + ".tga";
            var encoded = scratch + ".astc";
            await File.WriteAllBytesAsync(input, tga, token);
            File.Delete(encoded);
            await runner.Run(encoder, ["-cl", input, encoded, "6x6", "-medium", "-j", "1", "-silent"], showOutput: false, token: token);
            var bytes = await File.ReadAllBytesAsync(encoded, token);
            if (bytes.Length != 16 + ((w + Block - 1) / Block) * ((h + Block - 1) / Block) * 16 ||
                U32(bytes, 0) != 0x5CA1AB13 || bytes[4] != Block || bytes[5] != Block || bytes[6] != 1 ||
                U24(bytes, 7) != w || U24(bytes, 10) != h || U24(bytes, 13) != 1)
                throw new InvalidDataException("astcenc returned a texture with an unexpected format or size.");
            output.Add(bytes[16..]);
        }
        return new Encoded(width, height, alpha, output);
    }

    private static void DecodeRaw(byte[] source, int offset, byte[] tga, int count, int layout, int bpp, bool paletted, bool alpha)
    {
        for (int i = 0, dest = 18; i < count; i++, offset += bpp, dest += 4)
        {
            if (paletted)
            {
                int entry = source[offset];
                int start = 88 + entry * 4;
                if (start + 4 > 88 + (((U32(source, 72) & 0x4000) != 0) ? 128 : 1024))
                    throw new InvalidDataException("Texture palette index is out of range.");
                tga[dest] = source[start + 2];
                tga[dest + 1] = source[start + 1];
                tga[dest + 2] = source[start];
                tga[dest + 3] = alpha ? source[start + 3] : (byte)255;
            }
            else if (bpp == 4)
            {
                Array.Copy(source, offset, tga, dest, 3);
                tga[dest + 3] = alpha ? source[offset + 3] : (byte)255;
            }
            else if (bpp == 1)
            {
                tga[dest] = tga[dest + 1] = tga[dest + 2] = source[offset];
                tga[dest + 3] = 255;
            }
            else
            {
                int c = U16(source, offset);
                uint pixel = layout switch
                {
                    0x100 => Pixel(Scale(c, 0, 31), Scale(c, 5, 31), Scale(c, 10, 31), (c >> 15) * 255),
                    0x200 => Color565(c),
                    _ => Pixel(Scale(c, 0, 15), Scale(c, 4, 15), Scale(c, 8, 15), Scale(c, 12, 15))
                };
                BinaryPrimitives.WriteUInt32LittleEndian(tga.AsSpan(dest), pixel);
            }
        }
    }

    private static void DecodeDxt(byte[] source, int offset, byte[] tga, int width, int height, int compression)
    {
        Span<uint> colors = stackalloc uint[4];
        Span<int> alphas = stackalloc int[8];
        for (int y = 0; y < height; y += 4)
        for (int x = 0; x < width; x += 4, offset += compression == 1 ? 8 : 16)
        {
            int colorStart = offset + (compression == 1 ? 0 : 8);
            int first = U16(source, colorStart), second = U16(source, colorStart + 2);
            colors[0] = Color565(first);
            colors[1] = Color565(second);
            bool threeColors = compression == 1 && first <= second;
            colors[2] = Blend(colors[0], colors[1], threeColors ? 1 : 2, threeColors ? 2 : 3);
            colors[3] = threeColors ? 0 : Blend(colors[0], colors[1], 1, 3);
            uint selectors = U32(source, colorStart + 4);
            ulong alphaSelectors = compression == 1 ? 0 : BinaryPrimitives.ReadUInt64LittleEndian(source.AsSpan(offset));
            if (compression == 5)
            {
                alphas[0] = source[offset];
                alphas[1] = source[offset + 1];
                int count = alphas[0] > alphas[1] ? 8 : 6;
                for (int i = 2; i < count; i++)
                    alphas[i] = ((count - i) * alphas[0] + (i - 1) * alphas[1]) / (count - 1);
                if (count == 6) { alphas[6] = 0; alphas[7] = 255; }
                alphaSelectors >>= 16;
            }
            for (int row = 0; row < Math.Min(4, height - y); row++)
            for (int column = 0; column < Math.Min(4, width - x); column++)
            {
                int index = row * 4 + column;
                uint color = colors[(int)(selectors >> (index * 2) & 3)];
                if (compression == 3) color = (color & 0xffffff) | (uint)((alphaSelectors >> (index * 4) & 15) * 17) << 24;
                if (compression == 5) color = (color & 0xffffff) | (uint)alphas[(int)(alphaSelectors >> (index * 3) & 7)] << 24;
                BinaryPrimitives.WriteUInt32LittleEndian(tga.AsSpan(18 + ((y + row) * width + x + column) * 4), color);
            }
        }
    }

    private static int Scale(int value, int shift, int mask) => ((value >> shift) & mask) * 255 / mask;
    private static uint Pixel(int b, int g, int r, int a) => (uint)b | (uint)g << 8 | (uint)r << 16 | (uint)a << 24;
    private static uint Color565(int color) => Pixel(Scale(color, 0, 31), Scale(color, 5, 63), Scale(color, 11, 31), 255);
    private static uint Blend(uint a, uint b, int weight, int divisor)
    {
        int Channel(int shift) => ((int)(a >> shift & 255) * weight + (int)(b >> shift & 255) * (divisor - weight)) / divisor;
        return Pixel(Channel(0), Channel(8), Channel(16), 255);
    }
    private static int U16(byte[] bytes, int offset) => BinaryPrimitives.ReadUInt16LittleEndian(bytes.AsSpan(offset));
    private static uint U32(byte[] bytes, int offset) => BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(offset));
    private static int U24(byte[] bytes, int offset) =>
        bytes[offset] | bytes[offset + 1] << 8 | bytes[offset + 2] << 16;
}
