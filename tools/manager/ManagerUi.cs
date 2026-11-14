using System.Collections.Concurrent;
using System.Collections.ObjectModel;
using Terminal.Gui.App;
using Terminal.Gui.Drawing;
using Terminal.Gui.ViewBase;
using Terminal.Gui.Views;
using Attribute = Terminal.Gui.Drawing.Attribute;

namespace LibertyManager;

public sealed class ManagerUi(IApplication app, SettingsStore store, ManagerSettings initial, bool loadMusicOnOpen = true) : IDisposable
{
    public const int SettingsPage = 8;
    public const int ToolsPage = 9;
    public const int LastPage = 11;
    private static bool IsActionPage(int page) => page < SettingsPage || page > ToolsPage;
    // GTA III menu cyan, amber HUD, cream lettering and the dark blue of its cover art.
    public static readonly Color Midnight = new(16, 24, 32);
    public static readonly Color Cyan = new(112, 190, 202);
    public static readonly Color Amber = new(230, 169, 64);
    public static readonly Color Cream = new(234, 227, 202);
    public static Scheme Theme() => new()
    {
        Normal = new Attribute(Cream, Midnight), Focus = new Attribute(Midnight, Cyan),
        HotNormal = new Attribute(Amber, Midnight), HotFocus = new Attribute(Midnight, Amber),
        Editable = new Attribute(Cream, new Color(28, 42, 53)),
        ReadOnly = new Attribute(Cream, Midnight),
        Disabled = new Attribute(new Color(107, 118, 126), Midnight)
    };

    public Window Window { get; } = new() { Title = "LIBERTY CITY / VR MANAGER", Width = Dim.Fill(), Height = Dim.Fill() };
    private readonly FrameView navigation = new() { Title = "ACTIONS", Width = 26, Height = Dim.Fill(1) };
    private readonly FrameView configuration = new() { Title = "CONFIGURATION", X = 26, Width = Dim.Fill(), Height = Dim.Fill(8) };
    private readonly FrameView activity = new() { Title = "ACTIVITY", X = 26, Y = Pos.AnchorEnd(8), Width = Dim.Fill(), Height = 7 };
#pragma warning disable CS0618 // Terminal.Gui 2.5 retains this read-only log view; no editor dependency is needed.
    private readonly TextView output = new() { ReadOnly = true, WordWrap = true, Width = Dim.Fill(), Height = Dim.Fill() };
#pragma warning restore CS0618
    private readonly ConcurrentQueue<string> pending = new();
    private readonly List<Action> readFields = [];
    private readonly List<Button> navigationButtons = [];
    private readonly Queue<string> lines = new();
    private readonly Button cancel = new() { Text = "_Cancel", X = 1, Y = Pos.AnchorEnd(1) };
    private readonly Button perform = new() { Text = "_Perform", X = Pos.AnchorEnd(13), Y = Pos.AnchorEnd(1) };
    private View? form;
    private int row;
    private int selected;
    private ManagerSettings settings = initial;
    private ManagerSettings draft = initial with { };
    private ActionOptions options = new();
    private CancellationTokenSource? cancellation;
    private Task? running;
    private string? logPath;
    private bool quitWhenStopped;
    private bool disposed;
    private string? auxiliary;
    private string auxiliaryArguments = "";
    private MusicManagementView? music;
    private bool musicRefresh;

    public void Initialize(int page = 0)
    {
        if(page == (int)ManagerAction.InstallModernModels)page=0;
        Window.SetScheme(Theme());
        output.SetScheme(Theme());
        cancel.ShadowStyle = perform.ShadowStyle = ShadowStyles.None;
        Window.Add(navigation, configuration, activity);
        activity.Add(output);
        var footer = new Label { Text = "Tab: move  |  Enter: select  |  Ctrl+Q: quit", Y = Pos.AnchorEnd(1), Width = Dim.Fill() };
        Window.Add(footer);
        string[] pages = ["Build and install", "Vehicle editor", "Manage music", "Original Xbox assets", "Repair game files", "Update", "Transfer save", "Mods", "Settings", "Tools and tests", "Launch PCVR", "PCVR diagnostics"];
        for (var i = 0; i < pages.Length; i++)
        {
            var index = i;
            var button = new Button { Text = pages[i], X = 0, Y = i > 3 ? i-1 : i, Width = Dim.Fill(), ShadowStyle = ShadowStyles.None, Visible = i != 3 };
            button.Accepting += (_, e) => { e.Handled = true; Render(index); form?.SetFocus(); };
            navigation.Add(button); navigationButtons.Add(button);
        }
        var quit = new Button { Text = "_Quit", X = 0, Y = Pos.AnchorEnd(1), Width = Dim.Fill(), ShadowStyle = ShadowStyles.None };
        quit.Accepting += (_, e) => { e.Handled = true; Quit(); };
        navigation.Add(quit);
        cancel.Accepting += (_, e) =>
        {
            e.Handled = true;
            if (running is { IsCompleted: false }) { cancellation?.Cancel(); pending.Enqueue("Cancelling; waiting for tools and cleanup to finish..."); return; }
            Render(selected); navigationButtons[selected].SetFocus();
        };
        perform.Accepting += (_, e) => { e.Handled = true; Start(); };
        Window.KeyDown += (_, e) => { if (e == Terminal.Gui.Input.Key.Q.WithCtrl) { e.Handled = true; Quit(); } };
        Window.IsRunningChanging += (_, e) =>
        {
            if (!e.NewValue && running is { IsCompleted: false })
            { e.Cancel = true; quitWhenStopped = true; cancellation?.Cancel(); }
        };
        app.AddTimeout(TimeSpan.FromMilliseconds(100), () =>
        {
            if (disposed) return false;
            var changed = false;
            while (pending.TryDequeue(out var line))
            {
                foreach (var part in line.Split('\n')) { lines.Enqueue(part.TrimEnd('\r')); while (lines.Count > 500) lines.Dequeue(); }
                changed = true;
            }
            if (changed) { output.Text = string.Join('\n', lines); output.MoveEnd(); }
            return true;
        });
        Render(page);
        navigationButtons[page].SetFocus();
    }

    private void Quit()
    {
        if (running is { IsCompleted: false }) { quitWhenStopped = true; cancellation?.Cancel(); }
        else app.RequestStop();
    }

    private void Render(int page, bool preserveDraft = false)
    {
        if(page == (int)ManagerAction.InstallModernModels) page = 0;
        if (running is { IsCompleted: false }) return;
        selected = page;
        if (!preserveDraft)
        {
            draft = settings with { Actions = new(settings.Actions) };
            options = IsActionPage(page) && settings.Actions.TryGetValue((ManagerAction)page, out var saved) ? saved with { } : new();
        }
        auxiliary = null; music = null; musicRefresh = false; readFields.Clear(); row = 0;
        if (form is not null) { configuration.Remove(form); form.Dispose(); }
        form = new View { X = 0, Y = 0, Width = Dim.Fill(), Height = Dim.Fill(2), CanFocus = true,
            ViewportSettings = ViewportSettingsFlags.HasVerticalScrollBar };
        configuration.Add(form);
        if (cancel.SuperView is null) configuration.Add(cancel, perform);
        perform.Text = page == SettingsPage ? "_Save" : "_Perform";
        string[] titles = ["BUILD AND INSTALL", "BUILD VEHICLE EDITOR", "MANAGE MUSIC", "INSTALL MODERN MODELS", "REPAIR GAME FILES", "UPDATE", "TRANSFER SAVE", "CURATED MODS", "SETTINGS", "TOOLS AND TESTS", "LAUNCH PCVR", "PCVR DIAGNOSTICS"];
        configuration.Title = titles[page];
        switch (page)
        {
            case 0:
                var target = new OptionSelector { X = 1, Y = row, Labels = ["Quest standalone", "Windows PCVR (experimental)", "Linux PCVR (experimental)"],
                    Value = (int)options.Target, Width = Dim.Fill(2), Height = 3 };
                target.SetScheme(Theme()); form.Add(target); row += 4;
                readFields.Add(() => options.Target = (GameTarget)(target.Value ?? 0));
                target.ValueChanged += (_, _) =>
                {
                    foreach (var read in readFields.ToArray()) read();
                    app.Invoke(() => { if (!disposed && selected == 0) Render(0, preserveDraft: true); });
                };
                if (options.Target is GameTarget.WindowsPcvr or GameTarget.LinuxPcvr)
                {
                    Note((options.Target==GameTarget.LinuxPcvr?"Linux":"Windows")+" x64 Release build, checks and local install. No Android tools.");
                    Toggle("Build only (skip install and data copy)", options.PcvrBuildOnly, v => options.PcvrBuildOnly = v);
                    Toggle("Run Vulkan GPU tests (requires an active GPU)", options.PcvrGpuTests, v => options.PcvrGpuTests = v);
                    Toggle("Run audio tests (requires an audio endpoint)", options.PcvrAudioTests, v => options.PcvrAudioTests = v);
                    VariantFields();
                    PcvrDirectories();
                    Note("Existing prepared data and saves are preserved. Use Launch PCVR after installation.");
                    break;
                }
                Note("Build a personal Quest APK from the pinned source.");
                Toggle("Build only (skip installation)", options.BuildOnly, v => options.BuildOnly = v);
                Toggle("Release (requires your signing configuration)", options.Release, v => options.Release = v);
                VariantFields();
                Toggle("Convert and compress radio stations", options.ConvertRadioStations, v => options.ConvertRadioStations = v);
                Note("AAC radio: converts PC stations when copying, otherwise existing headset WAVs. Requires FFmpeg.");
                Licenses(); break;
            case 1:
                Note("Build, test and install the native vehicle editor.");
                Field("Editor output (blank: workplace/vehicle-editor)", options.EditorOutput, v => options.EditorOutput = v, OpenMode.Directory, false);
                Toggle("Create a ZIP package", options.PackageEditor, v => options.PackageEditor = v);
                Note("Windows: Visual Studio 2022 C++ tools. Linux: C++, Ninja and GLFW build dependencies."); break;
            case 2:
                music = new MusicManagementView(app, options, draft); form.Add(music);
                readFields.Add(() => music.ReadOptions(options));
                music.RefreshRequested = () => { musicRefresh = true; Start(); };
                if (loadMusicOnOpen) app.Invoke(() => { if (!disposed && selected == 2 && running is not { IsCompleted: false }) { musicRefresh = true; Start(); } });
                break;
            case 4:
                Note("Repair replacement text and frontend assets on an existing installation.");
                Field("Port gamefiles folder (blank: pinned source cache)", options.GameFiles, v => options.GameFiles = v, OpenMode.Directory);
                Note("Retail game data is not a source for these replacement files."); break;
            case 5:
                Note("Fast-forward official main, build and update the APK.");
                Toggle("Dry run (local checks only)", options.UpdateDryRun, v => options.UpdateDryRun = v);
                Toggle("Build only (skip installation)", options.BuildOnly, v => options.BuildOnly = v);
                Toggle("Release (requires signing configuration)", options.Release, v => options.Release = v);
                Licenses(); break;
            case 6:
                var direction = new OptionSelector { X = 1, Y = row, Labels = ["Quest → PC", "PC → Quest"], Value = (int)options.SaveDirection, Width = Dim.Fill(2), Height = 2 };
                direction.SetScheme(Theme()); form.Add(direction); row += 3;
                readFields.Add(() => options.SaveDirection = (SaveTransferDirection)(direction.Value ?? 0));
                form.Add(new Label { Text = "Save slots · click/Space: toggle · Ctrl+A: all", X = 1, Y = row }); row++;
                var slots = new ListView { X = 1, Y = row, Width = Dim.Fill(2), Height = 9, ShowMarks = true, MarkMultiple = true };
                slots.SetSource(new ObservableCollection<string>(Enumerable.Range(1,9).Select(SaveTransfer.SlotLabel)));
                foreach (var savedSlot in (options.SaveSlots ?? [options.SaveSlot]).Where(s => s is >= 1 and <= 9)) slots.Source!.SetMark(savedSlot - 1, true);
                slots.SetScheme(Theme()); form.Add(slots); row += 10;
                readFields.Add(() => { options.SaveSlots = slots.GetAllMarkedItems().Select(i => i + 1).ToArray(); options.SaveSlot = options.SaveSlots.FirstOrDefault(1); });
                Field("PC save folder (manual saves and Autosave)", string.IsNullOrWhiteSpace(options.PcSaveDirectory) ? SaveTransfer.DefaultPcDirectory(draft) : options.PcSaveDirectory,
                    v => options.PcSaveDirectory = v, OpenMode.Directory, false);
                Note("Close the PC game first. The Quest game will be stopped. Existing destination saves are backed up before replacement.");
                Note("Compatible GTA III/re3 saves only. Backups: " + Path.Combine(draft.Workplace, "save-transfer", "backups"));
                break;
            case (int)ManagerAction.ManageMods:
                var modTarget = new OptionSelector { X = 1, Y = row, Labels = ["Quest standalone", "Windows PCVR (experimental)", "Linux PCVR (experimental)"],
                    Value = (int)options.Target, Width = Dim.Fill(2), Height = 3 };
                modTarget.SetScheme(Theme()); form.Add(modTarget); row += 4;
                readFields.Add(() => options.Target = (GameTarget)(modTarget.Value ?? 0));
                modTarget.ValueChanged += (_, _) => {
                    foreach (var read in readFields.ToArray()) read();
                    app.Invoke(() => { if (!disposed && selected == (int)ManagerAction.ManageMods) Render(selected, preserveDraft: true); });
                };
                if (options.Target is GameTarget.WindowsPcvr or GameTarget.LinuxPcvr) PcvrDirectories();
                options.ModId = CuratedMods.Upstate.Id;
                Note("Upstate Liberty v1.1.2 — map expansion for Classic or Original Xbox assets.");
                var modOperation = new OptionSelector { X = 1, Y = row, Labels = ["Status", "Install", "Enable", "Disable", "Delete", "Validate locally"],
                    Value = (int)options.ModOperation, Width = Dim.Fill(2), Height = 6 };
                modOperation.SetScheme(Theme()); form.Add(modOperation); row += 7;
                readFields.Add(() => options.ModOperation = (ModOperation)(modOperation.Value ?? 0));
                Field("UL.7z package (blank: download v1.1.2)", options.ModArchive, v => options.ModArchive = v, OpenMode.File);
                Note("Install: choose UL.7z or leave blank to download v1.1.2. SHA256 verified.");
                Note("Upstate uses a separate native engine and save folder. Headset acceptance is experimental.");
                Note("Install auto-enables when the selected target has Upstate runtime support. Rebuild PCVR to add it.");
                Note("Status reads the selected target. Validate locally never accesses a device.");
                Note("Disable keeps files. Enable verifies files. Delete keeps saves and the PC archive.");
                Note("Quest stops before changes. Quit PCVR before changes; Launch PCVR loads the enabled mod.");
                break;
            case SettingsPage:
                Note("Shared configuration. Cancel discards edits; Save stores them for every action.");
                Field("Source repository", draft.Repository, v => draft.Repository = v, OpenMode.Directory);
                Field("Workplace", draft.Workplace, v => draft.Workplace = v, OpenMode.Directory, false);
                Field("GTA III PC installation", draft.GameDirectory, v => draft.GameDirectory = v, OpenMode.Directory);
                Field("Android SDK (blank: workplace/.android-sdk)", draft.AndroidSdk, v => draft.AndroidSdk = v, OpenMode.Directory);
                Field("JDK 21 home (blank: download/reuse)", draft.JavaHome, v => draft.JavaHome = v, OpenMode.Directory);
                Field("ADB executable (blank: SDK/PATH)", draft.Adb, v => draft.Adb = v, OpenMode.File);
                Field("ffprobe executable", draft.Ffprobe, v => draft.Ffprobe = v, OpenMode.File);
                Field("FFmpeg executable", draft.Ffmpeg, v => draft.Ffmpeg = v, OpenMode.File);
                Field("CMake executable (blank: PATH/SDK)", draft.CMake, v => draft.CMake = v, OpenMode.File);
                Field("Python 3 executable (auxiliary tools)", draft.Python, v => draft.Python = v, OpenMode.File);
                Field("PowerShell 7 executable (auxiliary tests)", draft.PowerShell, v => draft.PowerShell = v, OpenMode.File);
                Field("Bash executable (auxiliary tools)", draft.Bash, v => draft.Bash = v, OpenMode.File);
                Field("7-Zip executable (blank: installed 7zz/7z)", draft.SevenZip, v => draft.SevenZip = v, OpenMode.File);
                Note("Windows PCVR build tools. These are separate from the Android toolchain.");
                Field("Vulkan SDK directory", draft.VulkanSdk, v => draft.VulkanSdk = v, OpenMode.Directory);
                Field("PCVR glslc executable (blank: Vulkan SDK/Bin)", draft.PcvrShaderCompiler, v => draft.PcvrShaderCompiler = v, OpenMode.File);
                Field("OpenXR-SDK 1.1.58 sources (blank: pinned download/cache)", draft.PcvrOpenXrSource, v => draft.PcvrOpenXrSource = v, OpenMode.Directory);
                PcvrDirectories();
                Toggle("Allow pinned build-tool downloads", draft.AllowToolDownloads, v => draft.AllowToolDownloads = v);
                Note("Saved to " + store.FilePath); break;
            case ToolsPage:
                Note("Additional project tools and regression checks.");
                Note("Select a tool. Specialised tools use the configured Python, PowerShell or Bash runtime.");
                foreach (var tool in AuxiliaryTools.Catalog)
                {
                    var button = new Button { Text = tool.Category + ": " + tool, X = 1, Y = row, Width = Dim.Fill(2), ShadowStyle = ShadowStyles.None };
                    button.Accepting += (_, e) => { e.Handled = true; RenderTool(tool); };
                    form.Add(button); row += 2;
                }
                break;
            case (int)ManagerAction.LaunchPcvr:
                DesktopTarget();
                Note("Launch the installed desktop PCVR game with separate local game data and saves.");
                Field("Quest profile (blank: native PCVR; restart to change)", options.PcvrQuestProfile, v => options.PcvrQuestProfile = v, OpenMode.File);
                Note("Matches captured Quest workload; PC timings are not Quest performance.");
                PcvrDirectories();
                Note("Connect the headset to your PC OpenXR runtime. Quit in-game to return here.");
                Note("Cancel stops the game process; use in-game Quit for a normal exit.");
                break;
            case (int)ManagerAction.DiagnosePcvr:
                DesktopTarget();
                Note("Check the active OpenXR runtime, Vulkan GPU and headset session.");
                PcvrDirectories();
                Note("Requires a PCVR install. No game assets or ADB. Each probe times out after 60 seconds.");
                break;
        }
        if (page != 2) form.SetContentHeight(row + 1);
    }

    private void Note(string text)
    {
        var label = new Label { Text = text, X = 1, Y = row, Width = Dim.Fill(2), Height = 2 };
        form!.Add(label); row += 3;
    }
    private void DesktopTarget()
    {
        if(options.Target==GameTarget.Quest)options.Target=PcvrWorkflow.HostTarget;
        var selector=new OptionSelector {X=1,Y=row,Labels=["Windows PCVR","Linux PCVR"],Value=options.Target==GameTarget.LinuxPcvr?1:0,Width=Dim.Fill(2),Height=2};
        selector.SetScheme(Theme());form!.Add(selector);row+=3;
        readFields.Add(()=>options.Target=selector.Value==1?GameTarget.LinuxPcvr:GameTarget.WindowsPcvr);
        selector.ValueChanged+=(_,_)=>{foreach(var read in readFields.ToArray())read();app.Invoke(()=>{if(!disposed)Render(selected,preserveDraft:true);});};
    }
    private void PcvrDirectories()
    {
        bool linux=options.Target==GameTarget.LinuxPcvr || (selected==SettingsPage && OperatingSystem.IsLinux());
        var target=linux?"linux-pcvr":"windows-pcvr";
        Field("PCVR app (blank: workplace/"+target+"/app)",linux?draft.LinuxPcvrInstallDirectory:draft.PcvrInstallDirectory,v=>{if(linux)draft.LinuxPcvrInstallDirectory=v;else draft.PcvrInstallDirectory=v;},OpenMode.Directory,false);
        Field("PCVR data (blank: workplace/"+target+"/data)",linux?draft.LinuxPcvrDataDirectory:draft.PcvrDataDirectory,v=>{if(linux)draft.LinuxPcvrDataDirectory=v;else draft.PcvrDataDirectory=v;},OpenMode.Directory,false);
        Field("PCVR saves (blank: workplace/"+target+"/user)",linux?draft.LinuxPcvrUserDirectory:draft.PcvrUserDirectory,v=>{if(linux)draft.LinuxPcvrUserDirectory=v;else draft.PcvrUserDirectory=v;},OpenMode.Directory,false);
        Field("Desktop SDL3/FFmpeg prefix (blank: repository/.cache/desktop-deps/"+(linux?"linux":"windows")+")",linux?draft.LinuxPcvrDependencyPrefix:draft.PcvrDependencyPrefix,v=>{if(linux)draft.LinuxPcvrDependencyPrefix=v;else draft.PcvrDependencyPrefix=v;},OpenMode.Directory,false);
        if(linux)Field("Linux OpenXR sources (blank: pinned cache)",draft.LinuxPcvrOpenXrSource,v=>draft.LinuxPcvrOpenXrSource=v,OpenMode.Directory,false);
    }
    private void Field(string label, string value, Action<string> set, OpenMode mode, bool mustExist = true, bool enabled = true)
    {
        form!.Add(new Label { Text = label, X = 1, Y = row, Width = Dim.Fill(2) });
        var field = new TextField { Text = value, X = 1, Y = row + 1, Width = Dim.Fill(13) };
        field.SetScheme(Theme());
        var browse = new Button { Text = "Browse", X = Pos.AnchorEnd(11), Y = row + 1, ShadowStyle = ShadowStyles.None };
        field.Enabled = browse.Enabled = enabled;
        browse.Accepting += (_, e) =>
        {
            e.Handled = true;
            using var dialog = new ManagerFileDialog { Title = label, OpenMode = mode, MustExist = mustExist };
            dialog.SetScheme(Theme());
            var home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            var start = string.IsNullOrWhiteSpace(field.Text) ? (Directory.Exists(draft.Workplace) ? draft.Workplace : home) : field.Text;
            if (mode == OpenMode.Directory)
            {
                try
                {
                    var folder = new DirectoryInfo(Path.GetFullPath(start));
                    while (folder is not null && !folder.Exists) folder = folder.Parent;
                    start = folder?.FullName ?? home;
                }
                catch (Exception error) when (error is ArgumentException or IOException or NotSupportedException) { start = home; }
            }
            dialog.Path = start;
            app.Run(dialog);
            if (!dialog.Canceled) field.Text = dialog.Path;
        };
        form.Add(field, browse); row += 3;
        readFields.Add(() => set(field.Text.Trim()));
    }
    private CheckBox Toggle(string label, bool value, Action<bool> set)
    {
        var check = new CheckBox { Text = label, Value = value ? CheckState.Checked : CheckState.UnChecked, X = 1, Y = row, Width = Dim.Fill(2) };
        check.SetScheme(Theme());
        form!.Add(check); row += 2;
        readFields.Add(() => set(check.Value == CheckState.Checked));
        return check;
    }
    private void VariantFields()
    {
        // Read old saved requests without retaining a second installation page.
        options.InstallPcVariant = VariantInstaller.WantsPc(options);
        options.CopyGameData = options.PcvrPrepareData = false;
        if(string.IsNullOrWhiteSpace(options.XboxIsoPath) && settings.Actions.TryGetValue(ManagerAction.InstallModernModels,out var old))
            options.XboxIsoPath = old.ModelArchive;
        var pc=Toggle("Install PC variant",options.InstallPcVariant,v=>options.InstallPcVariant=v);
        Field("Owned GTA III PC installation",draft.GameDirectory,v=>draft.GameDirectory=v,OpenMode.Directory,true,options.InstallPcVariant);
        var xbox=Toggle("Install Original Xbox variant",options.InstallXboxVariant,v=>options.InstallXboxVariant=v);
        Field("Original Xbox US 1.0 ISO",options.XboxIsoPath,v=>options.XboxIsoPath=v,OpenMode.File,true,options.InstallXboxVariant);
        void Refresh() {foreach(var read in readFields.ToArray())read();app.Invoke(()=>{if(!disposed && selected==0)Render(0,preserveDraft:true);});}
        pc.ValueChanged+=(_,_)=>Refresh();xbox.ValueChanged+=(_,_)=>Refresh();
        Note("Select either or both variants. Leave both unchecked to update only the application. Saves and existing variants are preserved.");
    }
    private void Licenses()
    {
        Toggle("I accept Google's Android SDK licenses (if installation is needed)", false, v => options.AcceptSdkLicenses = v);
        Note("License terms: developer.android.com/studio/terms");
    }

    private void RenderTool(AuxiliaryTool tool)
    {
        Render(ToolsPage);
        foreach (var view in form!.SubViews.ToArray()) { form.Remove(view); view.Dispose(); }
        row = 0; readFields.Clear(); auxiliary = tool.Path;
        Note(tool.Category + " / " + tool);
        Note(tool.Description);
        Note("Parameters: " + (tool.Parameters.Length == 0 ? "use --help for full usage" : string.Join(" ", tool.Parameters)));
        form.Add(new Label { Text = "Arguments (quote paths containing spaces)", X = 1, Y = row, Width = Dim.Fill(2) });
        var field = new TextField { X = 1, Y = row + 1, Width = Dim.Fill(2) };
        form.Add(field); readFields.Add(() => auxiliaryArguments = field.Text);
        row += 3;
        Note("Workplace: " + settings.Workplace + "/auxiliary. Source snapshots and outputs stay local.");
        form.SetContentHeight(row + 1); field.SetFocus();
    }

    private void Start()
    {
        if (running is { IsCompleted: false }) return;
        foreach (var read in readFields) read();
        try
        {
            draft.Validate();
            if (selected == SettingsPage)
            { store.Save(draft); settings = draft with { }; pending.Enqueue("Settings saved to " + store.FilePath); navigationButtons[SettingsPage].SetFocus(); return; }
            if (selected == ToolsPage && auxiliary is null) { pending.Enqueue("Select a tool first."); return; }
            if (IsActionPage(selected) && !musicRefresh)
            {
                // License consent is explicit for each run rather than inferred from a saved checkbox.
                draft.Actions[(ManagerAction)selected] = options with { AcceptSdkLicenses = false, MusicRevision = "", MusicOrder = [], MusicDeleted = [], ModOperation = ModOperation.Status };
                store.Save(draft); settings = draft with { Actions = new(draft.Actions) };
            }
        }
        catch (Exception error) when (error is IOException or ArgumentException or UnauthorizedAccessException)
        { pending.Enqueue("Configuration error: " + error.Message); return; }
        cancellation?.Dispose(); cancellation = new();
        var action = (ManagerAction)selected;
        var snapshot = draft with { };
        var selectedOptions = options with { };
        var selectedTool = auxiliary;
        var arguments = auxiliaryArguments;
        var refresh = musicRefresh; musicRefresh = false; var musicTarget = music;
        form!.Enabled = false; perform.Enabled = false;
        foreach (var button in navigationButtons) button.Enabled = false;
        logPath = Path.Combine(store.DirectoryPath, "logs", DateTime.Now.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N")[..6] + ".log");
        var token = cancellation.Token;
        running = Task.Run(async () =>
        {
            using var log = new PrivateLog(logPath);
            log.Line += pending.Enqueue;
            try
            {
                log.Write("Started " + action + ". Log: " + logPath);
                var runner = new CommandRunner(log);
                var workflows = new Workflows(snapshot, runner, log, ChooseDevice);
                if (refresh) { var library = await workflows.MusicLibrary(token); app.Invoke(() => musicTarget?.Load(library)); }
                else if (selectedTool is not null) await new AuxiliaryTools(snapshot, runner, log, ChooseDevice).Run(selectedTool, AuxiliaryTools.ParseArguments(arguments), token);
                else
                {
                    await workflows.Perform(action, selectedOptions, token);
                    if (musicTarget is not null && !selectedOptions.ValidateMusicOnly) { var library = await workflows.MusicLibrary(token); app.Invoke(() => musicTarget.Load(library)); }
                }
                log.Write("Action completed.");
            }
            catch (OperationCanceledException) { log.Write("Action cancelled. Completed build products are retained."); }
            catch (Exception error) { log.Write("Action failed: " + error.Message); }
            finally
            {
                app.Invoke(() =>
                {
                    if (disposed) return;
                    form!.Enabled = true; perform.Enabled = true;
                    foreach (var button in navigationButtons) button.Enabled = true;
                    navigationButtons[selected].SetFocus();
                    if (quitWhenStopped) app.RequestStop();
                });
            }
        });
    }

    private Task<int?> ChooseDevice(int count)
    {
        var completion = new TaskCompletionSource<int?>(TaskCreationOptions.RunContinuationsAsynchronously);
        app.Invoke(() =>
        {
            var choices = Enumerable.Range(1, count).Select(i => "Quest 3 #" + i).Append("Cancel").ToArray();
            var selectedDevice = MessageBox.Query(app, "Select headset", "Multiple eligible Quest 3 headsets are connected. Select the target by connection order, or connect only the intended headset.", choices);
            completion.SetResult(selectedDevice >= 0 && selectedDevice < count ? selectedDevice : null);
        });
        return completion.Task.WaitAsync(cancellation!.Token);
    }
    public void Dispose() { disposed = true; cancellation?.Dispose(); Window.Dispose(); }
}
