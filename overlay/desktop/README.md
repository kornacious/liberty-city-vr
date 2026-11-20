# Shared desktop PCVR

Windows x64 and Linux x64 build the same SDL3 window/input/audio adapter,
FFmpeg media decoder, game integration, OpenXR loop and librw Vulkan renderer.
The diagnostic (`libertyvr-pcvr`) requires no game data. Classic/Xbox
(`libertyvr-game`) and Upstate (`libertyvr-upstate`) require locally prepared
owned game data. No original assets belong in source/dependency bundles.

## Dependencies

Use CMake 3.22.1+, Ninja, a C++17 compiler, Vulkan headers/loader and `glslc`.
Windows uses MSVC Desktop C++ tools; Linux uses GCC or Clang and pthread/dl.
The existing pinned OpenXR SDK source fallback remains available, including
`FETCHCONTENT_SOURCE_DIR_OPENXR_SDK` for offline builds.

`tools/desktop/dependencies.json` records SDL 3.4.16 and FFmpeg 9.0.2 source
URLs and SHA256 hashes. Build their shared libraries once into a separate
prefix. The scripts reuse downloaded archives and verify their hashes before
extracting them. For offline builds, place both named archives in the cache.

Linux also needs the development packages for SDL's X11, Wayland/EGL,
xkbcommon/libdecor and audio backends (PipeWire, PulseAudio or ALSA), plus
GNU Make, pkg-config and NASM. Install these with your distribution's package
manager, then from the repository root:

```sh
bash tools/desktop/build-deps.sh "$PWD/.cache/desktop-deps" "$PWD/.cache/desktop-deps/linux" linux
```

On Windows install MSYS2 with `make`, `nasm` and `diffutils`. The PowerShell
wrapper imports the Visual Studio x64 compiler environment automatically:

```powershell
./tools/desktop/build-deps.ps1 -Cache "$PWD/.cache/desktop-deps" -Prefix "$PWD/.cache/desktop-deps/windows" -Bash C:/msys64/usr/bin/bash.exe
```

The game links FFmpeg libraries directly; it does not spawn `ffmpeg` during
playback. The FFmpeg build disables network protocols, encoders and nonrequired
decoders. Supported media include MP3, AAC/M4A, Vorbis, Opus, FLAC, PCM WAV,
Microsoft/IMA ADPCM and the existing XOR-encoded ADF radio format. The spatial
SFX mixer still supplies 48 kHz stereo PCM; music is not HRTF processed.

## Build and install

The manager offers Quest, Windows PCVR and Linux PCVR as separate targets.
Select the matching desktop host, configure the dependency prefix if it is not
at the default above, then use Build and install. Linux has separate dependency,
OpenXR source, install, data and save settings. Existing Windows settings remain
valid. An omitted target in legacy launch/diagnostics requests selects the host
desktop target; build requests still default to Quest.

Direct diagnostic build on Linux:

```sh
cmake -S overlay/desktop -B out/linux-pcvr -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DLIBERTYVR_DESKTOP_PREFIX="$PWD/.cache/desktop-deps/linux"
cmake --build out/linux-pcvr --parallel 8
ctest --test-dir out/linux-pcvr --output-on-failure
cmake --install out/linux-pcvr --prefix /absolute/local/install
```

For Windows use the Windows prefix. `overlay/windows` forwards to this build;
its diagnostic executables remain in the top-level build output directory.

For the game, first assemble a clean pinned re3 checkout using the manager's
`--assemble RE3 OUTPUT` command, with OUTPUT outside both source repositories.
Configure the assembled `desktop` directory and set `LIBERTYVR_RE3_ROOT` to
the assembled root. Keep original game data outside the source tree and use
explicit `--game-data` and `--user-data` directories when launching, or provide
`pcvr-paths.ini` beside the game executable. Manager installations generate this
UTF-8 file with `version=1`, `data=PATH` and `user=PATH`, one key per line. Relative
paths resolve against the executable directory, allowing direct launch from any
working directory; each explicit CLI path overrides its corresponding default.
The base executable forwards to the sibling Upstate engine when enabled, retaining
package validation and separate saves. `--help` works without configuration.

Desktop saves use `<user-data>/userfiles/GTA3sf1.b` through `GTA3sf8.b`,
plus `GTA3sfautosave.b`. Upstate uses the `upstate-liberty-1.1.2` subfolder.
On first use, startup copies missing legacy slots from the selected content
directory without overwriting saves or removing originals. Builds made before this correction
wrote `1.b` through `8.b` and `autosave.b` into the content directory; after a
variant reinstall, those originals may be in `variants/.backup-*`. Recover them
into the matching base/Upstate save folder using the new names, retaining backups.

Local installations include the SDL/FFmpeg shared libraries and notices,
FFmpeg build configuration and matching source archives. Linux uses `$ORIGIN`
library lookup; the host's Vulkan driver is never bundled. No public binary
release or Linux distribution package is established by this change.

## Runtime and controls

For headset-independent world inspection, use the separate
[renderer debug viewer](RENDER_DEBUG.md). It provides an ImGui flycam, rendering
visualizations, synthetic stereo and automated camera-path benchmarks, sharing
the game renderer without creating a player or executing missions.

Use a working OpenXR runtime with Vulkan support. WiVRn is the initial Linux
headset validation target. The manager honors `XR_RUNTIME_JSON` and otherwise
checks the user/system XDG OpenXR configuration, or the Windows registry.
It does not select a runtime or change global runtime settings.

The preview retains F1–F12 controls, keyboard/mouse and gamepad input, ImGui
diagnostics and framing. SDL handles X11/Wayland surface extensions and pixel
sizes. Wayland compositors control window placement and may restrict focus or
position requests. Fullscreen is desktop fullscreen; no display-mode change is
requested. Minimized previews skip presentation. If the selected XR GPU cannot
present the preview, headset rendering continues with a logged warning.

F1 includes an audio-output selector. System default follows OS routing;
explicit device preferences fall back to default if disconnected and retry on
reconnection. SFX and music move together. No output device leaves the game
running silently while waiting for recovery. Preferences use device names,
never persisted SDL instance IDs. Saves and preview settings retain their
existing formats and Windows locations.

## Checks and acceptance

Default CTest includes the shared core/CLI checks and synthetic SDL dummy-audio
playback tests. `LIBERTYVR_GPU_TESTS=ON` additionally exercises offscreen stereo,
color, texture/upload and desktop mirror rendering. These need an appropriate
GPU/display. They do not establish headset compatibility.

Generate the extended codec fixtures with a developer FFmpeg executable:

```sh
python3 tools/desktop/generate-fixtures.py /tmp/lcvr-audio-fixtures
SDL_AUDIO_DRIVER=dummy out/linux-pcvr/desktop-audio-tests /tmp/lcvr-audio-fixtures
```

The fixtures are synthesized tones, including ADPCM/ADF and malformed input;
they contain no game data. Tests exercise seek, pause, loop, output replacement,
stream replacement and shutdown. Audio callback counters are diagnostics, not
measurements of headset latency.

Windows PCVR has release candidate status in 0.9.5. The maintainer completed the
main story, all payphone missions and the off-road missions with Original Xbox
assets and Upstate Liberty enabled. Linux PCVR remains experimental pending a
native mission-progression retest. A later Linux playtest confirmed the crowded-scene
audio fix. Earlier CachyOS/WiVRn testing covered build and launch, PC and
Original Xbox audio, first-mission completion and saving. See [platform status](../../PLATFORMS.md).

Broader native acceptance must cover Windows/VirtualDesktopXR and Linux/WiVRn,
both X11 and native Wayland, controller bindings, audio routing, session/focus
recovery, Classic/Xbox/Upstate and clean shutdown. WSL builds do not establish
native Linux display or VR acceptance. Keep the migration fallback until that
acceptance is complete:

```text
cmake -S overlay/windows -B out/windows-legacy -DLIBERTYVR_LEGACY_WINDOWS_BACKEND=ON
```

Debug builds retain symbols. Windows retains DbgHelp crash stacks; Linux logs
the last checkpoint and permits ordinary core-dump debugging with gdb. RenderDoc
is optional and only attaches to an already injected module on either host.
