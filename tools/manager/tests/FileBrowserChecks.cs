using System.Drawing;
using System.IO.Abstractions;
using LibertyManager;
using Terminal.Gui.Input;
using Terminal.Gui.Views;

internal static class FileBrowserChecks
{
    public static Task Run()
    {
        using var dialog = new ManagerFileDialog { OpenMode = OpenMode.Directory, Path = Path.GetTempPath() };
        dialog.BeginInit(); dialog.EndInit();
        var tree = dialog.SubViews.OfType<TreeView<IFileSystemInfo>>().Single();
        void Assert(bool condition, string message)
        { if (!condition) throw new Exception(message); }

        Assert(tree.Visible && tree.Enabled, "File tree is hidden on opening.");
        var toggle = dialog.Buttons[0];
        Assert(!toggle.Visible && !toggle.Enabled, "File tree can still be hidden with the toggle.");
        var roots = tree.Objects?.ToArray() ?? [];
        if (OperatingSystem.IsWindows())
        {
            Assert(tree.AspectGetter(roots[0]) == "User", "User is not the first root.");
            Assert(tree.IsExpanded(roots[0]), "User shortcuts are initially collapsed.");
            var folders = tree.GetChildren(roots[0]).ToArray();
            Assert(folders.Select(f => tree.AspectGetter(f)).SequenceEqual(new[] { "Desktop", "Downloads", "Documents", "Music", "Pictures", "Video" }), "User shortcuts have unexpected entries or ordering.");
            var drives = DriveInfo.GetDrives().Where(d => d.DriveType is DriveType.Fixed or DriveType.Removable or DriveType.Network)
                .OrderBy(d => d.Name, StringComparer.OrdinalIgnoreCase).Select(d => d.Name);
            Assert(roots.Skip(1).Select(r => r.FullName).SequenceEqual(drives), "Root drive letters do not match mounted fixed, removable and network drives.");
            var documents = folders[2];
            if (documents.Exists)
            {
                tree.SelectedObject = documents;
                Assert(Path.TrimEndingDirectorySeparator(dialog.Path) == Path.TrimEndingDirectorySeparator(documents.FullName), "Shortcut selection does not navigate to its actual folder.");
                dialog.Layout();
                Assert(ReferenceEquals(tree.SelectedObject, documents), "Path synchronization did not prefer the User shortcut.");
            }
        }
        else
        {
            Assert(roots.Length == 1 && roots[0].FullName == "/", "Linux has roots other than /.");
            Assert(tree.IsExpanded(roots[0]), "Linux root is initially collapsed.");
        }
        foreach (var size in new[] { new Size(120, 35), new Size(80, 24) })
        {
            dialog.Width = size.Width; dialog.Height = size.Height;
            dialog.Layout();
            Assert(tree.Frame.Width > 0 && tree.Frame.Height > 0, "Tree has no space in the dialog layout.");
            var table = dialog.SubViews.Single(v => v.Id == "_tableViewContainer");
            Assert(table.Frame.X >= tree.Frame.Right && table.Frame.Width > 0, "File list overlaps the tree or has no space.");
        }
        CheckNavigation();
        return Task.CompletedTask;
    }

    private static void CheckNavigation()
    {
        var fixture = Path.Combine(ManagerSettings.FindRepository(), ".cache", "file-browser-" + Guid.NewGuid().ToString("N"));
        var parent = Path.Combine(fixture, "parent");
        var child = Path.Combine(parent, "child");
        var deep = Path.Combine(child, "deep");
        Directory.CreateDirectory(deep);
        File.WriteAllText(Path.Combine(child, "example.txt"), "synthetic");
        for (var i = 0; i < 30; i++) Directory.CreateDirectory(Path.Combine(parent, $"a{i:00}"));
        using var dialog = new ManagerFileDialog { OpenMode = OpenMode.Mixed, Path = deep, Width = 80, Height = 24 };
        dialog.BeginInit(); dialog.EndInit();
        var tree = dialog.SubViews.OfType<TreeView<IFileSystemInfo>>().Single();
        var table = dialog.SubViews.Single(v => v.Id == "_tableViewContainer").SubViews.OfType<TableView>().Single();
        void Refresh() { dialog.SetNeedsLayout(); dialog.Layout(); }
        bool Same(string? left, string right) => left is not null && string.Equals(Path.TrimEndingDirectorySeparator(left), Path.TrimEndingDirectorySeparator(right), OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal);
        void Selected(string path)
        {
            Refresh();
            if (!Same(tree.SelectedObject?.FullName, path)) throw new Exception("Tree did not follow the browsed directory.");
            var row = tree.GetScrollOffsetOf(tree.SelectedObject!);
            if (row < tree.ScrollOffsetVertical || row >= tree.ScrollOffsetVertical + tree.Viewport.Height)
                throw new Exception("Current tree directory was not scrolled into view.");
        }
        Selected(deep); // Initial paths must expand the full ancestor chain.
        dialog.Path = parent; Selected(parent);
        table.SetFocus();
        var childRow = Enumerable.Range(0, table.Table!.Rows).Single(r => table.Table[r, 0]?.ToString()?.EndsWith("child", StringComparison.Ordinal) == true);
        table.SetSelection(0, childRow, false);
        Selected(parent); // Highlighting a child must not navigate the tree into it.
        table.InvokeCommand(Command.Accept);
        Selected(child);
        if (!table.HasFocus) throw new Exception("Tree synchronization stole the file list's focus.");
        dialog.Path = Path.Combine(child, "example.txt");
        Selected(child);
        if (dialog.Path != Path.Combine(child, "example.txt")) throw new Exception("File selection was overwritten.");
        dialog.Path = Path.Combine(deep, "new-file.txt");
        var pathField = dialog.SubViews.OfType<TextField>().Single(v => v is not DropDownList);
        pathField.InsertionPoint = 2; pathField.SelectedStart = 1;
        Selected(deep);
        if (dialog.Path != Path.Combine(deep, "new-file.txt")) throw new Exception("Tree synchronization overwrote the filename.");
        if (pathField.InsertionPoint != 2 || pathField.SelectedStart != 1) throw new Exception("Tree synchronization moved the path edit cursor or selection.");
        dialog.Path = parent; Selected(parent);
        // The right-hand parent entry exercises the same navigation used by Up/Back.
        table.SetSelection(0, 0, false);
        table.InvokeCommand(Command.Accept);
        Selected(fixture);
    }
}
