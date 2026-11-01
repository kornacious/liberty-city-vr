using LibertyManager;
using Terminal.Gui.App;
using Terminal.Gui.Drivers;
using Terminal.Gui.Input;
using Terminal.Gui.ViewBase;
using Terminal.Gui.Views;

internal static class UiChecks
{
    private static IEnumerable<View> Descendants(View parent)
    {
        foreach (var child in parent.SubViews)
        { yield return child; foreach (var nested in Descendants(child)) yield return nested; }
    }
    public static int Run(string directory)
    {
        Directory.CreateDirectory(directory);
        var settings = new ManagerSettings();
        var store = new SettingsStore(Path.Combine(directory, "settings"));
        using var app = Application.Create().Init();
        using var ui = new ManagerUi(app, store, settings, loadMusicOnOpen: false);
        ui.Initialize();
        Exception? failure = null;
        Button Button(string text) => Descendants(ui.Window).OfType<Button>().Single(b => b.Text == text);
        app.AddTimeout(TimeSpan.FromMilliseconds(350), () =>
        {
            try
            {
                Button("Settings").InvokeCommand(Command.Accept);
                var fields = Descendants(ui.Window).OfType<TextField>().ToArray();
                fields[1].Text = Path.Combine(Path.GetTempPath(), "cancelled-workplace");
                Button("_Cancel").InvokeCommand(Command.Accept);
                if (Descendants(ui.Window).OfType<TextField>().ElementAt(1).Text != settings.Workplace || ui.Window.MostFocused?.Text != "Settings")
                    throw new Exception("Cancel did not restore settings and navigation focus.");
                Button("Settings").InvokeCommand(Command.Accept);
                Button("_Save").InvokeCommand(Command.Accept);
                if (store.Load().Workplace != settings.Workplace) throw new Exception("Settings did not persist.");
                File.WriteAllText(Path.Combine(directory, "settings.txt"), app.ToString());
                Button("Manage music").InvokeCommand(Command.Accept);
                var music = Descendants(ui.Window).OfType<MusicManagementView>().Single();
                music.Load(new(new string('a', 64), Enumerable.Range(1, 12).Select(i => new MusicEntry { Name = i + ".mp3", Artist = "Artist", Album = "Album", Title = "Track " + i, SampleRate = 48000 }).ToArray()));
                ui.Window.Layout();
                app.Mouse.RaiseMouseEvent(new Mouse { ScreenPosition = music.Tracks.ViewportToScreen(new System.Drawing.Point(1, 0)), Flags = MouseFlags.LeftButtonPressed });
                app.Mouse.RaiseMouseEvent(new Mouse { ScreenPosition = music.Tracks.ViewportToScreen(new System.Drawing.Point(1, 1)), Flags = MouseFlags.LeftButtonPressed | MouseFlags.PositionReport });
                app.Mouse.RaiseMouseEvent(new Mouse { ScreenPosition = music.Tracks.ViewportToScreen(new System.Drawing.Point(1, 1)), Flags = MouseFlags.LeftButtonReleased });
                if (music.Playlist.Tracks[1].Name != "1.mp3" || music.Playlist.Tracks[0].Name != "2.mp3") throw new Exception("Drag did not insert track at second position.");
#pragma warning disable CS0618
                if (!Descendants(music).OfType<TextView>().Single().Text.Contains("Track 1")) throw new Exception("Selected file details did not follow the drag.");
#pragma warning restore CS0618
                var options = new ActionOptions(); music.ReadOptions(options);
                if (options.MusicOrder[1] != "1.mp3") throw new Exception("Perform did not capture playlist order.");
                music.Load(new(new string('a', 64), Enumerable.Range(1, 12).Select(i => new MusicEntry { Name = i + ".mp3", Title = "Track " + i }).ToArray()));
                void Mouse(int row, MouseFlags flags) => app.Mouse.RaiseMouseEvent(new Mouse { ScreenPosition = music.Tracks.ViewportToScreen(new System.Drawing.Point(1, row)), Flags = flags });
                void Click(int row, bool shift = false)
                {
                    var modifier = shift ? MouseFlags.Shift : 0;
                    Mouse(row, MouseFlags.LeftButtonPressed | modifier); Mouse(row, MouseFlags.LeftButtonReleased | modifier);
                }
                Click(0); Click(2, true);
                if (!music.Playlist.Selection.SetEquals(new[] { "1.mp3", "2.mp3", "3.mp3" })) throw new Exception("Shift-click did not include the endpoints and intervening track.");
                Click(1);
                if (!music.Playlist.Selection.SetEquals(new[] { "2.mp3" })) throw new Exception("Plain click did not collapse the range selection.");
                // Exercise the terminal wire format, not pre-decoded modifier flags.
                void TerminalClick(int row, int modifier = 0)
                {
                    var point = music.Tracks.ViewportToScreen(new System.Drawing.Point(1, row));
                    var parser = new AnsiMouseParser();
                    app.Mouse.RaiseMouseEvent(parser.ProcessMouseInput($"\u001b[<{modifier};{point.X + 1};{point.Y + 1}M")!);
                    app.Mouse.RaiseMouseEvent(parser.ProcessMouseInput($"\u001b[<{modifier};{point.X + 1};{point.Y + 1}m")!);
                }
                TerminalClick(0); TerminalClick(2, 16);
                if (!music.Playlist.Selection.SetEquals(new[] { "1.mp3", "2.mp3", "3.mp3" })) throw new Exception("ANSI Ctrl-click did not select the range.");
                TerminalClick(1); TerminalClick(0, 16);
                if (!music.Playlist.Selection.SetEquals(new[] { "1.mp3", "2.mp3" })) throw new Exception("Reverse Ctrl-click did not select the range.");
                TerminalClick(1);
                Button("Range end").InvokeCommand(Command.Accept); TerminalClick(0);
                if (!music.Playlist.Selection.SetEquals(new[] { "1.mp3", "2.mp3" })) throw new Exception("Range end did not extend selection with an ordinary click.");
                TerminalClick(0);
                if (!music.Playlist.Selection.SetEquals(new[] { "1.mp3" })) throw new Exception("Range end was not one-shot.");
                Button("Range end").InvokeCommand(Command.Accept);
                app.Keyboard.RaiseKeyDownEvent(Key.Esc); TerminalClick(1);
                if (!music.Playlist.Selection.SetEquals(new[] { "2.mp3" })) throw new Exception("Escape did not cancel Range end.");
                TerminalClick(0, 16);
                Mouse(0, MouseFlags.LeftButtonPressed); Mouse(2, MouseFlags.LeftButtonPressed | MouseFlags.PositionReport); Mouse(2, MouseFlags.LeftButtonReleased);
                if (music.Playlist.Tracks[2].Name != "1.mp3" || music.Playlist.Tracks[3].Name != "2.mp3" || music.Playlist.Selection.Count != 2) throw new Exception("Group drag did not preserve the selected tracks and order.");
                Button("↓ Down").InvokeCommand(Command.Accept);
                if (music.Playlist.Tracks[3].Name != "1.mp3" || music.Playlist.Tracks[4].Name != "2.mp3") throw new Exception("Group keyboard movement failed.");
                Button("Delete").InvokeCommand(Command.Accept);
                music.ReadOptions(options);
                if (!options.MusicDeleted.ToHashSet().SetEquals(new[] { "1.mp3", "2.mp3" }) || options.MusicOrder.Contains("1.mp3") || music.Playlist.Tracks.Count != 10) throw new Exception("Delete did not stage exactly the selected files.");
                app.LayoutAndDraw(true);
                File.WriteAllText(Path.Combine(directory, "music.txt"), app.ToString());
                music.Playlist.Select(0, false); music.Playlist.Select(music.Playlist.Tracks.Count - 1, true); music.DeleteSelected();
                music.ReadOptions(options);
                if (options.MusicDeleted.Length != 12 || options.MusicOrder.Length != 0) throw new Exception("Delete-all did not retain an empty pending playlist.");
                music.Load(new(new string('b',64), []));
                Button("_Cancel").InvokeCommand(Command.Accept);
                var cancelled = Descendants(ui.Window).OfType<MusicManagementView>().Single().Playlist;
                if (cancelled.Dirty || cancelled.Deleted.Count != 0) throw new Exception("Cancel retained pending music changes.");
                Button("Tools and tests").InvokeCommand(Command.Accept);
                if (Descendants(ui.Window).OfType<Button>().Any(b => b.Text.Contains("save_transfer", StringComparison.Ordinal) || b.Text.Contains("transfer-vr-save", StringComparison.Ordinal) || b.Text.Contains("convert_vr_save", StringComparison.Ordinal)))
                    throw new Exception("Legacy save transfer is still visible in auxiliary tools.");
                Button("Transfer save").InvokeCommand(Command.Accept);
                var choices = Descendants(ui.Window).OfType<OptionSelector>().ToArray();
                if (choices.Length != 2 || choices[0].Labels?.Count != 2 || choices[1].Labels?.Count != 8)
                    throw new Exception("Save direction/slot options are missing.");
                choices[0].Value = 1; choices[1].Value = 7;
                Button("_Cancel").InvokeCommand(Command.Accept);
                choices = Descendants(ui.Window).OfType<OptionSelector>().ToArray();
                if (choices[0].Value != 0 || choices[1].Value != 0 || ui.Window.MostFocused?.Text != "Transfer save")
                    throw new Exception("Save transfer Cancel did not reset choices and navigation focus.");
                Button("Transfer save").InvokeCommand(Command.Accept);
                app.AddTimeout(TimeSpan.FromMilliseconds(400), () =>
                {
                    try
                    {
                        if (app.TopRunnable is not OpenDialog dialog || dialog.OpenMode != OpenMode.Directory) throw new Exception("Save folder Browse did not open an integrated directory dialog.");
                        if (!Directory.Exists(dialog.Path)) throw new Exception("Folder browser did not start at an existing parent of the proposed save directory.");
                        File.WriteAllText(Path.Combine(directory, "browser.txt"), app.ToString());
                    }
                    catch (Exception error) { failure = error; }
                    app.RequestStop(); return false;
                });
                Descendants(ui.Window).OfType<Button>().First(b => b.Text == "Browse").InvokeCommand(Command.Accept);
            }
            catch (Exception error) { failure = error; }
            app.RequestStop(); return false;
        });
        app.Run(ui.Window);
        Console.WriteLine(failure is null ? "PASS UI: ANSI Ctrl-click ranges, Range end/Escape, Shift-click, plain click reset, group drag/movement, deletion/Cancel, save selectors, settings and folder browser" : "FAIL UI: " + failure.Message);
        return failure is null ? 0 : 1;
    }
}
