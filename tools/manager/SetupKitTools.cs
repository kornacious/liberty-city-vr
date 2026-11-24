namespace LibertyManager;

internal static class SetupKitTools
{
    internal static string HostRid => OperatingSystem.IsWindows() ? "win-x64" : "linux-x64";

    internal static string? Find(string repository, string name)
    {
        var path = Paths.Child(repository, $"Setup/tools/{HostRid}/{name}");
        var manifest = path + ".sha256";
        Paths.NoLinks(manifest);
        if (!File.Exists(path) && !File.Exists(manifest)) return null;
        if (!File.Exists(path) || !File.Exists(manifest) ||
            !Digests.Sha256(path).Equals(File.ReadAllText(manifest).Trim(), StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException($"The Setup kit copy of {name} is missing or failed verification. Extract the complete kit again.");
        return path;
    }

    internal static string? XboxConverter(string repository) =>
        Find(repository, OperatingSystem.IsWindows() ? "xbox-convert.exe" : "xbox-convert");

    internal static async Task TestXboxConverter(ICommandRunner runner, string converter, CancellationToken token)
    {
        foreach (var argument in new[] { "--self-test", "--head-uv-test" })
            if ((await runner.Run(converter, [argument], showOutput: false, check: false, token: token)).ExitCode != 0)
                throw new IOException("The bundled Xbox converter failed its self-test. Extract the complete Setup kit again.");
    }
}
