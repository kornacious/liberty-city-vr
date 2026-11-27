using System.Runtime.InteropServices;
using System.Text;

namespace LibertyManager;

public static class PcvrShortcuts
{
    public static string DesktopDirectory()
    {
        if (OperatingSystem.IsWindows()) return Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
        var home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        var config = Environment.GetEnvironmentVariable("XDG_CONFIG_HOME");
        var file = Path.Combine(string.IsNullOrWhiteSpace(config) || !Path.IsPathFullyQualified(config) ? Path.Combine(home,".config") : config,"user-dirs.dirs");
        if (File.Exists(file))
            foreach (var line in File.ReadLines(file))
                if (line.Trim().StartsWith("XDG_DESKTOP_DIR=",StringComparison.Ordinal))
                {
                    var value = line.Trim()[16..].Trim('"').Replace("$HOME",home);
                    if (Path.IsPathFullyQualified(value)) return value;
                }
        return Path.Combine(home,"Desktop");
    }

    public static string LinuxEntry(string executable)
    {
        if(executable.IndexOfAny(['\r','\n','\0'])>=0) throw new InvalidDataException("Invalid desktop executable path.");
        var quoted = executable.Replace("\\","\\\\").Replace("\"","\\\"").Replace("`","\\`").Replace("$","\\$").Replace("%","%%");
        // Desktop entry string escaping is applied before Exec argument parsing.
        quoted = quoted.Replace("\\","\\\\");
        return "[Desktop Entry]\nType=Application\nName=Liberty City VR\nExec=\""+quoted+"\"\nTerminal=false\nCategories=Game;\n";
    }

    public static Task Create(PcvrPaths paths, CancellationToken token, string? desktop = null) =>
        CreateForApp(Path.Combine(paths.Install, "current"), paths.Target, token, desktop);

    public static Task CreatePortable(string root, GameTarget target, CancellationToken token) =>
        CreateForApp(Paths.Child(root, "app"), target, token, null);

    private static Task CreateForApp(string app, GameTarget target, CancellationToken token, string? desktop)
    {
        token.ThrowIfCancellationRequested();
        desktop ??= DesktopDirectory();
        if (string.IsNullOrWhiteSpace(desktop)) throw new IOException("Desktop directory is unavailable.");
        Directory.CreateDirectory(desktop);
        var executable = Path.Combine(app, target == GameTarget.LinuxPcvr ? "libertyvr-game" : "libertyvr-game.exe");
        if (target == GameTarget.LinuxPcvr)
        {
            var destination = Path.Combine(desktop,"Liberty City VR.desktop");
            File.WriteAllText(destination,LinuxEntry(executable),new UTF8Encoding(false));
            if (!OperatingSystem.IsWindows()) File.SetUnixFileMode(destination,UnixFileMode.UserRead|UnixFileMode.UserWrite|UnixFileMode.UserExecute);
        }
        else if (OperatingSystem.IsWindows()) CreateWindows(Path.Combine(desktop,"Liberty City VR.lnk"),executable);
        else throw new PlatformNotSupportedException("Windows shortcuts require Windows.");
        return Task.CompletedTask;
    }

    [System.Runtime.Versioning.SupportedOSPlatform("windows")]
    private static void CreateWindows(string destination,string executable)
    {
        var type = Type.GetTypeFromProgID("WScript.Shell") ?? throw new IOException("Windows shortcut service unavailable.");
        dynamic shell = Activator.CreateInstance(type)!;
        try
        {
            dynamic shortcut = shell.CreateShortcut(destination);
            try { shortcut.TargetPath=executable; shortcut.WorkingDirectory=Path.GetDirectoryName(executable); shortcut.Description="Liberty City VR"; shortcut.Save(); }
            finally { Marshal.FinalReleaseComObject(shortcut); }
        }
        finally { Marshal.FinalReleaseComObject(shell); }
    }
}
