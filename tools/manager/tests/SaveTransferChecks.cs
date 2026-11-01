using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using LibertyManager;

internal static class SaveTransferChecks
{
    private static void Check(bool condition) { if (!condition) throw new Exception("Save transfer assertion failed."); }
    private static async Task Reject<T>(Func<Task> operation) where T : Exception
    { try { await operation(); } catch (T) { return; } throw new Exception("Expected " + typeof(T).Name); }
    internal static byte[] Save(int marker = 17)
    {
        using var file = new MemoryStream(); using var writer = new BinaryWriter(file);
        var payload = 0;
        for (var i = 0; i < 20; i++)
        {
            var bytes = new byte[i == 0 ? 196 : i == 2 ? 5488 : 8];
            if (i == 0)
            {
                BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(64), 201729);
                BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(188), 4);
                bytes[192] = (byte)marker;
            }
            else BinaryPrimitives.WriteUInt32LittleEndian(bytes, (uint)bytes.Length - 4);
            writer.Write(bytes.Length); writer.Write(bytes); payload += bytes.Length;
        }
        while (payload < LibertySave.PayloadBytes)
        {
            var size = Math.Min(55000, LibertySave.PayloadBytes - payload);
            writer.Write(size); writer.Write(new byte[size]); payload += size;
        }
        writer.Write(0U); var result = file.ToArray(); Checksum(result); return result;
    }
    private static void Checksum(byte[] data)
    { uint sum = 0; foreach (var b in data.AsSpan(0, data.Length - 4)) sum += b; BinaryPrimitives.WriteUInt32LittleEndian(data.AsSpan(data.Length - 4), sum); }

    internal static async Task Run(string root, Func<string, Func<Task>, Task> test)
    {
        await test("GTA III compatible saves retain identical bytes across platforms", () =>
        { var data = Save(); var before = data.ToArray(); LibertySave.Validate(data); Check(before.SequenceEqual(data)); Check(data.Length == 201828); return Task.CompletedTask; });
        await test("Save validator rejects checksum corruption and truncation", async () =>
        {
            var data = Save(); data[192] ^= 3;
            await Reject<InvalidDataException>(() => { LibertySave.Validate(data); return Task.CompletedTask; });
            await Reject<InvalidDataException>(() => { LibertySave.Validate(data[..^1]); return Task.CompletedTask; });
        });
        await test("Save validator rejects Vice City and invalid block layouts", async () =>
        {
            var data = Save(); BinaryPrimitives.WriteUInt32LittleEndian(data.AsSpan(216), 7876); Checksum(data);
            await Reject<InvalidDataException>(() => { LibertySave.Validate(data); return Task.CompletedTask; });
            data = Save(); BinaryPrimitives.WriteUInt32LittleEndian(data, uint.MaxValue); Checksum(data);
            await Reject<InvalidDataException>(() => { LibertySave.Validate(data); return Task.CompletedTask; });
            data = Save(); BinaryPrimitives.WriteUInt32LittleEndian(data.AsSpan(68), 100); Checksum(data);
            await Reject<InvalidDataException>(() => { LibertySave.Validate(data); return Task.CompletedTask; });
        });
        await test("Save transfers are hidden from auxiliary tools", () =>
        { Check(!AuxiliaryTools.Catalog.Any(t => t.Path.StartsWith("overlay/tools/saves/", StringComparison.Ordinal))); return Task.CompletedTask; });
        await test("Save direction and slot persist without changing existing action keys", () =>
        {
            Check((int)ManagerAction.Update == 5 && (int)ManagerAction.TransferSave == 6);
            var original = new ManagerSettings { Actions = new() { [ManagerAction.TransferSave] = new() { SaveDirection = SaveTransferDirection.PcToQuest, SaveSlot = 8, PcSaveDirectory = "chosen folder" } } };
            var saved = JsonSerializer.Deserialize<ManagerSettings>(JsonSerializer.Serialize(original))!;
            Check(saved.Actions[ManagerAction.TransferSave].SaveSlot == 8 && saved.Actions[ManagerAction.TransferSave].SaveDirection == SaveTransferDirection.PcToQuest);
            return Task.CompletedTask;
        });

        async Task Exercise(SaveTransferDirection direction, Func<string, ManagerSettings, ActionOptions, Slots, Task> check)
        {
            var work = Path.Combine(root, "save-" + Guid.NewGuid().ToString("N")); Directory.CreateDirectory(work);
            var pc = Path.Combine(work, "pc saves with spaces"); Directory.CreateDirectory(pc);
            var settings = new ManagerSettings { Repository = Path.Combine(work, "source"), Workplace = Path.Combine(work, "work") };
            var options = new ActionOptions { PcSaveDirectory = pc, SaveDirection = direction, SaveSlot = 3 };
            var slots = new Slots { Bytes = Save(23) };
            await check(Path.Combine(pc, "GTA3sf3.b"), settings, options, slots);
        }
        async Task Perform(ManagerSettings settings, ActionOptions options, Slots slots, CancellationToken token = default)
        {
            using var log = new PrivateLog();
            await new SaveTransfer(settings, log, (_, _) => Task.FromResult<ISaveSlotDevice>(slots)).Perform(options, token);
        }
        string[] Backups(ManagerSettings s) => Directory.Exists(Path.Combine(s.Workplace, "save-transfer/backups"))
            ? Directory.GetFiles(Path.Combine(s.Workplace, "save-transfer/backups"), "*.bak", SearchOption.AllDirectories) : [];

        await test("Quest to PC backs up the old destination and exports exact bytes", () => Exercise(SaveTransferDirection.QuestToPc, async (file, s, o, d) =>
        {
            var old = Save(42); File.WriteAllBytes(file, old); var quest = d.Bytes!.ToArray();
            await Perform(s, o, d); Check((await File.ReadAllBytesAsync(file)).SequenceEqual(quest));
            Check(Backups(s).Length == 1 && File.ReadAllBytes(Backups(s)[0]).SequenceEqual(old)); Check(d.Writes == 0 && d.Stopped && d.SlotNumbers.All(n => n == 3));
        }));
        await test("Quest export creates a new PC folder without replacing other slots", () => Exercise(SaveTransferDirection.QuestToPc, async (file, s, o, d) =>
        {
            o.PcSaveDirectory = Path.Combine(o.PcSaveDirectory, "new folder");
            var other = Path.Combine(Path.GetDirectoryName(file)!, "GTA3sf4.b"); File.WriteAllText(other, "other slot");
            await Perform(s, o, d); Check(File.Exists(Path.Combine(o.PcSaveDirectory, "GTA3sf3.b"))); Check(File.ReadAllText(other) == "other slot");
        }));
        await test("Empty or invalid Quest saves preserve the PC destination", () => Exercise(SaveTransferDirection.QuestToPc, async (file, s, o, d) =>
        {
            var before = Save(); File.WriteAllBytes(file, before); d.Bytes = null;
            await Reject<IOException>(() => Perform(s, o, d)); Check(File.ReadAllBytes(file).SequenceEqual(before));
            d.Bytes = Save(); d.Bytes[1000] ^= 1;
            await Reject<InvalidDataException>(() => Perform(s, o, d)); Check(File.ReadAllBytes(file).SequenceEqual(before) && Backups(s).Length == 0);
        }));
        await test("PC to Quest backs up the original and verifies the selected slot", () => Exercise(SaveTransferDirection.PcToQuest, async (file, s, o, d) =>
        {
            var pc = Save(42); File.WriteAllBytes(file, pc); var original = d.Bytes!.ToArray();
            await Perform(s, o, d); Check(d.Bytes!.SequenceEqual(pc) && d.Writes == 1 && d.Stopped);
            Check(Backups(s).Length == 1 && File.ReadAllBytes(Backups(s)[0]).SequenceEqual(original));
        }));
        await test("Corrupt PC saves are rejected before any headset connection", () => Exercise(SaveTransferDirection.PcToQuest, async (file, s, o, d) =>
        {
            File.WriteAllBytes(file, [1, 2, 3]); using var log = new PrivateLog();
            await Reject<InvalidDataException>(() => new SaveTransfer(s, log, (_, _) => throw new Exception("Unexpected headset connection")).Perform(o, default));
            Check(d.Writes == 0);
        }));
        await test("Failed save read-back restores the previous Quest save", () => Exercise(SaveTransferDirection.PcToQuest, async (file, s, o, d) =>
        {
            File.WriteAllBytes(file, Save(42)); var before = d.Bytes!.ToArray(); d.CorruptFirstWrite = true;
            await Reject<IOException>(() => Perform(s, o, d)); Check(d.Writes == 2 && d.Bytes!.SequenceEqual(before)); Check(File.ReadAllBytes(Backups(s).Single()).SequenceEqual(before));
        }));
        await test("Cancelled save write rolls back with an independent recovery token", () => Exercise(SaveTransferDirection.PcToQuest, async (file, s, o, d) =>
        {
            File.WriteAllBytes(file, Save(42)); var before = d.Bytes!.ToArray(); d.CancelFirstWrite = true;
            await Reject<OperationCanceledException>(() => Perform(s, o, d)); Check(d.Writes == 2 && d.Bytes!.SequenceEqual(before) && !d.RecoveryTokenCancelled);
        }));
        await test("Failed import to an empty Quest slot restores empty state", () => Exercise(SaveTransferDirection.PcToQuest, async (file, s, o, d) =>
        {
            File.WriteAllBytes(file, Save(42)); d.Bytes = null; d.CorruptFirstWrite = true;
            await Reject<IOException>(() => Perform(s, o, d)); Check(d.Bytes is null && d.Writes == 2);
        }));
        await test("Concurrent Quest destination changes abort before writing", () => Exercise(SaveTransferDirection.PcToQuest, async (file, s, o, d) =>
        {
            File.WriteAllBytes(file, Save(42)); d.ChangeOnSecondRead = true;
            await Reject<IOException>(() => Perform(s, o, d)); Check(d.Writes == 0);
        }));
        await test("Save backup failures prevent destination replacement", () => Exercise(SaveTransferDirection.PcToQuest, async (file, s, o, d) =>
        {
            File.WriteAllBytes(file, Save(42)); var original = d.Bytes!.ToArray();
            Directory.CreateDirectory(Path.Combine(s.Workplace, "save-transfer"));
            File.WriteAllText(Path.Combine(s.Workplace, "save-transfer/backups"), "blocks backup directory creation");
            await Reject<IOException>(() => Perform(s, o, d)); Check(d.Writes == 0 && d.Bytes!.SequenceEqual(original));
        }));
        await test("Invalid slots and transfer directions never connect", async () =>
        {
            using var log = new PrivateLog(); var service = new SaveTransfer(new(), log, (_, _) => throw new Exception("Unexpected connection"));
            await Reject<ArgumentException>(() => service.Perform(new() { SaveSlot = 9 }, default));
            await Reject<ArgumentException>(() => service.Perform(new() { SaveDirection = (SaveTransferDirection)99 }, default));
        });
        await test("ADB save adapter preserves binary data through sync files", async () =>
        {
            var folder = Path.Combine(root, "save-transport"); Directory.CreateDirectory(folder);
            using var log = new PrivateLog(); var runner = new SaveRunner { Bytes = Save(255) };
            var device = new QuestDevice(runner, log, "adb", "synthetic-target");
            var adapter = new QuestSaveSlots(device, folder, log); await adapter.StopGame(default);
            Check((await adapter.Read(8, default))!.SequenceEqual(runner.Bytes!)); var next = Save(17);
            await adapter.Write(8, next, default); Check((await adapter.Read(8, default))!.SequenceEqual(next));
            Check(runner.Calls.All(a => a[0] == "-s" && a[1] == "synthetic-target"));
            Check(runner.Calls.Any(a => a.Contains("pull")) && runner.Calls.Any(a => a.Contains("push")));
            Check(runner.Calls.SelectMany(a => a).All(a => !a.Contains("miamivr")) && runner.Remote.Count == 0 && Directory.GetFiles(folder).Length == 0);
        });
        await test("ADB provider errors cannot masquerade as an empty save slot", async () =>
        {
            var folder = Path.Combine(root, "save-provider-error"); Directory.CreateDirectory(folder);
            using var log = new PrivateLog(); var runner = new SaveRunner { QueryError = true };
            await Reject<IOException>(() => new QuestSaveSlots(new QuestDevice(runner, log, "adb", "synthetic-target"), folder, log).Read(1, default));
        });
    }

    private sealed class Slots : ISaveSlotDevice
    {
        public byte[]? Bytes;
        public bool Stopped, CorruptFirstWrite, CancelFirstWrite, ChangeOnSecondRead, RecoveryTokenCancelled;
        public int Writes, Reads;
        public List<int> SlotNumbers { get; } = [];
        public Task StopGame(CancellationToken token) { token.ThrowIfCancellationRequested(); Stopped = true; return Task.CompletedTask; }
        public Task<byte[]?> Read(int slot, CancellationToken token)
        {
            token.ThrowIfCancellationRequested(); SlotNumbers.Add(slot); Reads++;
            if (ChangeOnSecondRead && Reads == 2) Bytes = Save(99);
            return Task.FromResult(Bytes?.ToArray());
        }
        public Task Write(int slot, byte[] data, CancellationToken token)
        {
            token.ThrowIfCancellationRequested(); SlotNumbers.Add(slot); Writes++;
            Bytes = data.Length == 0 ? null : data.ToArray();
            if (Writes == 1 && (CorruptFirstWrite || CancelFirstWrite)) Bytes![0] ^= 1;
            if (Writes == 1 && CancelFirstWrite) throw new OperationCanceledException();
            if (Writes == 2) RecoveryTokenCancelled = token.IsCancellationRequested;
            return Task.CompletedTask;
        }
    }
    private sealed class SaveRunner : ICommandRunner
    {
        public byte[]? Bytes;
        public bool QueryError;
        public Dictionary<string, byte[]> Remote { get; } = [];
        public List<string[]> Calls { get; } = [];
        public Task<CommandResult> Run(string executable, IEnumerable<string> arguments, string? workingDirectory = null, bool showOutput = true, bool check = true, CancellationToken token = default, string? input = null)
        {
            var a = arguments.ToArray(); Calls.Add(a); Check(!showOutput && input is null);
            var args = a.Skip(2).ToArray(); var output = "";
            if (args.SequenceEqual(new[] { "shell", "pm", "path", QuestDevice.Package })) output = "package:/installed.apk";
            else if (args.Contains("query")) output = QueryError ? "Error while accessing provider" : $"Row: 0 _display_name=GTA3sf{args[4][^1]}.b, _size={Bytes?.Length ?? 0}";
            else if (args[0] == "pull") File.WriteAllBytes(args[2], Remote[args[1]]);
            else if (args[0] == "push") Remote[args[2]] = File.ReadAllBytes(args[1]);
            else if (args[0] == "shell" && args.Length == 2)
            {
                var quoted = Regex.Matches(args[1], "'([^']*)'").Select(m => m.Groups[1].Value).ToArray();
                if (args[1].StartsWith("content read", StringComparison.Ordinal)) Remote[quoted[1]] = Bytes!.ToArray();
                else if (args[1].StartsWith("content write", StringComparison.Ordinal)) Bytes = Remote[quoted[1]].ToArray();
                else if (args[1].StartsWith("sha256sum", StringComparison.Ordinal)) output = Convert.ToHexString(SHA256.HashData(Remote[quoted[0]])) + "  file";
                else if (args[1].StartsWith("rm -f", StringComparison.Ordinal)) Remote.Remove(quoted[0]);
            }
            return Task.FromResult(new CommandResult(0, output));
        }
    }
}
