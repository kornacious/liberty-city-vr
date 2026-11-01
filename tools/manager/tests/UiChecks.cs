using LibertyManager;
using Terminal.Gui.App;
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
        using var ui = new ManagerUi(app, store, settings);
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
        Console.WriteLine(failure is null ? "PASS UI: save direction/slot selectors, Cancel, hidden legacy entries, saved settings and integrated folder browser" : "FAIL UI: " + failure.Message);
        return failure is null ? 0 : 1;
    }
}
