using System.Buffers.Binary;
using System.Text.RegularExpressions;

namespace LibertyManager;

/// <summary>
/// GTA III/re3 COMPATIBLE_SAVES serializes the same layout on Win64, Linux and Quest.
/// GenericGameStorage writes 20 data blocks, up to four padding blocks and a byte-sum
/// checksum. Garages is 5484 bytes; the inherited Vice City garage converter is unrelated.
/// </summary>
public static class LibertySave
{
    public const int PayloadBytes = 201728; // align4(SIZE_OF_ONE_GAME_IN_BYTES - 4)
    public const int MaximumBytes = PayloadBytes + 24 * 4 + 4;
    public static void Validate(ReadOnlySpan<byte> data)
    {
        if (data.Length < PayloadBytes + 20 * 4 + 4 || data.Length > MaximumBytes)
            throw new InvalidDataException("Unsupported GTA III save size. Use a compatible Liberty City/re3 save.");
        uint sum = 0;
        foreach (var value in data[..^4]) sum += value;
        if (sum != BinaryPrimitives.ReadUInt32LittleEndian(data[^4..]))
            throw new InvalidDataException("Save checksum mismatch. No destination was changed.");
        var offset = 0; var payload = 0; var blocks = 0;
        while (offset < data.Length - 4 && blocks < 24)
        {
            if (offset + 4 > data.Length - 4) throw new InvalidDataException("Truncated save block header.");
            var size = BinaryPrimitives.ReadUInt32LittleEndian(data.Slice(offset, 4));
            if (size is 0 or > 55000) throw new InvalidDataException("Invalid save block length.");
            var aligned = ((int)size + 3) & ~3;
            offset += 4;
            if (aligned > data.Length - 4 - offset) throw new InvalidDataException("Save block overruns its container.");
            var block = data.Slice(offset, (int)size);
            if (blocks == 0)
            {
                if (size < 192 || (BinaryPrimitives.ReadUInt32LittleEndian(block.Slice(64, 4)) & 0xFFFFFF) != 201729)
                    throw new InvalidDataException("The selected file is not a supported GTA III/re3 save.");
                var scripts = BinaryPrimitives.ReadUInt32LittleEndian(block.Slice(188, 4));
                if (scripts == 0 || scripts > 55000 || 192 + ((scripts + 3) & ~3U) != size)
                    throw new InvalidDataException("Invalid saved script block.");
            }
            else if (blocks < 20)
            {
                if (size < 4) throw new InvalidDataException("Missing saved subsystem block.");
                var inner = BinaryPrimitives.ReadUInt32LittleEndian(block[..4]);
                if (inner == 0 || inner > 55000 || 4 + ((inner + 3) & ~3U) != size)
                    throw new InvalidDataException("Invalid saved subsystem layout.");
                if (blocks == 2 && inner != 5484)
                    throw new InvalidDataException("Unsupported garages layout. Vice City and incompatible custom saves cannot be transferred.");
            }
            payload += aligned; offset += aligned; blocks++;
        }
        if (offset != data.Length - 4 || payload != PayloadBytes || blocks < 20)
            throw new InvalidDataException("Invalid GTA III save block stream.");
    }
}

public interface ISaveSlotDevice
{
    Task StopGame(CancellationToken token);
    Task<byte[]?> Read(int slot, CancellationToken token);
    Task Write(int slot, byte[] data, CancellationToken token);
}

/// <summary>Binary data uses ADB sync files, never text stdout or shell stdin on Windows.</summary>
public sealed class QuestSaveSlots(QuestDevice device, string temporaryDirectory, PrivateLog log) : ISaveSlotDevice
{
    private const int MaximumBackupBytes = 4 * 1024 * 1024;
    private static string Uri(int slot)
    {
        if (slot is < 1 or > 8) throw new ArgumentOutOfRangeException(nameof(slot));
        return QuestDevice.SaveUri + "/slot/" + slot;
    }
    public async Task StopGame(CancellationToken token)
    {
        using var limit = CancellationTokenSource.CreateLinkedTokenSource(token); limit.CancelAfter(TimeSpan.FromSeconds(30));
        var installed = await device.Run(["shell", "pm", "path", QuestDevice.Package], limit.Token);
        if (!installed.Output.Contains("package:", StringComparison.Ordinal)) throw new IOException("Liberty City VR is not installed on the selected headset.");
        await device.Run(["shell", "am", "force-stop", QuestDevice.Package], limit.Token);
    }
    public async Task<byte[]?> Read(int slot, CancellationToken token)
    {
        var uri = Uri(slot);
        using var limit = CancellationTokenSource.CreateLinkedTokenSource(token); limit.CancelAfter(TimeSpan.FromSeconds(60));
        var query = await device.Run(["shell", "content", "query", "--uri", uri, "--projection", "_display_name:_size"], limit.Token);
        var sizes = Regex.Matches(query.Output, @"\b_size=(\d+)\b");
        if (!query.Output.Contains("_display_name=GTA3sf" + slot + ".b", StringComparison.Ordinal) || sizes.Count != 1 ||
            !long.TryParse(sizes[0].Groups[1].Value, out var size) || size > MaximumBackupBytes)
            throw new IOException("Save provider did not return a valid slot size. No save was changed.");
        if (size == 0) return null;
        var name = "lcvr-save-" + Guid.NewGuid().ToString("N") + ".b";
        var remote = "/data/local/tmp/" + name; var local = Path.Combine(temporaryDirectory, name);
        try
        {
            await device.Shell("content read --uri " + QuestDevice.Quote(uri) + " > " + QuestDevice.Quote(remote), limit.Token);
            await device.Run(["pull", remote, local], limit.Token);
            if (!File.Exists(local) || new FileInfo(local).Length != size) throw new IOException("Save download was incomplete.");
            var digest = await device.Shell("sha256sum " + QuestDevice.Quote(remote), limit.Token);
            if (!string.Equals(digest.Output.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries).FirstOrDefault(), Digests.Sha256(local), StringComparison.OrdinalIgnoreCase))
                throw new IOException("Save download checksum verification failed.");
            return await File.ReadAllBytesAsync(local, limit.Token);
        }
        finally { await Cleanup(remote, local); }
    }
    public async Task Write(int slot, byte[] data, CancellationToken token)
    {
        var uri = Uri(slot);
        using var limit = CancellationTokenSource.CreateLinkedTokenSource(token); limit.CancelAfter(TimeSpan.FromSeconds(60));
        var name = "lcvr-save-" + Guid.NewGuid().ToString("N") + ".b";
        var remote = "/data/local/tmp/" + name; var local = Path.Combine(temporaryDirectory, name);
        try
        {
            await File.WriteAllBytesAsync(local, data, limit.Token);
            await device.Push(local, remote, limit.Token);
            await device.Shell("content write --uri " + QuestDevice.Quote(uri) + " < " + QuestDevice.Quote(remote), limit.Token);
        }
        finally { await Cleanup(remote, local); }
    }
    private async Task Cleanup(string remote, string local)
    {
        using var cleanup = new CancellationTokenSource(TimeSpan.FromSeconds(5));
        try { await device.Shell("rm -f " + QuestDevice.Quote(remote), cleanup.Token, check: false); }
        catch (Exception e) when (e is IOException or OperationCanceledException) { log.Write("Could not remove temporary save transfer file on the headset."); }
        if (File.Exists(local)) File.Delete(local);
    }
}

public sealed class SaveTransfer(ManagerSettings settings, PrivateLog log,
    Func<string, CancellationToken, Task<ISaveSlotDevice>> connect)
{
    public static string DefaultPcDirectory(ManagerSettings settings) => Path.Combine(settings.Workplace, "save-transfer", "pc");
    public async Task Perform(ActionOptions options, CancellationToken token)
    {
        if (!Enum.IsDefined(options.SaveDirection) || options.SaveSlot is < 1 or > 8)
            throw new ArgumentException("Select a transfer direction and a save slot from 1 through 8.");
        var directory = string.IsNullOrWhiteSpace(options.PcSaveDirectory) ? DefaultPcDirectory(settings) : Path.GetFullPath(options.PcSaveDirectory);
        Paths.NoLinks(directory);
        if (Paths.Within(settings.Repository, directory)) throw new IOException("Choose a PC save folder outside the source repository.");
        var fileName = "GTA3sf" + options.SaveSlot + ".b";
        var destination = Paths.Child(directory, fileName);
        var previousPc = await ReadLocal(destination, token);
        if (options.SaveDirection == SaveTransferDirection.PcToQuest)
        {
            if (previousPc is null) throw new IOException("PC save was not found: " + fileName + ". Choose the folder containing that slot.");
            LibertySave.Validate(previousPc);
        }
        var root = Path.Combine(settings.Workplace, "save-transfer"); Paths.NoLinks(root);
        var scratch = Path.Combine(root, ".transfer-" + Guid.NewGuid().ToString("N")); Directory.CreateDirectory(scratch);
        try
        {
            var device = await connect(scratch, token);
            await device.StopGame(token);
            var previousQuest = await device.Read(options.SaveSlot, token);
            if (options.SaveDirection == SaveTransferDirection.QuestToPc)
            {
                if (previousQuest is null) throw new IOException("The selected Quest save slot is empty.");
                LibertySave.Validate(previousQuest);
                if (previousPc is not null) Backup(root, fileName, "pc", previousPc);
                token.ThrowIfCancellationRequested();
                if (!Equal(previousPc, await ReadLocal(destination, token))) throw new IOException("The PC save changed during transfer. Retry after closing the PC game.");
                Directory.CreateDirectory(directory);
                var stage = Path.Combine(directory, ".lcvr-save-" + Guid.NewGuid().ToString("N"));
                try
                {
                    WriteDurable(stage, previousQuest);
                    token.ThrowIfCancellationRequested();
                    File.Move(stage, destination, overwrite: previousPc is not null);
                }
                finally { if (File.Exists(stage)) File.Delete(stage); }
                log.Write("Exported Quest slot " + options.SaveSlot + " to " + destination + ". Save bytes preserved and verified.");
            }
            else
            {
                if (previousQuest is not null) Backup(root, fileName, "quest-native", previousQuest);
                if (!Equal(previousQuest, await device.Read(options.SaveSlot, token))) throw new IOException("The Quest slot changed during transfer. Retry after closing other save tools.");
                token.ThrowIfCancellationRequested();
                try
                {
                    await device.Write(options.SaveSlot, previousPc!, token);
                    if (!Equal(previousPc, await device.Read(options.SaveSlot, token))) throw new IOException("Quest save read-back differs from the PC source.");
                }
                catch
                {
                    // The existing provider truncates on write. Recover even after caller cancellation.
                    using var recovery = new CancellationTokenSource(TimeSpan.FromSeconds(30));
                    try
                    {
                        await device.Write(options.SaveSlot, previousQuest ?? [], recovery.Token);
                        if (!Equal(previousQuest, await device.Read(options.SaveSlot, recovery.Token))) throw new IOException("Restore verification failed.");
                        log.Write("Previous Quest slot restored after an interrupted or failed transfer.");
                    }
                    catch (Exception e) when (e is IOException or OperationCanceledException)
                    { log.Write("Automatic slot recovery failed. Keep the game stopped and restore the local backup before using this slot."); }
                    throw;
                }
                log.Write("Imported " + fileName + " into Quest slot " + options.SaveSlot + "; byte-for-byte read-back verified.");
            }
            log.Write("Transfer complete. The Quest game is stopped; launch it when ready.");
        }
        finally { Directory.Delete(scratch, recursive: true); }
    }
    private static bool Equal(byte[]? a, byte[]? b) => (a is null || a.Length == 0) && (b is null || b.Length == 0) || a is not null && b is not null && a.AsSpan().SequenceEqual(b);
    private static async Task<byte[]?> ReadLocal(string file, CancellationToken token)
    {
        Paths.NoLinks(file);
        if (!File.Exists(file)) return null;
        if (new FileInfo(file).Length > 4 * 1024 * 1024) throw new IOException("PC save is unexpectedly large; destination was preserved.");
        return await File.ReadAllBytesAsync(file, token);
    }
    private void Backup(string root, string name, string kind, byte[] data)
    {
        var folder = Paths.Child(root, "backups/" + DateTime.UtcNow.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(folder);
        var file = Path.Combine(folder, name + "." + kind + ".bak"); WriteDurable(file, data);
        log.Write("Existing " + (kind == "pc" ? "PC" : "Quest") + " save backed up to " + file);
    }
    private static void WriteDurable(string file, byte[] data)
    {
        using var stream = new FileStream(file, FileMode.CreateNew, FileAccess.Write, FileShare.None);
        stream.Write(data); stream.Flush(flushToDisk: true);
    }
}
