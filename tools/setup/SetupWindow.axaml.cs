using System.Diagnostics;
using Avalonia;
using Avalonia.Automation;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using LibertyManager;

namespace LibertySetup;

public sealed partial class SetupWindow : Window
{
    private static readonly string[] Steps = ["Where you’ll play", "Your game files", "Before you install", "Installation"];
    private const string Documentation = "https://github.com/kornacious/liberty-city-vr/blob/main/";
    private const string AndroidTerms = "https://developer.android.com/studio/terms";
    private readonly ManagerSettings settings;
    private readonly string logsDirectory;
    private GameTarget target = GameTarget.Quest;
    private string? pcFolder;
    private string? xboxImage;
    private bool allowUnverifiedXboxIso;
    private int step;
    private bool helpOpen;
    private bool toolLocationsOpen;
    private bool accepted;
    private bool releaseSigning;
    private bool closeAfterWork;
    private string? error;
    private string? logPath;
    private SetupReadiness? readiness;
    private InstallProgress? progress;
    private InstallResult? result;
    private CancellationTokenSource? work;
    private TextBlock? progressLabel;
    private ProgressBar? progressBar;
    private TextBlock? progressCount;

    private bool Quest => target == GameTarget.Quest;
    private SetupSelection Selection => new(target, pcFolder, xboxImage, accepted, releaseSigning, allowUnverifiedXboxIso);

    public SetupWindow()
    {
        InitializeComponent();
        var store = new SettingsStore();
        logsDirectory = Path.Combine(store.DirectoryPath, "logs");
        try { settings = store.Load(); }
        catch (Exception)
        {
            settings = new();
            error = "Your saved manager settings could not be read. Setup is using the defaults for this session.";
        }
        // A packaged Setup travels with its matching source kit.
        var packagedKit = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, ".."));
        if (File.Exists(Path.Combine(packagedKit, "overlay/android/app/build.gradle.kts")))
            settings.Repository = packagedKit;
        var options = settings.Actions.GetValueOrDefault(ManagerAction.BuildAndInstall);
        releaseSigning = options is { Target: GameTarget.Quest, Release: true };
        if (options?.Target == SetupInstaller.DesktopTarget) target = options.Target;
        if (Directory.Exists(settings.GameDirectory)) pcFolder = settings.GameDirectory;
        if (File.Exists(options?.XboxIsoPath)) xboxImage = options!.XboxIsoPath;

        NextButton.Click += async (_, _) => await Next();
        BackButton.Click += (_, _) => Back();
        HelpButton.Click += (_, _) =>
        {
            error = null;
            helpOpen = !helpOpen;
            Render();
        };
        Closing += (_, args) =>
        {
            if (work is null) return;
            closeAfterWork = true;
            args.Cancel = true;
            Stop();
        };
        Render();
    }

    private void Render(bool focusHeading = true)
    {
        Page.Children.Clear();
        StepList.Children.Clear();
        for (var index = 0; index < Steps.Length; index++)
        {
            var marker = index == step ? "●" : index < step ? "✓" : "○";
            var label = Copy($"{marker}  {Steps[index]}", index == step ? "current" : "caption");
            AutomationProperties.SetName(label, index == step ? $"{Steps[index]}, current step" : Steps[index]);
            StepList.Children.Add(label);
        }

        HelpButton.IsEnabled = work is null;
        HelpButton.Content = helpOpen ? "Close help" : "Setup help";
        BackButton.IsVisible = !helpOpen && (step > 0 || work is not null);
        BackButton.Content = work is null ? "Back" : "Cancel";
        BackButton.IsEnabled = work?.IsCancellationRequested != true;
        NextButton.IsVisible = !helpOpen;
        NextButton.IsEnabled = work is null;
        NextButton.Content = step switch
        {
            2 when readiness is null || !readiness.CanInstall => "Check again",
            2 => "Install",
            3 when result?.Status == InstallStatus.Installed => "Done",
            3 => "Try again",
            _ => "Continue"
        };
        if (step == 2 && readiness is { CanInstall: true, NeedsAndroidConsent: true } && !accepted)
            NextButton.IsEnabled = false;
        if (work is not null && step == 3) NextButton.IsVisible = false;

        var title = Copy(helpOpen ? "Setup help" : step switch
        {
            0 => "Where will you play?",
            1 => "Find your GTA III files",
            2 => "Check what’s needed",
            3 when result is not null => ResultTitle(result.Status),
            _ => "Installing Liberty City VR"
        }, "heading");
        title.Focusable = true;
        title.IsTabStop = false;
        Page.Children.Add(title);
        if (error is not null) Page.Children.Add(Copy(error, "status"));
        if (helpOpen) RenderHelp();
        else
            switch (step)
            {
                case 0: RenderTargets(); break;
                case 1: RenderSources(); break;
                case 2: RenderChecklist(); break;
                case 3: RenderInstall(); break;
            }
        if (focusHeading)
        {
            PageScroll.Offset = default;
            Dispatcher.UIThread.Post(() => { if (title.Parent is not null) title.Focus(); }, DispatcherPriority.Loaded);
        }
    }

    private void RenderTargets()
    {
        Page.Children.Add(Copy("Choose where to install Liberty City VR."));
        AddTarget(GameTarget.Quest, "Quest 3 standalone", "Install on your headset. Play without a PC.");
        AddTarget(SetupInstaller.DesktopTarget, OperatingSystem.IsWindows() ? "Windows PCVR" : "Linux PCVR (experimental)",
            "Run the game on this computer and play through your connected headset.");
    }

    private void AddTarget(GameTarget value, string name, string description)
    {
        var radio = new RadioButton
        {
            GroupName = "Target",
            IsChecked = target == value,
            Content = Column(Copy(name, "label"), Copy(description, "caption"))
        };
        AutomationProperties.SetName(radio, name);
        AutomationProperties.SetHelpText(radio, description);
        radio.IsCheckedChanged += (_, _) => { if (radio.IsChecked == true) target = value; };
        Page.Children.Add(Card(radio));
    }

    private void RenderSources()
    {
        Page.Children.Add(Copy("Choose either copy or both. Setup prepares a separate copy and keeps your original files on this computer."));
        if (work is not null)
        {
            Page.Children.Add(new ProgressBar { IsIndeterminate = true });
            Page.Children.Add(Copy(work.IsCancellationRequested ? "Stopping the checks…" : "Checking your game files…", "status"));
            return;
        }
        AddPath("PC copy", "Choose the folder that contains the data, models, and audio folders.", pcFolder, false, value => pcFolder = value);
        AddPath("Original Xbox copy", "Choose your GTA III US disc image (.iso or .xiso.iso).", xboxImage, true, value =>
        {
            xboxImage = value;
            allowUnverifiedXboxIso = false;
        },
            [new FilePickerFileType("Xbox disc image") { Patterns = ["*.iso", "*.ISO"] }]);
    }

    private void RenderChecklist()
    {
        var prebuilt = PcvrWorkflow.SetupBundle(settings.Repository, target) is not null;
        Page.Children.Add(Copy(Quest ? "Setup builds the game on this computer before installing it on your headset." :
            prebuilt ? "Setup installs the included PCVR build. Close the game first if you’re reinstalling." :
            "Setup builds the game on this computer. Close the game first if you’re reinstalling."));
        if (work is not null)
        {
            Page.Children.Add(new ProgressBar { IsIndeterminate = true });
            Page.Children.Add(Copy(work.IsCancellationRequested ? "Stopping the checks…" : "Checking the source kit and tools…", "status"));
            return;
        }
        if (readiness is null) return;
        foreach (var check in readiness.Checks)
        {
            var status = check.Status switch
            {
                SetupCheckStatus.Ready => "Ready",
                SetupCheckStatus.Automatic => "During setup",
                _ => "Action needed"
            };
            Page.Children.Add(Card(Column(Copy($"{check.Title}  ·  {status}", check.Status == SetupCheckStatus.NeedsAttention ? "status" : "label"),
                Copy(check.Message, "caption"))));
        }
        if (readiness.NeedsAndroidConsent)
        {
            var consent = new CheckBox { Content = Copy("I accept Google’s Android SDK license terms"), IsChecked = accepted };
            consent.IsCheckedChanged += (_, _) =>
            {
                accepted = consent.IsChecked == true;
                NextButton.IsEnabled = accepted && readiness.CanInstall;
            };
            Page.Children.Add(Card(Column(Link("Read Android SDK license terms", AndroidTerms), consent)));
        }
        if (readiness.CanInstall)
            Page.Children.Add(ButtonFor("Check again", async () => await Check(), HorizontalAlignment.Left));
    }

    private void RenderInstall()
    {
        if (result is not null)
        {
            Page.Children.Add(Copy(result.Message));
            if (result.Status is InstallStatus.Cancelled or InstallStatus.Failed)
                Page.Children.Add(Copy($"Stopped at: {StageTitle(result.Stage)}.", "status"));
            if (result.InstallFolder is { } folder) Page.Children.Add(Link("Open install folder", folder));
            if (logPath is not null && File.Exists(logPath)) Page.Children.Add(Link("Open setup log", logPath));
            return;
        }
        Page.Children.Add(Copy($"Installing for {TargetName()}. Preparing the game files can take a while."));
        progressLabel = Copy("", "label");
        progressBar = new ProgressBar();
        AutomationProperties.SetName(progressBar, "Installation stage progress");
        progressCount = Copy("", "caption");
        Page.Children.Add(Card(Column(progressLabel, progressBar, progressCount)));
        Page.Children.Add(Copy("You can cancel and return to setup. Keep your computer awake while the installation runs.", "caption"));
        UpdateProgress(progress ?? new InstallProgress(InstallStage.CheckingSetup));
    }

    private void RenderHelp()
    {
        Page.Children.Add(Copy("Use the matching source kit and keep it together. Setup uses the tools already installed when it can."));
        if (OperatingSystem.IsLinux())
        {
            var menuStatus = Copy("", "caption");
            Page.Children.Add(ButtonFor("Add Setup to app menu", () =>
            {
                try
                {
                    SetupDesktopEntry.Register();
                    menuStatus.Text = "Liberty City VR Setup was added to your app menu.";
                }
                catch (Exception exception) { menuStatus.Text = "Couldn’t add the shortcut. " + exception.Message; }
                menuStatus.IsVisible = true;
                return Task.CompletedTask;
            }, HorizontalAlignment.Left));
            menuStatus.IsVisible = false;
            Page.Children.Add(menuStatus);
        }
        Page.Children.Add(Link(Quest ? "Open the Quest 3 setup guide" : "Open the PCVR prerequisites guide",
            Documentation + (Quest ? "BUILDING.md#quest-build-and-deployment" : "overlay/desktop/README.md#dependencies")));
        AddPath("Liberty City VR source kit", "The folder you extracted. It contains the overlay and patches folders.", settings.Repository, false,
            value => settings.Repository = value ?? "", clearable: false);
        AddPath("Build folder", OperatingSystem.IsWindows()
                ? "Use a short, writable location like C:\\LCVRBuild with room for downloaded tools and prepared files."
                : "Use a separate, writable folder with room for downloaded tools and prepared game files.", settings.Workplace, false,
            value => settings.Workplace = value ?? "", clearable: false);
        var downloads = new CheckBox { IsChecked = settings.AllowToolDownloads, Content = Copy("Allow verified tool archive downloads") };
        downloads.IsCheckedChanged += (_, _) => { settings.AllowToolDownloads = downloads.IsChecked == true; ResetChecks(); };
        Page.Children.Add(downloads);
        var toolPaths = Column();
        if (Quest)
        {
            AddTool(toolPaths, "Android SDK", settings.AndroidSdk, value => settings.AndroidSdk = value, false);
            AddTool(toolPaths, "Java 21 (JDK) folder", settings.JavaHome, value => settings.JavaHome = value, false);
            AddTool(toolPaths, "ADB executable", settings.Adb, value => settings.Adb = value);
            AddTool(toolPaths, "Texture preparation executable", settings.AstcEncoder, value => settings.AstcEncoder = value);
            var signing = new CheckBox { IsChecked = releaseSigning, Content = Copy("Reuse my existing release signing setup") };
            signing.IsCheckedChanged += (_, _) => { releaseSigning = signing.IsChecked == true; ResetChecks(); };
            toolPaths.Children.Add(Card(Column(signing, Copy("Use this if your current Quest installation was built with a private release key.", "caption"))));
        }
        else if (OperatingSystem.IsWindows())
        {
            AddTool(toolPaths, "Vulkan SDK folder", settings.VulkanSdk, value => settings.VulkanSdk = value, false);
            AddTool(toolPaths, "Desktop libraries folder", settings.PcvrDependencyPrefix, value => settings.PcvrDependencyPrefix = value, false);
        }
        else AddTool(toolPaths, "Desktop libraries folder", settings.LinuxPcvrDependencyPrefix, value => settings.LinuxPcvrDependencyPrefix = value, false);
        AddTool(toolPaths, "CMake executable", settings.CMake, value => settings.CMake = value);
        AddTool(toolPaths, "FFmpeg executable", settings.Ffmpeg == "ffmpeg" ? "" : settings.Ffmpeg, value => settings.Ffmpeg = string.IsNullOrEmpty(value) ? "ffmpeg" : value);
        AddTool(toolPaths, "ffprobe executable", settings.Ffprobe == "ffprobe" ? "" : settings.Ffprobe, value => settings.Ffprobe = string.IsNullOrEmpty(value) ? "ffprobe" : value);
        var locations = new Expander { Header = "Tool locations", Content = toolPaths, IsExpanded = toolLocationsOpen };
        locations.PropertyChanged += (_, args) =>
        {
            if (args.Property == Expander.IsExpandedProperty) toolLocationsOpen = locations.IsExpanded;
        };
        Page.Children.Add(locations);
    }

    private void AddTool(StackPanel panel, string name, string current, Action<string> action, bool file = true) =>
        AddPath(name, "When blank, Setup uses normal tool discovery.", current, file, value => action(value ?? ""), parent: panel);

    private void AddPath(string name, string description, string? path, bool file, Action<string?> set,
        IReadOnlyList<FilePickerFileType>? filter = null, bool clearable = true, StackPanel? parent = null)
    {
        var hasPath = !string.IsNullOrWhiteSpace(path);
        var choose = ButtonFor(hasPath ? "Change" : file ? "Choose file" : "Choose folder", async () =>
        {
            try
            {
                var startingFolder = hasPath && Path.IsPathFullyQualified(path!)
                    ? file ? Path.GetDirectoryName(path) : path
                    : null;
                using var start = startingFolder is not null && Directory.Exists(startingFolder)
                    ? await StorageProvider.TryGetFolderFromPathAsync(startingFolder) : null;
                IStorageItem? item;
                if (file)
                    item = (await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
                        { Title = name, SuggestedStartLocation = start, FileTypeFilter = filter })).FirstOrDefault();
                else
                    item = (await StorageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions
                        { Title = name, SuggestedStartLocation = start })).FirstOrDefault();
                using (item)
                {
                    if (item is null) return;
                    var local = item.TryGetLocalPath() ?? throw new IOException("Choose a file or folder on this computer.");
                    set(local);
                }
                error = null;
                ResetChecks();
            }
            catch (Exception exception) { error = exception.Message; }
            Render(focusHeading: !helpOpen);
        });
        AutomationProperties.SetName(choose, $"Choose {name}");
        var actions = new WrapPanel { ItemSpacing = 8, LineSpacing = 4, Children = { choose } };
        if (clearable && hasPath)
        {
            var remove = ButtonFor("Clear", () => { set(null); ResetChecks(); Render(focusHeading: !helpOpen); return Task.CompletedTask; });
            AutomationProperties.SetName(remove, $"Clear {name}");
            actions.Children.Add(remove);
        }
        (parent ?? Page).Children.Add(Card(Column(Copy(name, "label"), Copy(description, "caption"),
            Copy(hasPath ? path! : "Nothing selected", "caption"), actions)));
    }

    private async Task Next()
    {
        if (work is not null) return;
        error = null;
        switch (step)
        {
            case 0:
                step = 1;
                Render();
                break;
            case 1:
                await CheckSources();
                break;
            case 2 when readiness?.CanInstall == true && (!readiness.NeedsAndroidConsent || accepted):
                await Install();
                break;
            case 2:
                await Check();
                break;
            case 3 when result?.Status == InstallStatus.Installed:
                Close();
                break;
            case 3:
                step = 2;
                await Check();
                break;
        }
    }

    private void Back()
    {
        if (work is not null) { Stop(); return; }
        error = null;
        step = Math.Max(0, step - 1);
        Render();
    }

    private void ResetChecks()
    {
        readiness = null;
        accepted = false;
    }

    private async Task CheckSources()
    {
        allowUnverifiedXboxIso = false;
        var selection = Selection;
        string? mismatch = null;
        using var check = new CancellationTokenSource();
        work = check;
        Render();
        try
        {
            mismatch = await Task.Run(() => SetupInstaller.CheckGameFiles(selection, checkXboxIntegrity: true, token: check.Token), check.Token);
        }
        catch (OperationCanceledException) when (check.IsCancellationRequested) { }
        catch (Exception exception) { error = exception.Message; }
        finally { work = null; }
        if (closeAfterWork) { Close(); return; }
        if (check.IsCancellationRequested || error is not null) { Render(); return; }
        if (mismatch is not null)
        {
            allowUnverifiedXboxIso = await ConfirmUnverifiedXboxImport(mismatch);
            if (!allowUnverifiedXboxIso) { Render(); return; }
        }
        step = 2;
        ResetChecks();
        await Check();
    }

    private async Task<bool> ConfirmUnverifiedXboxImport(string mismatch)
    {
        var dialog = new Window
        {
            Title = "Xbox disc integrity check",
            Width = 520,
            SizeToContent = SizeToContent.Height,
            CanResize = false,
            WindowStartupLocation = WindowStartupLocation.CenterOwner
        };
        var cancel = new Button { Content = "Cancel", IsCancel = true };
        var proceed = new Button { Content = "Continue with unverified ISO" };
        cancel.Click += (_, _) => dialog.Close(false);
        proceed.Click += (_, _) => dialog.Close(true);
        var actions = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
            Children = { cancel, proceed }
        };
        var content = Column(
            Copy($"The file {mismatch} differs from the verified GTA III US 1.0 disc."),
            Copy(OriginalXboxModels.UnverifiedImportNotice),
            actions);
        content.Margin = new Thickness(24);
        content.Spacing = 14;
        dialog.Content = content;
        dialog.Opened += (_, _) => cancel.Focus();
        return await dialog.ShowDialog<bool>(this);
    }

    private async Task Check()
    {
        if (work is not null) return;
        error = null;
        var selection = Selection;
        using var check = new CancellationTokenSource();
        work = check;
        Render();
        try
        {
            using var log = new PrivateLog();
            readiness = await Task.Run(() => new SetupInstaller(settings, log).Check(selection, check.Token), check.Token);
        }
        catch (OperationCanceledException) when (check.IsCancellationRequested) { readiness = null; }
        catch (Exception exception) { error = exception.Message; readiness = null; }
        finally { work = null; }
        if (closeAfterWork) Close(); else Render();
    }

    private async Task Install()
    {
        var selection = Selection;
        step = 3;
        result = null;
        progress = null;
        logPath = Path.Combine(logsDirectory, $"setup-{DateTime.UtcNow:yyyyMMdd-HHmmss}-{Guid.NewGuid():N}.log");
        using var install = new CancellationTokenSource();
        work = install;
        Render();
        var reporter = new Progress<InstallProgress>(value => { if (work == install) UpdateProgress(value); });
        try
        {
            using var log = new PrivateLog(logPath);
            result = await Task.Run(() => new SetupInstaller(settings, log).Install(selection, reporter, install.Token));
        }
        catch (Exception exception) { result = new(InstallStatus.Failed, InstallStage.CheckingSetup, false, exception.Message); }
        finally { work = null; }
        if (closeAfterWork) Close(); else Render();
    }

    private void Stop()
    {
        work?.Cancel();
        BackButton.IsEnabled = false;
        if (step == 3) progressLabel!.Text = "Stopping setup…";
        else Render(focusHeading: false);
    }

    private void UpdateProgress(InstallProgress update)
    {
        progress = update;
        if (work?.IsCancellationRequested == true || progressLabel is null || progressBar is null || progressCount is null) return;
        progressLabel.Text = StageTitle(update.Stage);
        progressBar.IsIndeterminate = update.Total == 0;
        progressBar.Maximum = Math.Max(1, update.Total);
        progressBar.Value = update.Completed;
        progressCount.IsVisible = update.Total > 0;
        progressCount.Text = update.CountingFiles
            ? $"{update.Completed} of {update.Total} files verified for this copy"
            : $"{update.Completed} of {update.Total} game file sets installed";
    }

    private Button Link(string name, string destination) => ButtonFor(name, () =>
    {
        try { Process.Start(new ProcessStartInfo(destination) { UseShellExecute = true }); }
        catch (Exception)
        {
            error = "Setup couldn’t open that location. You can open it manually: " + destination;
            Render();
        }
        return Task.CompletedTask;
    }, HorizontalAlignment.Left);

    private string TargetName() => target switch
    {
        GameTarget.Quest => "Quest 3 standalone",
        GameTarget.WindowsPcvr => "Windows PCVR",
        _ => "Linux PCVR"
    };

    private static string ResultTitle(InstallStatus status) => status switch
    {
        InstallStatus.Installed => "Liberty City VR is ready",
        InstallStatus.HeadsetPending => "Built locally. Headset installation pending",
        InstallStatus.Cancelled => "Installation cancelled",
        _ => "Setup couldn’t finish"
    };

    private static string StageTitle(InstallStage stage) => stage switch
    {
        InstallStage.PreparingGameFiles => "Preparing your game files",
        InstallStage.BuildingGame => "Building Liberty City VR",
        InstallStage.ConnectingHeadset => "Connecting to your Quest 3",
        InstallStage.InstallingApplication => "Installing Liberty City VR",
        InstallStage.InstallingGameFiles => "Installing your game files",
        _ => "Checking setup"
    };

    private static TextBlock Copy(string content, string? style = null)
    {
        var text = new TextBlock { Text = content };
        if (style is not null) text.Classes.Add(style);
        return text;
    }

    private static Border Card(Control child) => new() { Classes = { "card" }, Child = child };
    private static StackPanel Column(params Control[] children)
    {
        var panel = new StackPanel { Spacing = 7 };
        foreach (var child in children) panel.Children.Add(child);
        return panel;
    }

    private static Button ButtonFor(string name, Func<Task> action, HorizontalAlignment alignment = HorizontalAlignment.Stretch)
    {
        var button = new Button { Content = name, HorizontalAlignment = alignment };
        button.Click += async (_, _) => await action();
        return button;
    }
}
