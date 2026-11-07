using System.Buffers.Binary;
using System.Text;
using System.Text.RegularExpressions;

namespace LibertyManager;

public sealed record PackFile(long Bytes, string Sha256);
public sealed record ImgEntry(string Name, uint Sector, uint Count, string Archive);

public sealed class ModernModels(ManagerSettings settings, ICommandRunner runner, PrivateLog log)
{
    public static IEnumerable<ImgEntry> ReadImg(string img, string directory, bool allowEmpty = false, bool retailSource = false)
    {
        var length = new FileInfo(img).Length;
        var bytes = File.ReadAllBytes(directory);
        if (bytes.Length == 0 || bytes.Length % 32 != 0) throw new InvalidDataException("Invalid IMG v1 directory length.");
        var names = new HashSet<string>();
        for (var i = 0; i < bytes.Length; i += 32)
        {
            var sector = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(i, 4));
            var count = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(i + 4, 4));
            var name = Encoding.ASCII.GetString(bytes, i + 8, 24).Split('\0')[0].ToLowerInvariant();
            if ((!Regex.IsMatch(name, @"\A[a-z0-9_@]{1,19}\.(dff|txd)\z") && !(retailSource && name == "rwfilemaker.rws")) || !names.Add(name) ||
                (!allowEmpty && count == 0) || ((long)sector + count) * 2048 > length)
                throw new InvalidDataException("Invalid or duplicate IMG v1 entry.");
            if (count != 0 && name != "rwfilemaker.rws") yield return new(name, sector, count, img);
        }
    }

    public static Dictionary<string, string> ValidateOverlay(string root) => OriginalXboxModels.ValidateOverlay(root);

    public Task<string> Prepare(ActionOptions options, CancellationToken token) => new OriginalXboxModels(settings, runner, log).Prepare(options, token);

    public async Task Install(QuestDevice device, string output, CancellationToken token)
    {
        var hashes = ValidateOverlay(output);
        var installed = await device.Run(["shell", "pm", "path", QuestDevice.Package], token);
        if (!installed.Output.Contains("package:")) throw new IOException("Install Liberty City VR before installing models.");
        await device.Bootstrap(token);
        await device.Run(["shell", "am", "force-stop", QuestDevice.Package], token);
        var root = QuestDevice.GameData + "/modelsets";
        var stamp = Guid.NewGuid().ToString("N");
        var stage = root + "/.modern-incoming-" + stamp;
        var backup = root + "/.modern-backup-" + stamp;
        var final = root + "/modern";
        var activated = false;
        try
        {
            foreach (var (relative, expectedHash) in hashes.Append(new KeyValuePair<string, string>("BUILD_INFO.txt", Digests.Sha256(Path.Combine(output, "BUILD_INFO.txt")))))
            {
                var remote = stage + "/" + relative;
                await device.Shell("mkdir -p " + QuestDevice.Quote(remote[..remote.LastIndexOf('/')]), token);
                await device.Push(Paths.Child(output, relative), remote, token, expectedHash);
            }
            await device.Shell("chmod -R a+rX " + QuestDevice.Quote(stage), token);
            // One remote shell transaction restores the old profile if activation fails.
            var qFinal = QuestDevice.Quote(final); var qBackup = QuestDevice.Quote(backup); var qStage = QuestDevice.Quote(stage);
            await device.Shell($"if [ -d {qFinal} ]; then mv {qFinal} {qBackup} || exit 1; fi; if mv {qStage} {qFinal}; then :; else if [ -d {qBackup} ]; then mv {qBackup} {qFinal}; fi; exit 1; fi", token);
            activated = true;
            await device.Shell("rm -rf " + qBackup, token);
            log.Write("Original Xbox visuals and audio installed. Select Original Xbox under Appearance > Asset Source and fully restart the game.");
        }
        finally
        {
            if (!activated)
            {
                using var cleanup = new CancellationTokenSource(TimeSpan.FromSeconds(10));
                try { await device.Shell("rm -rf " + QuestDevice.Quote(stage), cleanup.Token, check: false); }
                catch (Exception error) when (error is IOException or OperationCanceledException) { log.Write("Staged model cleanup failed; inspect the local action log before retrying."); }
            }
        }
    }
}
