using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

namespace LibertyManager;

internal sealed class PortablePcvrSetup(ManagerSettings settings, SetupSelection selection, PrivateLog log)
{
    private const string Ownership = ".portable-pcvr";
    private const string ScratchOwner = ".setup-owned";
    private const string Capability = "portable-engine/1";
    private static readonly string[] Folders = ["app", "data", "user"];
    private static readonly string[] PersonalConfigs = ["re3.ini", "vr_settings.ini", "vr_body_ik.cfg",
        "vr_cutscene_camera_anchors.cfg", "vr_cutscene_mode.cfg", "desktop-preview.cfg",
        "vehicle_placements.template.ini", "vehicle_placements.personal.ini", "vehicle_placements.personal.ini.bak"];
    private GameTarget Target => selection.Target;
    private string Root => Path.TrimEndingDirectorySeparator(Path.GetFullPath(selection.InstallRoot ?? settings.Workplace));
    private string Scratch => Path.Combine(Root, "tools");
    internal string? Bundle => PcvrWorkflow.SetupBundle(settings.Repository, Target);
    internal ManagerSettings ScratchSettings => settings with { Workplace = Scratch };

    internal static bool OwnsApp(string app, GameTarget target)
    {
        var marker = Paths.Child(app, Ownership);
        return File.Exists(marker) && File.ReadAllText(marker).Trim() == target.ToString();
    }

    internal static bool CanUpdate(string root, GameTarget target)
    {
        if (!Path.IsPathFullyQualified(root)) return false;
        return OwnsApp(Paths.Child(root, "app"), target) ||
            OwnsApp(Paths.Child(root, "tools/previous/app"), target);
    }

    internal void CheckDestination()
    {
        if (!Path.IsPathFullyQualified(selection.InstallRoot ?? settings.Workplace) ||
            Root == Path.GetPathRoot(Root) || File.Exists(Root))
            throw new IOException("Choose a dedicated PCVR install folder.");
        Paths.NoLinks(Root);
        foreach (var source in new[] { settings.Repository, selection.PcFolder, selection.XboxImage,
                     selection.UpstateArchive, selection.LegacyInstall }.Where(p => !string.IsNullOrWhiteSpace(p)))
            if (Paths.Within(source!, Root) || Paths.Within(Root, source!))
                throw new IOException("Choose an install folder separate from the kit, originals and migration source.");
        if (!Directory.Exists(Root)) return;
        var owned = OwnsApp(Paths.Child(Root, "app"), Target);
        var scratchOwned = File.Exists(Paths.Child(Scratch, ScratchOwner)) &&
            File.ReadAllText(Paths.Child(Scratch, ScratchOwner)).Trim() == Target.ToString();
        if (Directory.EnumerateFileSystemEntries(Root).Any(p =>
            !(Path.GetFileName(p) == "tools" && scratchOwned ||
              Folders.Contains(Path.GetFileName(p), StringComparer.Ordinal) &&
              (owned || scratchOwned && File.Exists(Path.Combine(Scratch, "publishing"))))))
            throw new IOException("Choose an empty folder or an existing portable installation for this target.");
        if (owned && !string.IsNullOrWhiteSpace(selection.LegacyInstall))
            throw new IOException("Copy an existing installation into an empty destination.");
    }

    internal (string Data, string User)? FindLegacy()
    {
        if (string.IsNullOrWhiteSpace(selection.LegacyInstall)) return null;
        var source = Path.GetFullPath(selection.LegacyInstall);
        Paths.NoLinks(source);
        var app = Directory.Exists(Paths.Child(source, "app")) ? Paths.Child(source, "app") : source;
        var binary = app;
        if (OwnsApp(app, Target)) ValidateApp(app, Target);
        else binary = PcvrWorkflow.ResolveInstallation(app, Target);
        var paths = PcvrBuilds.ReadPaths(binary);
        Paths.NoLinks(paths.User);
        PcvrWorkflow.ValidatePreparedData(paths.Data);
        if (new[] { binary, paths.Data, paths.User }.Any(p =>
            p == Path.GetPathRoot(p) || new[] { binary, paths.Data, paths.User }.Any(other =>
                !string.Equals(other, p, Paths.Comparison) && Paths.Within(p, other))) ||
            string.Equals(paths.Data, paths.User, Paths.Comparison) ||
            string.Equals(paths.User, binary, Paths.Comparison))
            throw new IOException("The existing installation has overlapping app, data or user paths.");
        if (!new[] { "pc", "xbox" }.Any(id => Directory.Exists(Paths.Child(paths.Data, "variants/" + id))))
            throw new IOException("The existing installation has no managed asset variants to copy.");
        foreach (var path in new[] { binary, paths.Data, paths.User })
            if (Paths.Within(Root, path) || Paths.Within(path, Root))
                throw new IOException("Choose a destination separate from the existing installation.");
        return paths;
    }

    internal static void ValidateApp(string app, GameTarget target)
    {
        PcvrWorkflow.ValidateBinaries(app, target, portable: true);
        if (!File.ReadAllLines(Paths.Child(app, PcvrMods.Capabilities)).Contains(Capability, StringComparer.Ordinal) ||
            !PcvrMods.Supports(app, target, CuratedMods.Upstate))
            throw new InvalidDataException("The PCVR package is missing its portable engine libraries. Extract the matching Setup kit.");
        var suffix = target == GameTarget.WindowsPcvr ? ".exe" : "";
        foreach (var name in new[] { "libertyvr-pcvr", "libertyvr-upstate", "libertyvr-render-debug", "libertyvr-render-debug-upstate" })
            if (File.Exists(Paths.Child(app, name + suffix)))
                throw new InvalidDataException("The portable PCVR package contains a developer executable.");
    }

    internal Task Install(ActionOptions options, InstallRun run, CancellationToken token) => Task.Run(() =>
    {
        // Mutex ownership is thread-affine. Keep this worker on the same thread until installation ends.
        var key = OperatingSystem.IsWindows() ? Root.ToUpperInvariant() : Root;
        using var mutex = new Mutex(false, "LibertyVRSetup-" + Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(key))));
        try
        {
            if (!mutex.WaitOne(0)) throw new IOException("Setup is already installing to this folder.");
        }
        catch (AbandonedMutexException) { }
        try { InstallLocked(options, run, token).GetAwaiter().GetResult(); }
        finally { mutex.ReleaseMutex(); }
    }, token);

    private void Recover()
    {
        if (!Directory.Exists(Scratch)) return;
        if (File.Exists(Paths.Child(Scratch, "publishing")) && !File.Exists(Paths.Child(Scratch, "committed")))
        {
            foreach (var folder in Folders.Reverse())
            {
                var live = Paths.Child(Root, folder);
                var staged = Paths.Child(Scratch, "incoming/" + folder);
                var saved = Paths.Child(Scratch, "previous/" + folder);
                if (!File.Exists(Paths.Child(Scratch, "moving-" + folder))) continue;
                if (!Directory.Exists(staged) && Directory.Exists(live)) Directory.Move(live, staged);
                if (Directory.Exists(saved)) Directory.Move(saved, live);
            }
        }
        RemoveTree(Scratch);
    }

    private static void RemoveTree(string path)
    {
        if (!Directory.Exists(path)) return;
        Paths.NoLinks(path);
        // Directory.Delete does not traverse directory links. Permit internal Linux library links.
        Directory.Delete(path, recursive: true);
    }

    private async Task InstallLocked(ActionOptions options, InstallRun run, CancellationToken token)
    {
        CheckDestination();
        PcvrMods.RequireStopped();
        Recover();
        CheckDestination();
        var legacy = FindLegacy();
        Directory.CreateDirectory(Scratch);
        File.WriteAllText(Paths.Child(Scratch, ScratchOwner), Target.ToString());
        var runner = new CommandRunner(log);
        var local = ScratchSettings;
        var app = Paths.Child(Scratch, "incoming/app");
        var data = Paths.Child(Scratch, "incoming/data");
        var user = Paths.Child(Scratch, "incoming/user");
        Directory.CreateDirectory(data);
        Directory.CreateDirectory(user);
        var liveData = Paths.Child(Root, "data");
        var liveUser = Paths.Child(Root, "user");
        try
        {
            run.Report(InstallStage.PreparingGameFiles);
            if (Directory.Exists(liveData)) Paths.CopyTree(liveData, data, token);
            if (Directory.Exists(liveUser)) Paths.CopyTree(liveUser, user, token);
            var configs = Paths.Child(user, "configs");
            Directory.CreateDirectory(configs);
            Directory.CreateDirectory(Paths.Child(user, "userfiles"));
            Directory.CreateDirectory(Paths.Child(user, "logs"));
            var variants = Paths.Child(data, "variants");
            Directory.CreateDirectory(variants);
            if (legacy is { } old)
            {
                CopyLegacyData(old.Data, data, token);
                var saves = Paths.Child(old.User, "userfiles");
                if (Directory.Exists(saves)) Paths.CopyTree(saves, Paths.Child(user, "userfiles"), token);
                foreach (var name in PersonalConfigs)
                {
                    var configured = Paths.Child(old.User, "configs/" + name);
                    var previous = Paths.Child(old.User, name);
                    if (File.Exists(configured)) File.Copy(configured, Paths.Child(configs, name));
                    else if (File.Exists(previous)) File.Copy(previous, Paths.Child(configs, name));
                }
            }
            foreach (var name in PersonalConfigs)
            {
                var source = Paths.Child(legacy?.Data ?? data, name);
                var destination = Paths.Child(configs, name);
                if (File.Exists(source) && !File.Exists(destination)) File.Copy(source, destination);
                File.Delete(Paths.Child(data, name));
            }
            File.Delete(Paths.Child(data, "vehicle_placements.live.ini"));
            File.Delete(Paths.Child(data, "vehicle_placements.reload"));
            var bundle = Bundle;
            string? portFiles = null;
            if (bundle is null)
            {
                run.Report(InstallStage.BuildingGame);
                portFiles = await new PcvrWorkflow(local, runner, log, Target).BuildPortablePackage(app, token, runChecks: false);
                run.Built = true;
            }
            else
            {
                PcvrBuilds.CopyPackage(Paths.Child(bundle, "app"), app, token);
                if (VariantInstaller.WantsAny(options))
                    portFiles = await PortablePortAssets.Prepare(bundle, local, log, token);
            }
            if (options.InstallXboxVariant) local = await new FfmpegTools(local, runner, log).Resolve(token);
            var converter = options.InstallXboxVariant ? SetupKitTools.XboxConverter(local.Repository) : null;
            if (VariantInstaller.WantsAny(options))
            {
                run.Report(InstallStage.PreparingGameFiles);
                var prepared = await VariantInstaller.Prepare(local, options, portFiles!, runner, log, token,
                    xboxConverter: converter, portable: true);
                run.Report(InstallStage.InstallingGameFiles);
                foreach (var (id, source) in prepared)
                {
                    VariantInstaller.Validate(source, id);
                    var destination = Paths.Child(variants, id);
                    if (Directory.Exists(destination)) VariantInstaller.Validate(destination, id);
                    RemoveTree(destination);
                    Directory.Move(source, destination);
                }
            }
            File.WriteAllText(Paths.Child(variants, "MANAGED.txt"), "Liberty City VR complete variants v1\n");
            File.WriteAllText(Paths.Child(data, ".libertyvr-pcvr-data"), "Liberty City VR local Windows data v1\n");
            Workflows.RefreshWindowsPortFiles(local.Repository, data, token);
            PcvrWorkflow.ValidatePreparedData(data);
            var installed = new[] { "pc", "xbox" }.Where(id => Directory.Exists(Paths.Child(variants, id))).ToArray();
            if (installed.Length == 0) throw new IOException("Setup needs at least one complete PC or Xbox asset variant.");
            foreach (var id in installed) VariantInstaller.Validate(Paths.Child(variants, id), id);

            run.Report(InstallStage.InstallingApplication);
            foreach (var notice in new[] { "LICENSE", "NOTICE.md" })
                File.Copy(Paths.Child(settings.Repository, notice), Paths.Child(app, notice), overwrite: true);
            PcvrBuilds.WritePaths(app, Paths.Child(Root, "app"), liveData, liveUser);
            File.WriteAllText(Paths.Child(app, Ownership), Target.ToString());
            File.WriteAllText(Paths.Child(app, PcvrBuilds.Identity), DateTime.UtcNow.ToString("yyyyMMdd-HHmmss"));
            ValidateApp(app, Target);
            if (selection.InstallUpstate)
            {
                var mods = new CuratedMods(local, runner, log);
                var preparedMod = await mods.Prepare(CuratedMods.Upstate, selection.UpstateArchive ?? "", token);
                var status = PcvrMods.Install(new(app, data, user, Target), CuratedMods.Upstate, preparedMod, token);
                if (!status.Enabled) throw new IOException("Could not enable Upstate Liberty.");
            }
            var modPath = Paths.Child(data, "mods/" + CuratedMods.Upstate.Id);
            if (Directory.Exists(modPath)) CuratedMods.ValidatePackage(modPath, CuratedMods.Upstate);
            token.ThrowIfCancellationRequested();
            PcvrMods.RequireStopped();
            Publish();
        }
        catch
        {
            Recover();
            throw;
        }
        run.Installed = true;
        run.OutputPath = Root;
        try { Recover(); }
        catch (IOException error) { log.Write("Could not remove Setup tools. The next Setup run will retry. " + error.Message); }
        catch (UnauthorizedAccessException error) { log.Write("Could not remove Setup tools. The next Setup run will retry. " + error.Message); }
        log.Write("PCVR installed: " + Root);
    }

    private void Publish()
    {
        Directory.CreateDirectory(Paths.Child(Scratch, "previous"));
        File.WriteAllText(Paths.Child(Scratch, "publishing"), "");
        foreach (var folder in Folders)
        {
            var live = Paths.Child(Root, folder);
            File.WriteAllText(Paths.Child(Scratch, "moving-" + folder), "");
            if (Directory.Exists(live)) Directory.Move(live, Paths.Child(Scratch, "previous/" + folder));
        }
        // Publish the executable only after its sibling data and user directories are ready.
        foreach (var folder in Folders.Reverse())
            Directory.Move(Paths.Child(Scratch, "incoming/" + folder), Paths.Child(Root, folder));
        File.WriteAllText(Paths.Child(Scratch, "committed"), "");
    }

    private static void CopyLegacyData(string source, string data, CancellationToken token)
    {
        foreach (var id in new[] { "pc", "xbox" })
        {
            var variant = Paths.Child(source, "variants/" + id);
            if (!Directory.Exists(variant)) continue;
            VariantInstaller.Validate(variant, id);
            Paths.CopyTree(variant, Paths.Child(data, "variants/" + id), token);
        }
        var relative = "mods/" + CuratedMods.Upstate.Id;
        var mod = Paths.Child(source, relative);
        if (!Directory.Exists(mod)) return;
        CuratedMods.ValidatePackage(mod, CuratedMods.Upstate);
        Paths.CopyTree(mod, Paths.Child(data, relative), token);
        var marker = Paths.Child(source, relative + ".enabled");
        if (File.Exists(marker))
        {
            var expected = CuratedMods.Upstate;
            if (File.ReadAllText(marker).Trim() != $"{expected.Id}:{expected.Version}:{expected.Sha256}")
                throw new InvalidDataException("Invalid legacy Upstate activation marker.");
            File.Copy(marker, Paths.Child(data, relative + ".enabled"));
        }
    }
}

internal static class PortablePortAssets
{
    internal const string Manifest = "port-assets.json";
    private const string Upstream = "https://raw.githubusercontent.com/mrxenginner/re3/";

    internal static Dictionary<string, string> Read(string bundle, ManagerSettings settings)
    {
        var hashes = JsonSerializer.Deserialize<Dictionary<string, string>>(File.ReadAllText(Paths.Child(bundle, Manifest)))
            ?? throw new InvalidDataException("Missing portable port asset manifest.");
        var required = Workflows.PortAssets(true).Where(p => !File.Exists(Paths.Child(settings.Repository, "overlay/gamefiles/" + p))).ToHashSet(StringComparer.Ordinal);
        if (!required.SetEquals(hashes.Keys) || hashes.Values.Any(value => value is null || value.Length != 64 || !value.All(char.IsAsciiHexDigit)))
            throw new InvalidDataException("Invalid portable port asset manifest.");
        return hashes;
    }

    internal static async Task<string> Prepare(string bundle, ManagerSettings settings, PrivateLog log, CancellationToken token)
    {
        var hashes = Read(bundle, settings);
        var output = Paths.Child(settings.Workplace, "port-assets");
        if (hashes.Count > 0 && !settings.AllowToolDownloads)
            throw new IOException("Allow verified downloads in Setup help to retrieve port assets.");
        foreach (var (file, hash) in hashes)
        {
            var path = Paths.Child(output, file);
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            await Toolchain.DownloadFile(Upstream + Toolchain.Re3Commit + "/gamefiles/" + file, path, log, token);
            if (!Digests.Sha256(path).Equals(hash, StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("Port asset failed verification: " + file);
        }
        return output;
    }

    internal static void Write(string repository, string portFiles, string output)
    {
        var hashes = Workflows.PortAssets(true).Where(p => !File.Exists(Paths.Child(repository, "overlay/gamefiles/" + p)))
            .ToDictionary(p => p, p => Digests.Sha256(Paths.Child(portFiles, p)), StringComparer.Ordinal);
        File.WriteAllText(output, JsonSerializer.Serialize(hashes));
    }
}
