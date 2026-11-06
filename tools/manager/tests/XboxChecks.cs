using System.Buffers.Binary;
using System.Text;
using LibertyManager;

internal static class XboxChecks
{
    public static async Task Run(Func<string, Func<Task>, Task> test, Action<bool, string> assert, string root)
    {
        byte[] Image(long partition = 0, string name = "default.xbe")
        {
            var bytes = new byte[checked((int)partition + 36 * 2048)];
            var descriptor = (int)partition + 32 * 2048;
            "MICROSOFT*XBOX*MEDIA"u8.CopyTo(bytes.AsSpan(descriptor));
            "MICROSOFT*XBOX*MEDIA"u8.CopyTo(bytes.AsSpan(descriptor + 2028));
            BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(descriptor + 20), 33);
            BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(descriptor + 24), 2048);
            var entry = (int)partition + 33 * 2048;
            BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(entry + 4), 34);
            BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(entry + 8), 4);
            bytes[entry + 12] = 32; bytes[entry + 13] = (byte)name.Length;
            Encoding.ASCII.GetBytes(name).CopyTo(bytes, entry + 14);
            "test"u8.CopyTo(bytes.AsSpan((int)partition + 34 * 2048));
            return bytes;
        }
        void Reject(byte[] bytes)
        {
            var path = Path.Combine(root, "invalid.xiso.iso"); File.WriteAllBytes(path, bytes);
            try { using var iso = new XboxIso(path); }
            catch (InvalidDataException) { return; }
            throw new Exception("Malformed Xbox image was accepted.");
        }
        await test("Read-only XISO reader extracts exact bounded file bytes", () =>
        {
            var path = Path.Combine(root, "fixture.xiso.iso"); var bytes = Image(); File.WriteAllBytes(path, bytes);
            using (var iso = new XboxIso(path))
            {
                assert(iso.Files.Count == 1 && iso.Files.ContainsKey("DEFAULT.XBE"), "Case-insensitive Xbox catalogue");
                var output = Path.Combine(root, "extracted/test"); iso.Extract("default.xbe", output, default);
                assert(File.ReadAllText(output) == "test", "Exact file length without sector padding");
            }
            assert(File.ReadAllBytes(path).SequenceEqual(bytes), "Source ISO must remain unchanged");
            return Task.CompletedTask;
        });
        await test("Xbox image reader rejects traversal, recursive trees, bounds and bad descriptors", () =>
        {
            Reject(Image(name: "../bad"));
            var bytes = Image(); bytes[33 * 2048] = 1; Reject(bytes);
            bytes = Image(); BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(33 * 2048 + 4), uint.MaxValue); Reject(bytes);
            bytes = Image(); bytes[32 * 2048 + 2028] = 0; Reject(bytes);
            bytes = Image(); bytes[33 * 2048 + 12] = 16; BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(33 * 2048 + 4), 33); Reject(bytes);
            return Task.CompletedTask;
        });
        await test("Full Xbox DVD partition is accepted independently of XISO layout", () =>
        {
            const long partition = 0x18300000;
            var bytes = Image(); var path = Path.Combine(root, "full-disc.iso");
            try
            {
                using (var file = File.Create(path))
                { file.SetLength(partition + bytes.Length); file.Position = partition; file.Write(bytes); }
                using var iso = new XboxIso(path);
                assert(iso.Files["default.xbe"].Offset == partition + 34 * 2048, "Full-disc extents use partition-relative sectors");
            }
            finally { File.Delete(path); }
            return Task.CompletedTask;
        });
        await test("Unsupported Xbox disc fails before converter or headset access and preserves prior output", async () =>
        {
            var path = Path.Combine(root, "unsupported.iso"); File.WriteAllBytes(path, Image());
            var output = Path.Combine(root, "xbox-work/modelsets/modern"); Directory.CreateDirectory(output);
            File.WriteAllText(Path.Combine(output, "BUILD_INFO.txt"), "Profile=MODERN\n"); File.WriteAllText(Path.Combine(output, "keep.txt"), "previous");
            var fake = new NoTools(); using var log = new PrivateLog();
            var settings = new ManagerSettings { Repository = Path.Combine(root, "source"), Workplace = Path.Combine(root, "xbox-work") };
            try { await new OriginalXboxModels(settings, fake, log).Prepare(new() { ModelArchive = path, ModelOutput = output }, default); }
            catch (InvalidDataException)
            {
                assert(File.ReadAllText(Path.Combine(output, "keep.txt")) == "previous", "Failed import preserves old profile");
                assert(!Directory.EnumerateDirectories(Path.GetDirectoryName(output)!).Any(p => Path.GetFileName(p).StartsWith('.')), "Failed import cleans temporary assets");
                return;
            }
            throw new Exception("Unsupported disc accepted.");
        });
    }
    private sealed class NoTools : ICommandRunner
    {
        public Task<CommandResult> Run(string executable, IEnumerable<string> args, string? workingDirectory = null, bool showOutput = true, bool check = true, CancellationToken token = default, string? input = null)
            => throw new Exception("Unexpected tool, network or device invocation");
    }
}
