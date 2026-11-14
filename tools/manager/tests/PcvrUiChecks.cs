using LibertyManager;
using Terminal.Gui.App;
using Terminal.Gui.Input;
using Terminal.Gui.ViewBase;
using Terminal.Gui.Views;

internal static class PcvrUiChecks
{
    static IEnumerable<View> All(View parent)
    {
        foreach(var view in parent.SubViews) { yield return view; foreach(var child in All(view)) yield return child; }
    }
    public static int Run(string directory)
    {
        Directory.CreateDirectory(directory);
        var initial=new ManagerSettings();
        initial.Actions[ManagerAction.BuildAndInstall]=new ActionOptions { Target=GameTarget.WindowsPcvr };
        using var app=Application.Create().Init();
        using var ui=new ManagerUi(app,new SettingsStore(Path.Combine(directory,"settings")),initial,loadMusicOnOpen:false);
        ui.Initialize(); Exception? failure=null; var phase=0;
        CheckBox Prepare()=>All(ui.Window).OfType<CheckBox>().Single(c=>c.Text=="Install PC variant");
        Button Button(string name)=>All(ui.Window).OfType<Button>().Single(b=>b.Text==name);
        void Check(bool value,string message) { if(!value) throw new Exception(message); }
        app.AddTimeout(TimeSpan.FromMilliseconds(250),()=>
        {
            try
            {
                switch(phase++)
                {
                    case 0:
                        Check(!All(ui.Window).OfType<CheckBox>().Any(c=>c.Text.Contains("Convert and compress radio")),"Windows page contains Quest radio controls.");
                        Check(!All(ui.Window).OfType<Button>().Single(b=>b.Text=="Original Xbox assets").Visible,"Separate Xbox action remains visible.");
                        Check(All(ui.Window).OfType<TextField>().Take(2).All(f=>!f.Enabled),"Unselected variant paths are enabled.");
                        Prepare().Value=CheckState.Checked;
                        All(ui.Window).OfType<CheckBox>().Single(c=>c.Text=="Install Original Xbox variant").Value=CheckState.Checked;
                        All(ui.Window).OfType<OptionSelector>().Single().Value=0;
                        break;
                    case 1:
                        Check(All(ui.Window).OfType<CheckBox>().Any(c=>c.Text.Contains("Convert and compress radio")),"Quest controls did not return after switching target.");
                        Check(All(ui.Window).OfType<TextField>().Take(2).All(f=>f.Enabled),"Selected variant paths did not become editable.");
                        All(ui.Window).OfType<OptionSelector>().Single().Value=1;
                        break;
                    case 2:
                        Check(Prepare().Value==CheckState.Checked,"Target switching lost unsaved PCVR options.");
                        Check(All(ui.Window).OfType<CheckBox>().Single(c=>c.Text=="Install Original Xbox variant").Value==CheckState.Checked,"Both variant choices were not retained.");
                        Button("_Cancel").InvokeCommand(Command.Accept);
                        Check(Prepare().Value==CheckState.UnChecked,"Cancel did not discard PCVR option edits.");
                        File.WriteAllText(Path.Combine(directory,"pcvr-build.txt"),app.ToString());
                        Button("Launch PCVR").InvokeCommand(Command.Accept);
                        Check(All(ui.Window).OfType<TextField>().Count()==(OperatingSystem.IsLinux()?6:5),"Launch page lacks profile, dependencies or isolated PCVR directories.");
                        Button("_Cancel").InvokeCommand(Command.Accept);
                        Check(ui.Window.MostFocused?.Text=="Launch PCVR","Launch Cancel did not return navigation focus.");
                        Button("PCVR diagnostics").InvokeCommand(Command.Accept);
                        File.WriteAllText(Path.Combine(directory,"pcvr-diagnostics.txt"),app.ToString());
                        Button("Mods").InvokeCommand(Command.Accept);
                        Check(All(ui.Window).OfType<OptionSelector>().Count()==2,"Mods lacks target selector.");
                        All(ui.Window).OfType<OptionSelector>().Single(s=>s.Labels?.Count==3).Value=1;
                        break;
                    case 3:
                        Check(All(ui.Window).OfType<TextField>().Count()==5,"PCVR Mods lacks dependencies, isolated directories and package field.");
                        File.WriteAllText(Path.Combine(directory,"pcvr-mods.txt"),app.ToString());
                        All(ui.Window).OfType<OptionSelector>().Single(s=>s.Labels?.Count==3).Value=2;
                        break;
                    case 4:
                        Check(All(ui.Window).OfType<TextField>().Count()==6,"Linux Mods lacks its separate directories/dependencies/OpenXR fields.");
                        All(ui.Window).OfType<TextField>().First().Text=Path.GetFullPath(Path.Combine(directory,"linux-app-draft"));
                        All(ui.Window).OfType<OptionSelector>().Single(s=>s.Labels?.Count==3).Value=1;
                        break;
                    case 5:
                        Check(All(ui.Window).OfType<TextField>().First().Text.Length==0,"Linux directory edits replaced Windows directories.");
                        All(ui.Window).OfType<OptionSelector>().Single(s=>s.Labels?.Count==3).Value=2;
                        break;
                    case 6:
                        Check(All(ui.Window).OfType<TextField>().First().Text==Path.GetFullPath(Path.Combine(directory,"linux-app-draft")),"Linux target switching lost its directory draft.");
                        File.WriteAllText(Path.Combine(directory,"linux-pcvr-mods.txt"),app.ToString());
                        All(ui.Window).OfType<OptionSelector>().Single(s=>s.Labels?.Count==3).Value=0;
                        break;
                    case 7:
                        Check(All(ui.Window).OfType<TextField>().Count()==1,"Quest Mods retained desktop directory fields.");
                        app.RequestStop(); return false;
                }
            }
            catch(Exception error) { failure=error; app.RequestStop(); return false; }
            return true;
        });
        app.Run(ui.Window);
        Console.WriteLine(failure is null ? "PASS PCVR UI: target switching, draft preservation, Cancel, launch and diagnostics" : "FAIL PCVR UI: "+failure.Message);
        return failure is null ? 0 : 1;
    }
}
