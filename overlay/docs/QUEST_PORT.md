# Quest runtime and data layout

The Quest variant of Liberty City VR is a native arm64 Android/OpenXR application.
The Meta runtime creates the Vulkan instance/device and the renderer submits
both eyes in one multiview pass to a two-layer OpenXR swapchain.

## Runtime layout

- Package: `com.libertyvr.quest`
- Game data: `/sdcard/Android/data/com.libertyvr.quest/files/gamedata/`
- Saves/settings: `/sdcard/Android/data/com.libertyvr.quest/files/userfiles/`
- VR settings: `gamedata/vr_settings.ini`

## Install game data

Select **Quest standalone** in the manager's **Build and install** page. Enable
**Install PC variant** with a local PC installation, **Install Original Xbox
variant** with a supported US 1.0 ISO, or both. The manager bootstraps app-owned
storage, prepares the port resources (including the VR hands), verifies the
transfers and activates each complete base under `gamedata/variants/pc` or
`gamedata/variants/xbox`. Leave both unchecked for an application-only update.

With both bases installed, choose **Appearance > Asset Source** and fully
restart the game. Existing PC data directly under `gamedata` can still serve as
a legacy PC base. Reimport the ISO to migrate an old Xbox visual/audio overlay.
See the [asset guide](QUEST_MODELSETS.md) for selection and integrity checks.

## Rendering and performance defaults

Fresh installations request:

- 72 Hz
- 100% render scale relative to the OpenXR-recommended eye size
- sustained CPU and GPU performance hints
- Spatial AA enabled
- AUTHORED culling enabled; select STEREO SAFE or OFF if geometry disappears
- adaptive Physics Director V2, QUALITY preset, with an ORIGINAL/OFF fallback
- stock vehicle visual LOD behavior
- generated mipmaps enabled and stereo MSAA OFF (1x)
- ragdolls and vehicle deformation OFF

Saved preferences normally take precedence. The one-time Quest baseline
migration initializes older settings, and its version 2 migration resets render
scale to 100%. Later player changes remain editable and are retained.

The Graphics menu shows both requested and actually active render scale. If a
high-resolution allocation fails, the next launch recovers to 100% and records
the fallback instead of continuing to claim the unavailable scale.

Stereo MSAA is a player setting with OFF/2X/4X choices; restart after changing
it. Temporal AA/SGSR and runtime-only foveation remain disabled research paths.

## Logs and updates

Enable **Graphics > Active Performance Profiling** to record frame timings,
asset selections, player events and vehicle speeds. The FPS display setting
controls the on-screen readout independently. See [performance capture and comparison](PERFORMANCE_CAPTURE.md).

Useful runtime log:

```sh
adb logcat -s LibertyVR:V librw-vk:V
```

Update APKs with `adb install -r` to preserve application data. Settings and
saves are not part of the APK and should be backed up separately before major
experiments.
