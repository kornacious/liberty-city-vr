using System.Text.Json;

namespace LibertyManager;

public sealed record ManagerSettings
{
    public int Version { get; init; } = 1;
    public string Repository { get; set; } = FindRepository();
    public string Workplace { get; set; } = DefaultWorkplace(OperatingSystem.IsWindows());
    public string AndroidSdk { get; set; } = Environment.GetEnvironmentVariable("ANDROID_HOME") ?? Environment.GetEnvironmentVariable("ANDROID_SDK_ROOT") ?? "";
    public string JavaHome { get; set; } = Environment.GetEnvironmentVariable("JAVA_HOME") ?? "";
    public string GameDirectory { get; set; } = "";
    public string Adb { get; set; } = "";
    public string Ffprobe { get; set; } = "ffprobe";
    public string Ffmpeg { get; set; } = "ffmpeg";
    public string CMake { get; set; } = "";
    public string Python { get; set; } = "";
    public string PowerShell { get; set; } = "";
    public string Bash { get; set; } = "";
    public string SevenZip { get; set; } = "";
    public bool AllowToolDownloads { get; set; } = true;
    public Dictionary<ManagerAction, ActionOptions> Actions { get; set; } = [];
    public static string DefaultWorkplace(bool windows) => windows ? @"C:\LCVRBuild" : Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "LCVRBuild");
    public static string ConfigurationDirectory => OperatingSystem.IsWindows()
        ? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "LibertyCityVR")
        : Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".liberty-city-vr");

    public static string FindRepository()
    {
        foreach (var start in new[] { Environment.CurrentDirectory, AppContext.BaseDirectory })
            for (var dir = new DirectoryInfo(start); dir is not null; dir = dir.Parent)
                if (File.Exists(Path.Combine(dir.FullName, "overlay", "android", "app", "build.gradle.kts"))) return dir.FullName;
        return Environment.CurrentDirectory;
    }

    public void Validate()
    {
        if (Version != 1) throw new InvalidDataException("This settings version is not supported.");
        Paths.RequireRepository(Repository);
        if (string.IsNullOrWhiteSpace(Workplace) || !Path.IsPathFullyQualified(Workplace))
            throw new InvalidDataException("Choose an absolute workplace path.");
        if (Paths.Within(Repository, Workplace) || Paths.Within(Workplace, Repository))
            throw new InvalidDataException("Keep the workplace separate from the source repository.");
    }
}

public sealed class SettingsStore(string? directory = null)
{
    public string DirectoryPath { get; } = directory ?? Environment.GetEnvironmentVariable("LCVR_SETTINGS_DIR") ?? ManagerSettings.ConfigurationDirectory;
    public string FilePath => Path.Combine(DirectoryPath, "settings.json");
    public ManagerSettings Load() => File.Exists(FilePath)
        ? JsonSerializer.Deserialize<ManagerSettings>(File.ReadAllText(FilePath)) ?? throw new InvalidDataException("Settings file is empty.")
        : new();

    public void Save(ManagerSettings settings)
    {
        settings.Validate();
        Directory.CreateDirectory(DirectoryPath);
        var temp = Path.Combine(DirectoryPath, $".settings-{Guid.NewGuid():N}");
        try
        {
            File.WriteAllText(temp, JsonSerializer.Serialize(settings, new JsonSerializerOptions { WriteIndented = true }));
            if (!OperatingSystem.IsWindows()) File.SetUnixFileMode(temp, UnixFileMode.UserRead | UnixFileMode.UserWrite);
            File.Move(temp, FilePath, true);
        }
        finally { if (File.Exists(temp)) File.Delete(temp); }
    }
}

public enum ManagerAction { BuildAndInstall, BuildVehicleEditor, ManageMusic, InstallModernModels, RepairGameFiles, Update, TransferSave, ManageMods, ImportMusic = ManageMusic }

public enum ModOperation { Status, Install, Enable, Disable, Delete, Validate }

public enum SaveTransferDirection { QuestToPc, PcToQuest }

public sealed record ActionOptions
{
    public string ModId { get; set; } = "upstate-liberty";
    public string ModArchive { get; set; } = "";
    public ModOperation ModOperation { get; set; }
    public bool BuildOnly { get; set; }
    public bool Release { get; set; }
    public bool CopyGameData { get; set; }
    public bool ConvertRadioStations { get; set; }
    public bool AcceptSdkLicenses { get; set; }
    public bool PackageEditor { get; set; } = true;
    public bool ValidateMusicOnly { get; set; }
    public bool DownloadModels { get; set; }
    public bool UpdateDryRun { get; set; }
    public string MusicSource { get; set; } = "";
    public string[] MusicSources { get; set; } = [];
    public string BoardExport { get; set; } = "";
    public string ModelArchive { get; set; } = "";
    public string ModelSource { get; set; } = "";
    public string ModelOutput { get; set; } = "";
    public string GameFiles { get; set; } = "";
    public string EditorOutput { get; set; } = "";
    public SaveTransferDirection SaveDirection { get; set; } = SaveTransferDirection.QuestToPc;
    public int SaveSlot { get; set; } = 1;
    public int[]? SaveSlots { get; set; }
    public string PcSaveDirectory { get; set; } = "";
    public string MusicRevision { get; set; } = "";
    public string[] MusicOrder { get; set; } = [];
    public string[] MusicDeleted { get; set; } = [];
}

public static class Paths
{
    public static StringComparison Comparison => OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal;
    public static bool Within(string root, string path)
    {
        root = Path.TrimEndingDirectorySeparator(Path.GetFullPath(root));
        path = Path.GetFullPath(path);
        return path.Equals(root, Comparison) || path.StartsWith(root + Path.DirectorySeparatorChar, Comparison);
    }
    public static string Child(string root, string relative)
    {
        if (string.IsNullOrWhiteSpace(relative) || Path.IsPathRooted(relative) || relative.Contains('\\') || relative.Split('/').Any(p => p is ".." or "." or ""))
            throw new InvalidDataException("Unsafe relative path.");
        var result = Path.GetFullPath(Path.Combine(root, relative));
        if (!Within(root, result)) throw new InvalidDataException("Path escapes its destination.");
        NoLinks(root);
        for (var dir = new FileInfo(result) as FileSystemInfo; dir is not null && Within(root, dir.FullName); dir = Directory.GetParent(dir.FullName))
            NoLinks(dir.FullName);
        return result;
    }
    public static void NoLinks(string path)
    {
        for (string? current = Path.GetFullPath(path); current is not null;
             current = Path.GetDirectoryName(Path.TrimEndingDirectorySeparator(current)))
        {
            try
            {
                if ((File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0)
                    throw new InvalidDataException("Linked paths are not supported for managed files.");
            }
            catch (FileNotFoundException) { }
            catch (DirectoryNotFoundException) { }
        }
    }
    public static void RequireRepository(string root)
    {
        if (!File.Exists(Path.Combine(root, "overlay/android/app/build.gradle.kts")) || !Directory.Exists(Path.Combine(root, "patches")))
            throw new InvalidDataException("Choose the complete Liberty City VR source repository.");
    }
    public static string? FindExecutable(string name)
    {
        if (Path.IsPathRooted(name)) return File.Exists(name) ? name : null;
        foreach (var dir in (Environment.GetEnvironmentVariable("PATH") ?? "").Split(Path.PathSeparator))
            foreach (var ext in OperatingSystem.IsWindows() ? new[] { "", ".exe" } : new[] { "" })
            {
                var candidate = Path.Combine(dir.Trim('"'), name + ext);
                if (File.Exists(candidate)) return Path.GetFullPath(candidate);
            }
        return null;
    }
    public static void CopyTree(string source, string destination, CancellationToken token)
    {
        NoLinks(source);
        Directory.CreateDirectory(destination);
        foreach (var entry in Directory.EnumerateFileSystemEntries(source))
        {
            token.ThrowIfCancellationRequested();
            if (Path.GetFileName(entry) == ".git") continue;
            NoLinks(entry);
            var target = Path.Combine(destination, Path.GetFileName(entry));
            if (Directory.Exists(entry)) CopyTree(entry, target, token);
            else File.Copy(entry, target, true);
        }
    }
}
