using System.Diagnostics;

namespace LibertyManager;

public static class WindowsCompiler
{
    public static async Task Configure(CommandRunner runner, CancellationToken token)
    {
        if (!OperatingSystem.IsWindows() || Paths.FindExecutable("cl") is not null) return;
        var vswhere = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "Microsoft Visual Studio/Installer/vswhere.exe");
        if (!File.Exists(vswhere)) throw new IOException("Install Visual Studio or Build Tools with Desktop development with C++.");
        var result = await runner.Run(vswhere, ["-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], showOutput: false, token: token);
        var installation = result.Output.Trim();
        var environmentScript = Path.Combine(installation, "VC/Auxiliary/Build/vcvars64.bat");
        if (installation.Length == 0 || !File.Exists(environmentScript) || environmentScript.IndexOfAny(['"', '%', '!', '\r', '\n']) >= 0)
            throw new IOException("A Visual Studio x64 C++ toolchain could not be found.");
        // cmd.exe is needed only to read Microsoft's compiler environment. The path comes
        // from vswhere, is quoted, and no user arguments or filesystem mutations enter it.
        var start = new ProcessStartInfo(Environment.GetEnvironmentVariable("ComSpec") ?? "cmd.exe")
        {
            Arguments = "/d /s /c \"\"" + environmentScript + "\" >nul && set\"",
            UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true
        };
        using var process = Process.Start(start) ?? throw new IOException("Cannot read the C++ compiler environment.");
        using var stop = token.Register(() => { try { process.Kill(true); } catch (InvalidOperationException) { } });
        var output = process.StandardOutput.ReadToEndAsync(token); var error = process.StandardError.ReadToEndAsync(token);
        await process.WaitForExitAsync(token); await error;
        if (process.ExitCode != 0) throw new IOException("Visual Studio compiler environment setup failed. Private diagnostics suppressed.");
        foreach (var line in (await output).Split('\n'))
        {
            var separator = line.IndexOf('=');
            if (separator > 0) runner.EnvironmentOverrides[line[..separator]] = line[(separator + 1)..].TrimEnd('\r');
        }
    }
}
