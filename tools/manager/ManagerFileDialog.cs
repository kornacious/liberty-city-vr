using System.IO.Abstractions;
using System.Reflection;
using System.Runtime.InteropServices;
using Terminal.Gui.FileServices;
using Terminal.Gui.Input;
using Terminal.Gui.ViewBase;
using Terminal.Gui.Views;

namespace LibertyManager;

/// <summary>The shared, always-visible places tree for manager file and folder pickers.</summary>
public sealed class ManagerFileDialog : OpenDialog
{
    private readonly TreeView<IFileSystemInfo> tree;
    private readonly Dictionary<IDirectoryInfo, string> roots = [];
    private readonly Dictionary<IDirectoryInfo, string> userFolders = [];
    // Terminal.Gui 2.5 exposes Path (which can include the highlighted filename),
    // but keeps the browsed directory internal. Read only these two properties;
    // do not infer navigation from the editable path or mutate the dialog's state.
    private static readonly PropertyInfo DialogState = typeof(FileDialog).GetProperty("State", BindingFlags.Instance | BindingFlags.NonPublic)!;
    private static readonly PropertyInfo StateDirectory = DialogState.PropertyType.GetProperty("Directory")!;
    private readonly TextField pathField;
    private string? syncedDirectory;
    private bool syncingTree;
    private bool treeReady;

    public ManagerFileDialog()
    {
        var fileSystem = new FileSystem();
        IDirectoryInfo? user = null;
        if (OperatingSystem.IsWindows())
        {
            var home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            user = fileSystem.DirectoryInfo.New(home);
            roots.Add(user, "User");
            AddFolder("Desktop", Environment.SpecialFolder.DesktopDirectory);
            userFolders.Add(fileSystem.DirectoryInfo.New(Downloads(home)), "Downloads");
            AddFolder("Documents", Environment.SpecialFolder.MyDocuments);
            AddFolder("Music", Environment.SpecialFolder.MyMusic);
            AddFolder("Pictures", Environment.SpecialFolder.MyPictures);
            AddFolder("Video", Environment.SpecialFolder.MyVideos, "Videos");

            foreach (var drive in DriveInfo.GetDrives().OrderBy(d => d.Name, StringComparer.OrdinalIgnoreCase))
            {
                // Do not probe IsReady: an unavailable network mapping must not block opening a picker.
                try
                {
                    if (drive.DriveType is DriveType.Fixed or DriveType.Removable or DriveType.Network)
                        roots.Add(fileSystem.DirectoryInfo.New(drive.Name), drive.Name);
                }
                catch (IOException) { }
                catch (UnauthorizedAccessException) { }
            }

            void AddFolder(string label, Environment.SpecialFolder folder, string? fallback = null)
            {
                var path = Environment.GetFolderPath(folder, Environment.SpecialFolderOption.DoNotVerify);
                userFolders.TryAdd(fileSystem.DirectoryInfo.New(string.IsNullOrEmpty(path) ? System.IO.Path.Combine(home, fallback ?? label) : path), label);
            }
        }
        else roots.Add(fileSystem.DirectoryInfo.New("/"), "/");

        Style.TreeRootGetter = () => roots;
        tree = SubViews.OfType<TreeView<IFileSystemInfo>>().Single();
        var directories = new FileSystemTreeBuilder { IncludeFiles = false };
        tree.TreeBuilder = new DelegateTreeBuilder<IFileSystemInfo>(
            item => ReferenceEquals(item, user) ? userFolders.Keys : directories.GetChildren(item),
            item => ReferenceEquals(item, user) ? userFolders.Count > 0
                : item is IDirectoryInfo directory && roots.ContainsKey(directory) || directories.CanExpand(item));
        tree.AspectGetter = item =>
        {
            if (item is IDirectoryInfo directory)
            {
                if (roots.TryGetValue(directory, out var rootLabel)) return rootLabel;
                if (userFolders.TryGetValue(directory, out var folderLabel)) return folderLabel;
            }
            return item.Name;
        };

        // Terminal.Gui 2.5 puts the tree toggle first. Use its own layout operation,
        // then hide/disable the control so the pane cannot be closed accidentally.
        var toggle = Buttons[0];
        toggle.InvokeCommand(Command.Accept);
        toggle.Visible = toggle.Enabled = false;
        pathField = SubViews.OfType<TextField>().Single(v => v is not DropDownList);
        pathField.TextChanging += (_, e) => { if (syncingTree) e.Handled = true; };
        SubViewLayout += (_, _) => SyncTreeToDirectory();
        SetScheme(ManagerUi.Theme());
    }

    protected override bool OnDrawingContent(DrawContext? context)
    {
        SyncTreeToDirectory();
        return base.OnDrawingContent(context);
    }

    private void SyncTreeToDirectory()
    {
        // Navigation updates State after the path field, so synchronize at layout/draw
        // time, once the right-hand listing has settled (including Back/Up navigation).
        if (!treeReady || syncingTree) return;
        var state = DialogState.GetValue(this);
        var directory = state is null ? null : (StateDirectory.GetValue(state) as IDirectoryInfo)?.FullName;
        if (directory is null || SamePath(directory, syncedDirectory)) return;
        syncingTree = true;
        try
        {
            var user = roots.SingleOrDefault(r => r.Value == "User").Key;
            IFileSystemInfo? node = user is not null && SamePath(user.FullName, directory) ? user
                : userFolders.Keys.Concat(roots.Where(r => r.Value != "User").Select(r => r.Key))
                    .Where(r => ContainsPath(r.FullName, directory))
                    .OrderByDescending(r => r.FullName.Length).FirstOrDefault();
            if (node is null) return;
            if (node is IDirectoryInfo folder && userFolders.ContainsKey(folder) && user is not null) tree.Expand(user);

            var relative = System.IO.Path.GetRelativePath(node.FullName, directory);
            if (relative != ".")
            {
                foreach (var name in relative.Split([System.IO.Path.DirectorySeparatorChar, System.IO.Path.AltDirectorySeparatorChar], StringSplitOptions.RemoveEmptyEntries))
                {
                    tree.Expand(node);
                    var childPath = System.IO.Path.Combine(node.FullName, name);
                    var child = tree.GetChildren(node).FirstOrDefault(c => SamePath(c.FullName, childPath));
                    if (child is null) break; // An unreadable/link directory may have no expandable children.
                    node = child;
                }
            }
            if (!ReferenceEquals(tree.SelectedObject, node))
            {
                // Suppress the built-in handler's Path write during synchronization,
                // retaining file selections, partially typed names and navigation history.
                var insertionPoint = pathField.InsertionPoint;
                var selectedStart = pathField.SelectedStart;
                tree.SelectedObject = node;
                pathField.InsertionPoint = insertionPoint;
                pathField.SelectedStart = selectedStart;
            }
            tree.EnsureVisible(node);
            syncedDirectory = directory;
        }
        finally { syncingTree = false; }
    }

    private static bool SamePath(string left, string? right) => right is not null && string.Equals(
        System.IO.Path.TrimEndingDirectorySeparator(left), System.IO.Path.TrimEndingDirectorySeparator(right),
        OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal);

    private static bool ContainsPath(string parent, string child)
    {
        var relative = System.IO.Path.GetRelativePath(parent, child);
        return relative == "." || (!System.IO.Path.IsPathRooted(relative) && relative != ".."
            && !relative.StartsWith(".." + System.IO.Path.DirectorySeparatorChar, StringComparison.Ordinal));
    }

    public override void EndInit()
    {
        base.EndInit();
        // Show the curated places (or Linux root) immediately. Keep drive contents
        // lazy so disconnected network drives cannot stall startup.
        foreach (var root in roots.Keys)
            if (roots[root] == "User" || !OperatingSystem.IsWindows()) tree.Expand(root);
        treeReady = true;
    }

    private static string Downloads(string home)
    {
        // Downloads has no Environment.SpecialFolder value. Resolve the Windows known
        // folder so redirected folders (including OneDrive) retain their actual paths.
        var id = new Guid("374DE290-123F-4565-9164-39C4925E467B");
        var result = SHGetKnownFolderPath(in id, 0, IntPtr.Zero, out var path);
        try { return result >= 0 ? Marshal.PtrToStringUni(path) ?? System.IO.Path.Combine(home, "Downloads") : System.IO.Path.Combine(home, "Downloads"); }
        finally { Marshal.FreeCoTaskMem(path); }
    }

    [DllImport("shell32.dll", ExactSpelling = true)]
    private static extern int SHGetKnownFolderPath(in Guid rfid, uint flags, IntPtr token, out IntPtr path);
}
