namespace LibertySetup;

internal static class SetupDesktopEntry
{
    internal const string AppId = "org.libertycityvr.Setup";

    internal static void Register()
    {
        var executable = Path.Combine(AppContext.BaseDirectory, "LibertySetup");
        var icon = Path.Combine(AppContext.BaseDirectory, "Assets", "StreetMarker.png");
        if (!File.Exists(executable) || !File.Exists(icon))
            throw new IOException("Open Setup from the extracted Linux kit and try again.");
        if (executable.Any(char.IsControl) || executable.Contains('=') || icon.Any(char.IsControl))
            throw new IOException("Move the Setup kit to a folder whose path contains no control characters or equals signs.");

        var data = Environment.GetEnvironmentVariable("XDG_DATA_HOME");
        if (string.IsNullOrWhiteSpace(data) || !Path.IsPathFullyQualified(data))
            data = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".local", "share");
        var applications = Path.Combine(data, "applications");
        Directory.CreateDirectory(applications);

        // Exec quoting is applied after the desktop entry's value escaping.
        var command = executable.Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("`", "\\`").Replace("$", "\\$").Replace("%", "%%");
        command = command.Replace("\\", "\\\\");
        var iconValue = icon.Replace("\\", "\\\\");
        var entry = $"""
            [Desktop Entry]
            Type=Application
            Name=Liberty City VR Setup
            Comment=Install Liberty City VR
            Exec="{command}"
            Icon={iconValue}
            StartupWMClass={AppId}
            Terminal=false
            Categories=Game;
            """;

        var temporary = Path.Combine(applications, $".{AppId}-{Guid.NewGuid():N}");
        try
        {
            File.WriteAllText(temporary, entry.ReplaceLineEndings("\n") + "\n");
            File.Move(temporary, Path.Combine(applications, AppId + ".desktop"), overwrite: true);
        }
        finally
        {
            if (File.Exists(temporary)) File.Delete(temporary);
        }
    }
}
