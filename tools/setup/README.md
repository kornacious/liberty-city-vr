# Liberty City VR Setup

Use Setup on a Windows x64 or Linux x64 computer. A prebuilt PCVR kit installs
the desktop game on this computer. Quest standalone builds its APK locally.
Source kits also support local PCVR builds. Bring
your own original GTA III PC installation folder, Original Xbox US
disc image, or both.

1. Extract the entire Setup kit to a writable folder. On Windows, open
   `Setup/LibertySetup.exe` or use `SETUP.bat`. On Linux, open
   `Setup/LibertySetup`. Linux needs the
   X11, ICE, SM, and fontconfig libraries and uses XWayland in a Wayland session.
   If the window doesn’t open, use the package manager to install the missing
   libraries. On Debian or Ubuntu, the packages are `libx11-6`, `libice6`,
   `libsm6`, and `libfontconfig1`. On Arch or CachyOS, install `libx11`,
   `libice`, `libsm`, and `fontconfig`.
2. Choose **Quest 3 standalone** or your computer’s **PCVR** target. Linux PCVR
   is experimental. For Quest, enable Developer Mode, connect the headset by USB
   and accept the USB debugging prompt inside the headset. Select **Install Upstate
   Liberty** to add the map expansion. Enabling it may reduce performance on Quest 3.
3. Select your GTA III files and follow the checklist. Prebuilt PCVR kits need
   a working Vulkan driver and OpenXR runtime. Windows may also need the x64
   Microsoft Visual C++ Redistributable. Setup uses Git to obtain the pinned
   re3 files it needs for data preparation and uses FFmpeg and ffprobe for Xbox
   disc imports. Setup downloads verified FFmpeg tools when working executables
   are unavailable. Windows Setup release kits include a converter that runs
   without Visual Studio, CMake or Ninja. If building PCVR from source, see the
   [desktop build prerequisites](../../overlay/desktop/README.md#dependencies).
   If you selected Upstate Liberty, choose the official v1.1.2 `UL.7z` or leave
   the archive field blank for the verified download.
   If a check fails, open **Setup help** and use **Check again** after resolving the item.
   If an Xbox image has all required files and reports the North American GTA III
   title and region but fails the retail US 1.0 fingerprint check, Setup offers
   **Continue with unverified ISO** or **Cancel**. Continue only if you trust the
   image. The import still requires successful conversion.
   For Quest, review Google's Android SDK license terms on the checklist and
   select the acceptance checkbox to enable **Install**.
4. Select **Install**. The first install can take a while. Use the desktop shortcut
   for PCVR, or open Liberty City VR under Unknown Sources in the Quest app library.
   If the headset installation is pending, connect or authorize the headset and retry.
   Use **Settings → Assets → Upstate Liberty** to enable or disable an installed
   mod. Fully quit and reopen the game to apply the change. Each engine keeps its
   own save profile.

Setup reads the existing manager’s configuration and applies help-page changes
for the current session. Reuse the same build folder and signing key on Quest
when reinstalling. Setup preserves your original files and uses the manager’s
existing install and save locations. The progress screen can open a local,
sanitized setup log if you need technical details.

On Linux, open **Setup help** and choose **Add Setup to app menu** to use the
Street Marker launcher. If you move the kit, use the button again to update the
launcher’s location.

Use the [terminal manager](../manager/README.md) for command-line parameters,
tool-specific settings, the vehicle editor, diagnostics, or developer tools.

## Build and package Setup

From the checkout with the .NET 10 SDK, run
`dotnet run --project tools/setup -c Release --artifacts-path out/setup-local-build`.
Use a native Windows or Linux x64 host to verify its release build.

```text
dotnet publish tools/setup -c Release -r win-x64 --self-contained true -p:RestoreLockedMode=true -o out/setup-win-x64
dotnet publish tools/setup -c Release -r linux-x64 --self-contained true -p:RestoreLockedMode=true -o out/setup-linux-x64
```

For each release archive, start with the tracked files of the corresponding
source tag and put that target’s complete publish output in a top-level
`Setup/` folder. Keep the launchers at the archive root. Make
`Setup/LibertySetup` and `Setup/LibertyManager` executable before creating the Linux tarball. The Windows
kit is a ZIP. Include the runtime license and third-party notices described in
the [release checklist](../../RELEASING.md). Each archive contains the installer
and source kit. A prebuilt PCVR kit also contains the matching desktop CMake
install under `PCVR/win-x64/app` or `PCVR/linux-x64/app`.

Run [Stage-SetupTools.ps1](Stage-SetupTools.ps1) on the native host for every
release kit. Pass the Release `xbox-convert` built from the same source revision,
the upstream standalone 7-Zip console executable (`7za.exe` from 7-Zip Extra
on Windows or `7zz` on Linux), and its license. The script tests the tools and
writes the SHA-256 manifests into `Setup/tools/<rid>`.

For a source-build kit, also pass the native SDL3/FFmpeg installation prefix
produced by `tools/desktop/build-deps`, the release tag, and a directory for
release assets. The script packages the prefix and writes the pinned download
entry into the kit. Attach the generated archive to that release alongside the
Setup kit. Preserve Linux library symlinks and executable modes in the tarball.
Assembled re3, original game files, prepared variants, and saves stay local.
