using System.Collections.ObjectModel;
using System.Drawing;
using Terminal.Gui.App;
using Terminal.Gui.Input;
using Terminal.Gui.ViewBase;
using Terminal.Gui.Views;

namespace LibertyManager;

public sealed class MusicManagementView : View
{
    public OrderedMusic Playlist { get; } = new();
    public ListView Tracks { get; } = new() { Width = Dim.Fill(), Height = Dim.Fill() };
#pragma warning disable CS0618
    private readonly TextView details = new() { ReadOnly = true, WordWrap = true, Width = Dim.Fill(), Height = Dim.Fill(), Text = "Connect your Quest 3, then Refresh.\n\nDrag tracks to reorder them. Perform saves the order and imports any selected files.\nAlt+Up / Alt+Down also move tracks." };
#pragma warning restore CS0618
    private readonly TextField source, board;
    private readonly CheckBox validate;
    private readonly IApplication app;
    private int dragged = -1;
    private bool dragMoved, extendClick, updating, rangeEnd;
    private readonly Label selectionInfo = new() { X = 32, Y = 2, Width = Dim.Fill() };
    public Action? RefreshRequested { get; set; }
    public MusicManagementView(IApplication app, ActionOptions options, ManagerSettings settings)
    {
        this.app = app; Width = Dim.Fill(); Height = Dim.Fill(); CanFocus = true;
        SetScheme(ManagerUi.Theme()); Tracks.SetScheme(ManagerUi.Theme()); details.SetScheme(ManagerUi.Theme());
        source = new TextField { Text = options.MusicSource, X = 8, Width = Dim.Fill(12) };
        board = new TextField { Text = options.BoardExport, Y = Pos.AnchorEnd(1), Width = Dim.Fill(12) };
        source.SetScheme(ManagerUi.Theme()); board.SetScheme(ManagerUi.Theme());
        Add(new Label { Text = "Import:", Y = 0 }, source, Browse(source, OpenMode.Mixed, 0, settings));
        Add(Button("Refresh", 0, () => RefreshRequested?.Invoke()), Button("↑ Up", 12, () => MoveSelected(-1)), Button("↓ Down", 21, () => MoveSelected(1)), Button("Delete", 32, DeleteSelected));
        validate = new CheckBox { Text = "Validate only", X = 0, Y = 2, Value = options.ValidateMusicOnly ? CheckState.Checked : CheckState.UnChecked };
        validate.SetScheme(ManagerUi.Theme()); Add(validate, selectionInfo);
        var range = Button("Range end", 17, () => { rangeEnd = !rangeEnd; UpdateSelectionInfo(); Tracks.SetFocus(); });
        range.Y = 2; Add(range);
        var list = new FrameView { Title = "PLAYLIST · Ctrl-click range", Y = 3, Width = Dim.Percent(55), Height = Dim.Fill(2) };
        var info = new FrameView { Title = "FILE INFO", X = Pos.Right(list), Y = 3, Width = Dim.Fill(), Height = Dim.Fill(2) };
        list.Add(Tracks); info.Add(details); Add(list, info);
        Add(new Label { Text = "Board export (optional):", Y = Pos.AnchorEnd(2) }, board, Browse(board, OpenMode.File, Pos.AnchorEnd(1), settings));
        Tracks.ValueChanged += (_, _) =>
        {
            if (!updating && dragged < 0 && Tracks.SelectedItem is { } index) { rangeEnd = false; Playlist.Select(index, false); RenderRows(index); }
            ShowDetails();
        };
        Tracks.RowRender += (_, e) => { if (e.Row >= 0 && e.Row < Playlist.Tracks.Count && Playlist.Selection.Contains(Playlist.Tracks[e.Row].Name)) e.RowAttribute = ManagerUi.Theme().Focus; };
        Tracks.MouseEvent += (_, e) => HandleDrag(e);
        Tracks.KeyDown += (_, e) =>
        {
            if (e == Key.CursorUp.WithAlt) { MoveSelected(-1); e.Handled = true; }
            if (e == Key.CursorDown.WithAlt) { MoveSelected(1); e.Handled = true; }
            if (e == Key.CursorUp.WithShift || e == Key.CursorDown.WithShift)
            {
                rangeEnd = false;
                var index = Math.Clamp((Tracks.SelectedItem ?? 0) + (e == Key.CursorUp.WithShift ? -1 : 1), 0, Math.Max(0, Playlist.Tracks.Count - 1));
                Playlist.Select(index, true); RenderRows(index); ShowDetails(); e.Handled = true;
            }
            if (e == Key.Esc && (dragged >= 0 || rangeEnd)) { dragged = -1; rangeEnd = false; app.Mouse.UngrabMouse(); UpdateSelectionInfo(); e.Handled = true; }
        };
    }
    private Button Button(string text, int x, Action click)
    {
        var button = new Button { Text = text, X = x, Y = 1, ShadowStyle = ShadowStyles.None };
        button.Accepting += (_, e) => { e.Handled = true; click(); }; return button;
    }
    private Button Browse(TextField field, OpenMode mode, Pos y, ManagerSettings settings)
    {
        var button = new Button { Text = "Browse", X = Pos.AnchorEnd(11), Y = y, ShadowStyle = ShadowStyles.None };
        button.Accepting += (_, e) =>
        {
            e.Handled = true;
            using var dialog = new OpenDialog { Title = mode == OpenMode.Mixed ? "Import audio file or folder" : "Export radio board placement", OpenMode = mode, MustExist = mode == OpenMode.Mixed };
            dialog.SetScheme(ManagerUi.Theme());
            dialog.Path = File.Exists(field.Text) || Directory.Exists(field.Text) ? field.Text : Directory.Exists(settings.Workplace) ? settings.Workplace : Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            app.Run(dialog); if (!dialog.Canceled) field.Text = dialog.Path;
        }; return button;
    }
    public void Load(MusicLibrarySnapshot snapshot)
    {
        var selected = Tracks.SelectedItem is { } index && index >= 0 && index < Playlist.Tracks.Count ? Playlist.Tracks[index].Name : "";
        rangeEnd = false; Playlist.Load(snapshot);
        var found = Playlist.Tracks.FindIndex(t => t.Name == selected);
        Playlist.Select(Math.Max(0, found), false); RenderRows(Math.Max(0, found));
        ShowDetails();
    }
    public void ReadOptions(ActionOptions options)
    {
        options.MusicSource = source.Text.Trim(); options.BoardExport = board.Text.Trim(); options.ValidateMusicOnly = validate.Value == CheckState.Checked;
        options.MusicRevision = Playlist.Dirty ? Playlist.Revision : "";
        options.MusicOrder = Playlist.Dirty ? Playlist.Tracks.Select(t => t.Name).ToArray() : [];
        options.MusicDeleted = Playlist.Dirty ? Playlist.Deleted.ToArray() : [];
    }
    private void RenderRows(int? focus = null)
    {
        updating = true;
        try
        {
            var previous = Tracks.SelectedItem ?? 0;
            Tracks.SetSource(new ObservableCollection<string>(Playlist.Tracks.Select((t, i) => $"{(Playlist.Selection.Contains(t.Name) ? '●' : ' ')}{i + 1,3}. {t.Display}")));
            if (Playlist.Tracks.Count > 0) Tracks.SelectedItem = Math.Clamp(focus ?? previous, 0, Playlist.Tracks.Count - 1);
            UpdateSelectionInfo();
        }
        finally { updating = false; }
    }
    private void UpdateSelectionInfo() => selectionInfo.Text = rangeEnd ? "Click last track" : $"{Playlist.Selection.Count} sel; {Playlist.Deleted.Count} delete";
    private void ShowDetails() => details.Text = (Playlist.Selection.Count > 1 ? $"{Playlist.Selection.Count} tracks selected\n\n" : "") + (Tracks.SelectedItem is { } index && index >= 0 && index < Playlist.Tracks.Count ? Playlist.Tracks[index].Details : "No music files found. Perform applies pending deletions; Cancel discards them.");
    public void DeleteSelected()
    {
        rangeEnd = false; UpdateSelectionInfo();
        if (Playlist.Selection.Count == 0) return;
        // A remembered import folder must not immediately re-import deleted tracks.
        source.Text = "";
        Playlist.DeleteSelection(); RenderRows(Math.Max(0, Playlist.SelectionStart)); ShowDetails();
    }
    public void MoveSelected(int delta)
    {
        rangeEnd = false; UpdateSelectionInfo();
        var to = Playlist.SelectionStart + delta;
        if (!Playlist.MoveSelection(to)) return;
        RenderRows(to); ShowDetails();
    }
    private void HandleDrag(Mouse mouse)
    {
        if (Playlist.Tracks.Count == 0 || mouse.Position is not { } position) return;
        var row = Math.Clamp(Tracks.Viewport.Y + position.Y, 0, Playlist.Tracks.Count - 1);
        if (mouse.Flags.HasFlag(MouseFlags.LeftButtonPressed) && !mouse.Flags.HasFlag(MouseFlags.PositionReport) && dragged < 0)
        {
            if (position.Y < 0 || position.Y >= Tracks.Viewport.Height || Tracks.Viewport.Y + position.Y >= Playlist.Tracks.Count) return;
            dragged = row; dragMoved = false;
            // Windows Terminal reserves Shift-click for terminal text selection.
            // Ctrl-click and the one-shot Range end button reach the app normally.
            extendClick = rangeEnd || mouse.Flags.HasFlag(MouseFlags.Shift) || mouse.Flags.HasFlag(MouseFlags.Ctrl);
            rangeEnd = false;
            if (extendClick || !Playlist.Selection.Contains(Playlist.Tracks[row].Name)) Playlist.Select(row, extendClick);
            RenderRows(row); Tracks.SetFocus(); app.Mouse.GrabMouse(Tracks); mouse.Handled = true;
        }
        else if (dragged >= 0 && mouse.Flags.HasFlag(MouseFlags.LeftButtonReleased))
        {
            var from = dragged; dragged = -1; app.Mouse.UngrabMouse();
            if (dragMoved || row != from)
            {
                if (position.X >= 0 && position.X < Tracks.Viewport.Width && !Playlist.Selection.Contains(Playlist.Tracks[row].Name))
                    Playlist.MoveSelection(Math.Min(row, Playlist.Tracks.Count - Playlist.Selection.Count));
            }
            else if (!extendClick) Playlist.Select(row, false);
            RenderRows(dragMoved ? Math.Max(0, Playlist.SelectionStart) : row);
            ShowDetails(); mouse.Handled = true;
        }
        else if (dragged >= 0 && mouse.Flags.HasFlag(MouseFlags.PositionReport))
        {
            if (position.Y < 0) row = Math.Max(0, Tracks.Viewport.Y - 1);
            if (position.Y >= Tracks.Viewport.Height) row = Math.Min(Playlist.Tracks.Count - 1, Tracks.Viewport.Bottom);
            if (row != dragged) dragMoved = true;
            updating = true; try { Tracks.SelectedItem = row; } finally { updating = false; }
            mouse.Handled = true;
        }
        else if (mouse.Flags.HasFlag(MouseFlags.LeftButtonClicked)) mouse.Handled = true;
    }
}
