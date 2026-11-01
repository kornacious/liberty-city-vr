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
    public const int SettingsPage = 7;
    public const int ToolsPage = 8;
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
        Window.SetScheme(Theme());
        output.SetScheme(Theme());
        cancel.ShadowStyle = perform.ShadowStyle = ShadowStyles.None;
        Window.Add(navigation, configuration, activity);
        activity.Add(output);
        var footer = new Label { Text = "Tab: move  |  Enter: select  |  Ctrl+Q: quit", Y = Pos.AnchorEnd(1), Width = Dim.Fill() };
        Window.Add(footer);
        string[] pages = ["Build and install", "Vehicle editor", "Manage music", "Modern models", "Repair game files", "Update", "Transfer save", "Settings", "Tools and tests"];
        for (var i = 0; i < pages.Length; i++)
        {
            var index = i;
            var button = new Button { Text = pages[i], X = 0, Y = i * 2, Width = Dim.Fill(), ShadowStyle = ShadowStyles.None };
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

    private void Render(int page)
    {
        if (running is { IsCompleted: false }) return;
        selected = page; draft = settings with { Actions = new(settings.Actions) };
        options = page < SettingsPage && settings.Actions.TryGetValue((ManagerAction)page, out var saved) ? saved with { } : new();
        auxiliary = null; music = null; musicRefresh = false; readFields.Clear(); row = 0;
        if (form is not null) { configuration.Remove(form); form.Dispose(); }
        form = new View { X = 0, Y = 0, Width = Dim.Fill(), Height = Dim.Fill(2), CanFocus = true,
            ViewportSettings = ViewportSettingsFlags.HasVerticalScrollBar };
        configuration.Add(form);
        if (cancel.SuperView is null) configuration.Add(cancel, perform);
        perform.Text = page == SettingsPage ? "_Save" : "_Perform";
        string[] titles = ["BUILD AND INSTALL", "BUILD VEHICLE EDITOR", "MANAGE MUSIC", "INSTALL MODERN MODELS", "REPAIR GAME FILES", "UPDATE", "TRANSFER SAVE", "SETTINGS", "TOOLS AND TESTS"];
        configuration.Title = titles[page];
        switch (page)
        {
            case 0:
                Note("Build a personal Quest APK from the pinned source.");
                Toggle("Build only (skip installation)", options.BuildOnly, v => options.BuildOnly = v);
                Toggle("Release (requires your signing configuration)", options.Release, v => options.Release = v);
                Toggle("Copy my GTA III game data", options.CopyGameData, v => options.CopyGameData = v);
                Field("GTA III PC installation", draft.GameDirectory, v => draft.GameDirectory = v, OpenMode.Directory);
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
            case 3:
                Note("Build and verify the pinned Xbox HD re3 Modern overlay.");
                Field("Source ZIP (blank: cached pack)", options.ModelArchive, v => options.ModelArchive = v, OpenMode.File);
                Field("Or extracted pack folder", options.ModelSource, v => options.ModelSource = v, OpenMode.Directory);
                Field("Output folder named modern (blank: workplace)", options.ModelOutput, v => options.ModelOutput = v, OpenMode.Directory, false);
                Toggle("Download the supported pack from ModDB (1.45 GiB)", options.DownloadModels, v => options.DownloadModels = v);
                Toggle("Build only (skip model installation)", options.BuildOnly, v => options.BuildOnly = v); break;
            case 4:
                Note("Repair replacement text and frontend assets on an existing installation.");
                Field("Port gamefiles folder (blank: pinned source cache)", options.GameFiles, v => options.GameFiles = v, OpenMode.Directory);
                Note("Retail game data is not a source for these replacement files."); break;
            case 5:
                Note("Fast-forward official master, build and update the APK.");
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
            case SettingsPage:
                Note("Shared configuration. Cancel discards edits; Save stores them for every action.");
                Field("Source repository", draft.Repository, v => draft.Repository = v, OpenMode.Directory);
                Field("Workplace", draft.Workplace, v => draft.Workplace = v, OpenMode.Directory, false);
                Field("GTA III PC installation", draft.GameDirectory, v => draft.GameDirectory = v, OpenMode.Directory);
                Field("Android SDK (blank: workplace/.android-sdk)", draft.AndroidSdk, v => draft.AndroidSdk = v, OpenMode.Directory);
                Field("JDK 21 home (blank: download/reuse)", draft.JavaHome, v => draft.JavaHome = v, OpenMode.Directory);
                Field("ADB executable (blank: SDK/PATH)", draft.Adb, v => draft.Adb = v, OpenMode.File);
                Field("ffprobe executable", draft.Ffprobe, v => draft.Ffprobe = v, OpenMode.File);
                Field("CMake executable (blank: PATH/SDK)", draft.CMake, v => draft.CMake = v, OpenMode.File);
                Field("Python 3 executable (auxiliary tools)", draft.Python, v => draft.Python = v, OpenMode.File);
                Field("PowerShell 7 executable (auxiliary tests)", draft.PowerShell, v => draft.PowerShell = v, OpenMode.File);
                Field("Bash executable (auxiliary tools)", draft.Bash, v => draft.Bash = v, OpenMode.File);
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
        }
        if (page != 2) form.SetContentHeight(row + 1);
    }

    private void Note(string text)
    {
        var label = new Label { Text = text, X = 1, Y = row, Width = Dim.Fill(2), Height = 2 };
        form!.Add(label); row += 3;
    }
    private void Field(string label, string value, Action<string> set, OpenMode mode, bool mustExist = true)
    {
        form!.Add(new Label { Text = label, X = 1, Y = row, Width = Dim.Fill(2) });
        var field = new TextField { Text = value, X = 1, Y = row + 1, Width = Dim.Fill(13) };
        field.SetScheme(Theme());
        var browse = new Button { Text = "Browse", X = Pos.AnchorEnd(11), Y = row + 1, ShadowStyle = ShadowStyles.None };
        browse.Accepting += (_, e) =>
        {
            e.Handled = true;
            using var dialog = new OpenDialog { Title = label, OpenMode = mode, MustExist = mustExist };
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
    private void Toggle(string label, bool value, Action<bool> set)
    {
        var check = new CheckBox { Text = label, Value = value ? CheckState.Checked : CheckState.UnChecked, X = 1, Y = row, Width = Dim.Fill(2) };
        check.SetScheme(Theme());
        form!.Add(check); row += 2;
        readFields.Add(() => set(check.Value == CheckState.Checked));
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
            if (selected < SettingsPage && !musicRefresh)
            {
                // License consent is explicit for each run rather than inferred from a saved checkbox.
                draft.Actions[(ManagerAction)selected] = options with { AcceptSdkLicenses = false, MusicRevision = "", MusicOrder = [], MusicDeleted = [] };
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
