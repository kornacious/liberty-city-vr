using System.Buffers.Binary;
using LibertyManager;

internal static class XboxAudioChecks
{
    public static async Task Run(Func<string, Func<Task>, Task> test, Action<bool, string> assert, string root)
    {
        byte[] Table()
        {
            var bytes = new byte[3032 * 20];
            for (var i = 0; i < 3032; i++)
            {
                var record = bytes.AsSpan(i * 20);
                BinaryPrimitives.WriteUInt32LittleEndian(record, (uint)(i * 72));
                BinaryPrimitives.WriteUInt32LittleEndian(record[4..], 72);
                BinaryPrimitives.WriteUInt32LittleEndian(record[8..], i == 6 ? 32000u : 12000u);
                BinaryPrimitives.WriteUInt32LittleEndian(record[12..], 36);
                BinaryPrimitives.WriteInt32LittleEndian(record[16..], i == 0 ? -1 : 72);
            }
            return bytes;
        }
        await test("Xbox sound bank conversion preserves sample indices, frequencies and loop semantics", () =>
        {
            var source = Table(); var converted = OriginalXboxAudio.ConvertTable(source, 3032 * 72);
            assert(BinaryPrimitives.ReadUInt32LittleEndian(converted.AsSpan(6 * 20)) == 6 * 256, "PCM address of higher-rate horn");
            assert(BinaryPrimitives.ReadUInt32LittleEndian(converted.AsSpan(6 * 20 + 8)) == 32000, "Authored horn rate retained");
            assert(BinaryPrimitives.ReadUInt32LittleEndian(converted.AsSpan(12)) == 128, "Loop start decoded from block offset");
            assert(BinaryPrimitives.ReadInt32LittleEndian(converted.AsSpan(16)) == -1, "No-loop sentinel retained");
            assert(BinaryPrimitives.ReadInt32LittleEndian(converted.AsSpan(36)) == 256, "Loop end decoded to PCM bytes");
            assert(BinaryPrimitives.ReadUInt32LittleEndian(source.AsSpan(4)) == 72, "Input table stays unchanged");
            return Task.CompletedTask;
        });
        await test("Xbox sound bank rejects malformed indices, unaligned loops, gaps and trailing data", () =>
        {
            void Reject(byte[] bytes, long length)
            {
                try { OriginalXboxAudio.ConvertTable(bytes, length); }
                catch (InvalidDataException) { return; }
                throw new Exception("Malformed bank accepted");
            }
            Reject(new byte[20], 72);
            var bytes = Table(); BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(20), 36); Reject(bytes, 3032 * 72);
            bytes = Table(); BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(12), 1); Reject(bytes, 3032 * 72);
            bytes = Table(); BinaryPrimitives.WriteInt32LittleEndian(bytes.AsSpan(16), 108); Reject(bytes, 3032 * 72);
            Reject(Table(), 3032 * 72 + 36);
            return Task.CompletedTask;
        });
        await test("Xbox audio inventory includes all stations, speech and the bank pair without stem collisions", () =>
        {
            var paths = OriginalXboxAudio.RequiredFiles.ToArray();
            assert(paths.Length == 198 && paths.Distinct(StringComparer.OrdinalIgnoreCase).Count() == 198, "Complete unique audio output inventory");
            assert(paths.Contains("audio/head.flac") && paths.Contains("audio/a1_sso.flac") && paths.Contains("audio/sfx.raw"), "Radio, speech and effects covered");
            return Task.CompletedTask;
        });
        await test("Xbox stereo WAV validation counts ADPCM frames instead of container duration estimates", () =>
        {
            var path = Path.Combine(root, "xbox-stereo.wav");
            using (var writer = new BinaryWriter(File.Create(path)))
            {
                writer.Write("RIFF"u8); writer.Write(184u); writer.Write("WAVEfmt "u8); writer.Write(20u);
                writer.Write((ushort)0x69); writer.Write((ushort)2); writer.Write(44100u); writer.Write(49612u);
                writer.Write((ushort)72); writer.Write((ushort)4); writer.Write((ushort)2); writer.Write((ushort)64);
                writer.Write("data"u8); writer.Write(144u); writer.Write(new byte[144]);
            }
            assert(Math.Abs(OriginalXboxAudio.WaveDuration(path, 44100, 2) - 128.0 / 44100) < 1e-10, "Two stereo ADPCM blocks contain 128 frames");
            using (var file = File.OpenWrite(path)) file.SetLength(file.Length - 1);
            try { OriginalXboxAudio.WaveDuration(path, 44100, 2); }
            catch (InvalidDataException) { return Task.CompletedTask; }
            throw new Exception("Truncated Xbox stereo audio accepted");
        });
    }
}
