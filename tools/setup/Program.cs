using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;

namespace LibertySetup;

internal static class Program
{
    [STAThread]
    public static int Main(string[] args)
    {
        try
        {
            if (OperatingSystem.IsLinux() && args is ["--add-to-app-menu"])
            {
                SetupDesktopEntry.Register();
                Console.WriteLine("Liberty City VR Setup was added to your app menu.");
                return 0;
            }
            return AppBuilder.Configure<SetupApp>()
                .UsePlatformDetect()
                .With(new X11PlatformOptions { WmClass = SetupDesktopEntry.AppId })
                .StartWithClassicDesktopLifetime(args);
        }
        catch (Exception error) when (OperatingSystem.IsLinux() && args is ["--add-to-app-menu"])
        {
            Console.Error.WriteLine("Setup couldn’t add the app-menu shortcut. " + error.Message);
            return 1;
        }
        catch (DllNotFoundException error) when (OperatingSystem.IsLinux())
        {
            Console.Error.WriteLine("Setup needs the X11 desktop libraries. See tools/setup/README.md for Linux packages.");
            Console.Error.WriteLine(error.Message);
            return 1;
        }
    }
}

public sealed class SetupApp : Application
{
    public override void Initialize() => AvaloniaXamlLoader.Load(this);

    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
            desktop.MainWindow = new SetupWindow();
        base.OnFrameworkInitializationCompleted();
    }
}
