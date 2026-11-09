# Windows OpenXR/Vulkan development builds

The developer executables use the same librw Vulkan backend and shaders as Quest:
`libertyvr-pcvr` renders a diagnostic scene without game data; `libertyvr-game`
is an experimental playable GTA III port using the shared re3/VR integration.
`libertyvr-upstate` builds that integration with the isolated expanded Upstate
engine tables and save layout. In the manager, select **Windows PCVR** on **Mods**
to install/enable Upstate Liberty v1.1.2; **Launch PCVR** verifies the package and
selects its executable. Quit PCVR before changing mods. Disabling returns to the
Classic/Xbox executable. Upstate saves remain in
`<PCVR user>/userfiles/upstate-liberty-1.1.2`; Windows ASIs are excluded. Expansion
gameplay/performance acceptance remains experimental.

To check an installed Upstate archive without a headset, run
`libertyvr-upstate --game-data DATA --user-data USER --check-upstate-archive`
with the prepared data and user directories. It validates the pinned package,
then opens the DAT archive through the production Windows streamer and checks
that it registered the mod overlay. This checks archive resolution, not rendering.
The [manager](../../tools/manager/README.md#windows-pcvr) now provides Windows target
selection, build/checks, versioned local installation, launch and runtime diagnostics.
Public binary packaging remains in the [roadmap](../../PLATFORMS.md). Captured
Quest workload profiles and matched replay reporting are available below.

## Desktop preview and input

Launch with `--quest-profile FILE` (or the manager's Quest profile field) to use
a captured Quest rendering workload. The ImGui panel identifies the active profile;
restart to switch back to native PCVR. See [matched benchmarks](../../BENCHMARKS.md)
for export, exact workload constraints and measurement limits.

The game opens a desktop eye preview alongside its console. It requires the active
OpenXR headset/session. Select the left or right application eye; runtime dashboards
and lens distortion are not mirrored. An ImGui panel controls the preview independently
of the VR view. These shortcuts work while the preview has focus:

| Key | Action |
| --- | --- |
| F1 | Show/hide controls; opening the panel releases captured mouse input |
| F2 | Switch left/right eye |
| F3 | Switch square/cropped framing |
| F4 | Windowed |
| F5 | Borderless window |
| F6 | Fullscreen on the current monitor, without changing its display mode |
| F7 | Restore a centered default window size |
| F8 | Capture/release the mouse for gameplay |
| F9 | Toggle desktop keyboard/mouse controls |
| F10 | Toggle PC gamepad controls |
| F11 | Toggle preview statistics |
| F12 | Reset preview options |

Square framing selects a centered square from the eye. Cropped framing selects a
centered region matching the window aspect ratio. Both downscale only; smaller
pictures have black borders. Theatre menus/cutscenes retain the entire picture and
its aspect ratio. Resizing does not change headset resolution. Preferences are saved
as `desktop-preview.cfg` in the isolated user directory.

With the preview focused, native re3 keyboard/mouse bindings and an XInput PC gamepad
can navigate menus and control movement/driving. VR head and tracked weapon poses
still come from the headset/controllers. The panel consumes its own clicks/keys;
focus loss clears held controls and releases the cursor. F1 closes the panel before
using game controls; F8 enables relative mouse capture. Use in-game Quit, close the
preview or press Alt+F4 to exit. The console remains available for logs.

The preview uses the existing Vulkan device and skips unavailable desktop frames.
`libertyvr-pcvr --mirror-probe` tests synthetic eye presentation, framing/window
shortcuts and focus recovery on an active Windows desktop without game assets or a
headset. GPU-enabled CTest includes this check; it writes a synthetic preview BMP
under its working directory's `.cache` for inspection.

## Build

Use Windows x64, an MSVC-compatible C++17 toolchain (Visual Studio Desktop C++ tools), CMake 3.22.1
or newer, Ninja and a Vulkan SDK providing headers, `vulkan-1.lib` and `glslc`.
Use a CMake version that supports your installed Visual Studio, or build with
Ninja inside its x64 developer terminal. From the source-kit root:

```text
cmake -S overlay/windows -B out/windows-pcvr -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build out/windows-pcvr --parallel
ctest --test-dir out/windows-pcvr --output-on-failure
out\windows-pcvr\libertyvr-pcvr.exe --help
```

For a Visual Studio generator, select x64 and add `--config Release` to the build
and `-C Release` to CTest. The executable is then under `Release` in the build
directory. `GLSLC` can explicitly select a shader compiler; normal SDK discovery
uses `VULKAN_SDK`/PATH. `Vulkan_INCLUDE_DIR` and `Vulkan_LIBRARY` support an
existing SDK in a custom location. Do not add an Android sysroot's entire include
directory to MSVC: its C runtime headers are incompatible with Windows.

CMake first tries an installed `OpenXR` CMake package. Otherwise it fetches the
official OpenXR-SDK `release-1.1.58` source archive, checked against SHA256
`43d75ce00e31be8f662b58e8c74af8c04156bcaa7acbfc19112aa2880f3a3669`, and builds
the loader DLL from pre-generated sources. No Python code generation is required.
For an offline checkout, set `FETCHCONTENT_SOURCE_DIR_OPENXR_SDK` to that SDK's
source directory. The source-built loader DLL is copied beside the executable;
for an installed SDK, ensure its loader DLL can be found at launch.

Keep the matching loader DLL and license notices with any local binary bundle.
The source fallback copies its `LICENSE`, `COPYING.adoc`, `.reuse/dep5`, license
texts and JsonCpp license into `openxr-notices` beside the executable. Installed
SDK users must retain their SDK's equivalent notices. The source kit does not
publish prebuilt DLLs, EXEs or game assets.

## Run and interpret results

### Experimental game

Use .NET 10 and a clean local checkout of the pinned public re3 commit
`b050e1cc63b1869e6b514ab2f1a00ef4969e0c84`. Assemble into a new directory;
the manager applies patches and overlays without modifying the checkout.
Prepare a separate local data directory from your owned PC GTA III installation:

```text
dotnet run --project tools/manager -c Release -- --assemble C:/LCVRBuild/re3-public C:/LCVRBuild/windows-src
dotnet run --project tools/manager -c Release -- --prepare-windows-data C:/Games/GTAIII C:/LCVRBuild/windows-data
cmake -S overlay/windows -B out/windows-pcvr -G Ninja -DCMAKE_BUILD_TYPE=Release -DLIBERTYVR_RE3_ROOT=C:/LCVRBuild/windows-src -DLIBERTYVR_GPU_TESTS=ON -DLIBERTYVR_AUDIO_TESTS=ON
cmake --build out/windows-pcvr --parallel
ctest --test-dir out/windows-pcvr --output-on-failure
out\windows-pcvr\libertyvr-game.exe --game-data C:/LCVRBuild/windows-data --user-data C:/LCVRBuild/windows-user
```

Adjust paths to your machine. Preparation rejects existing or overlapping output
directories and symbolic links, copies locally owned data plus authored overlays,
and writes `.libertyvr-pcvr-data` only after completion. Never upload these files.
The game requires that marker. Use a distinct user directory; saves, crash/performance
diagnostics and generated audio caches stay there, while preferences stay in the
isolated data copy. Do not share live writable saves with another game instance.
Reassemble sources after changing patches/overlays; rebuild after source changes.

The shared game integration includes head/controller tracking, VR menus, gameplay
and spatial/HRTF sound effects. Windows uses event-driven WASAPI for 48 kHz stereo
effects and Media Foundation for music. Retail IMA/MS ADPCM WAVs are decoded locally
through Windows ACM to `audio-cache` under the user directory; originals stay intact.
Logs report output buffer exhaustion and longest mixer callback to help investigate
crackle. Endpoint changes during playback currently require restarting the game.
RenderDoc integration uses an already injected DLL and never loads one automatically.

`--frames N` bounds visible game frames for smoke tests; omit it for normal play.
Use the in-game Quit option for a clean shutdown. Audio tests need an active Windows
audio endpoint, and the GPU test needs a working Vulkan device. Synthetic playback
tests verify the platform path; listen to engines, sirens, music and spatial effects
in the headset before claiming playback quality. Other runtimes/controllers, complete
mission progression and focus recovery still need Windows-specific validation.

### Renderer diagnostic

Activate a PC OpenXR runtime that supports Vulkan enable2 and connect a PCVR
headset to it. For example, a Quest used through Virtual Desktop must be connected
to that PC runtime; an ADB connection alone does not provide a PCVR session.
The application uses the already selected runtime and does not change registry
settings or runtime selection.

- `--vulkan-probe`: enumerate desktop Vulkan GPUs and multiview support. This
  does not test the runtime's GPU selection or a headset.
- `--renderer-probe`: allocate a two-layer offscreen target and submit five
  stereo frames through librw, including shader/pipeline setup and GPU completion.
  Read back both eye layers and require visible triangle pixels with the expected
  stereo disparity. This does not test OpenXR/compositor presentation.
- `--colour-probe`: render into a mutable sRGB image through an UNORM attachment
  and read back nine encoded gray levels in both eyes, also checking stereo geometry.
  This verifies byte preservation; headset testing still verifies runtime presentation.
- `--texture-probe`: upload 80 synthetic 256x256 RGBA textures with generated mips
  and report CPU upload time. Render the resulting alpha-weighted mips and read
  back both eyes to reject missing textures or transparent-color bleeding. Run
  timing comparisons with the game closed and comparable GPU load.
- `--upload-burst-probe`: issue 4,096 ordered texture updates in a single frame
  and verify the final alpha-weighted texture colors in both eyes. CTest applies
  a 60-second timeout. This covers load/save bursts that exceeded desktop driver
  submission storage and left world textures black after a rejected submission.
- `--probe`: let OpenXR select the GPU, create its Vulkan device and an XR session,
  then exit. Failure to obtain a headset system is an environment/runtime result,
  not a successful rendering test.
- `--frames 300` (default): submit the diagnostic scene for 300 visible frames.
  A center triangle appears two metres ahead of the LOCAL origin. Tracked grip
  markers turn yellow while Select is held. Simple, Touch and Index bindings are
  suggested; each runtime/controller combination needs validation.

The render loop checks a 60-second deadline between frames; Ctrl+C requests exit.
OpenXR wait calls remain controlled by the runtime. Exit code 0 means the selected
operation completed; 1 indicates an error; 2 means rendering ended before the
requested visible-frame count. Focus loss suppresses controller data, invalid
view poses submit an empty frame, and `shouldRender=false` does not acquire images.
The initial renderer uses 1x MSAA and waits for queue completion each frame;
these timings are not representative PCVR or Quest performance.

The game retains re3's display-encoded shading/blending. Windows game swapchains
are sRGB, with `XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT` and matching UNORM render
attachment views. This preserves legacy color bytes without another encoding on
write, while telling the compositor to decode sRGB on sampling. CPU-uploaded overlays
also use sRGB swapchains. Rendering directly into a normal sRGB attachment would
encode the legacy colors twice; submitting the same bytes as linear UNORM lifts
brightness and can introduce banding in the compositor. An advertised sRGB format
is required for the Windows game. This changes no color-filter or brightness setting.
See the [OpenXR color-space requirements](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrSwapchain.html).

CPU mip filtering uses temporary ordinary memory when Vulkan selects uncached
upload memory, then copies the completed chain to the mapped staging allocation.
This avoids slow CPU reads from write-combined memory on discrete GPUs without
changing the alpha-weighted filter or requiring a special memory type. Cached
staging allocations retain direct filtering. The temporary copy lasts only for
the raster lock/unlock operation; GPU staging still retires with the frame fence.
An initial matched RX 6700 XT test reduced CPU time for 80 synthetic uploads from
2,876 ms to 78 ms. This is an upload benchmark, not an overall loading speedup.
The follow-up headset test confirmed much faster loading and streaming across
the tested transitions. All seven Windows checks passed, including generated-mip
GPU sampling/readback; the final synthetic texture run took 82 ms. The shared
Quest APK built and updated successfully on the personal Quest 3 device. Quest
gameplay/performance was not retested in this follow-up.

Large upload bursts are split into queue submissions of at most 64 command buffers.
Their order is preserved, and the fence on the final submission covers preceding
uploads. This adds no per-batch GPU waits. Normal small frames still use one submit.

The color-space follow-up through VirtualDesktopXR completed 3,802 game frames
and exited cleanly. The user confirmed normal brightness and reduced banding.
All six Windows checks passed, including the new color readback test. The shared
Quest build also compiled and updated on the personal Quest 3 device; its visual
output was not retested in this follow-up. This does not establish that all remaining
texture or streamed-video banding has been eliminated.

Run `ctest` for CPU/CLI checks. Enable `LIBERTYVR_GPU_TESTS=ON` to include the
offscreen renderer smoke test; this requires a working Vulkan GPU and driver.
For headset QA, check both eyes, head motion, controller tracking/select, focus
loss/recovery, runtime stop and clean exit. No automated host check substitutes
for that visual test. Keep diagnostic logs local and omit personal device IDs.

## Initial validation — 2026-11-08

Windows x64 Release built with MSVC. All four CTest checks passed, including
GPU readback of 6,633 triangle pixels in each eye and the expected stereo disparity
on an AMD Radeon RX 6700 XT. The corrected headset run through VirtualDesktopXR
submitted 2,700/2,700 stereo frames at 2496 x 2688 per eye, 1x MSAA, and exited
successfully. The user confirmed visible stereo geometry and both controller
markers tracking and changing color with trigger input on the personal Quest 3
device. The subsequent game run confirmed menu navigation, New Game, tracked
looking/movement and audio; music was confirmed after adding the ADPCM cache.
The WASAPI follow-up confirmed clean engines/sirens with music still working;
16,071 game frames completed, with zero empty output buffers and a longest mixer
callback of 3.36 ms on this PC. All five Windows CTest checks and all 99 manager
checks passed. The shared-code Quest APK also built and updated successfully on
the personal Quest 3 device; standalone gameplay was not retested in this stage.
Focus-loss recovery and other controllers/runtimes remain separate validation
work. These results do not establish performance parity
with the standalone Quest build.
