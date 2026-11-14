# Shared desktop validation — 2026-11-14

Execution was local Windows x64 and local CachyOS Linux under WSL. Original
game data stayed on this computer. No original assets were downloaded, uploaded
or added to source/dependency bundles.

## Results

| Check | Result |
| --- | --- |
| Windows MSVC Release, diagnostic + Classic + Upstate | Passed, including fresh manager assembly |
| Windows native CTest | 17/17 passed: core/CLI, stereo/color/texture/upload/mirror, SDL audio output, missing audio backend and archive streaming |
| Windows manager regression suite | 134/134 passed |
| Windows terminal UI | Passed target switching, Linux/Windows draft separation, Cancel, launch and diagnostics |
| Linux GCC Release, diagnostic + Classic + Upstate | Passed |
| Linux GCC native CTest | 11/11 passed |
| Linux Clang Release, diagnostic + Classic + Upstate | Passed |
| Linux Clang native CTest | 11/11 passed |
| Linux manager regression suite | 135/135 passed from a local source-only snapshot on Linux's filesystem |
| Linux focused desktop manager checks | 30/30 passed, including contained library symlinks, missing SONAME rejection, target isolation and runtime discovery |
| Portable media matrix, Windows and Linux | MP3, AAC/M4A, Vorbis, Opus, FLAC, PCM WAV, Microsoft/IMA ADPCM and XOR ADF passed; malformed input rejected |
| Legacy Windows diagnostic fallback | Built; 5/5 asset-free checks passed |
| Windows/Linux local install bundles | Installed with shared libraries, OpenXR loader, dependency configuration, licenses and matching SDL/FFmpeg source archives |
| Windows/Linux manager launcher packages | Refreshed; Linux package executed under WSL |
| Quest ARM64 APK | Full build passed; APK updated on personal Quest 3 device, preserving data and saves |

The final Quest APK and installed Windows game carry matching shared-source
identities, including the extracted INI implementation. The complete Linux
manager rerun used a native-filesystem source snapshot to avoid slow fixture
operations on the Windows-mounted checkout; no game data was copied into it.

SDL was built with both X11 and Wayland backends enabled. WSL build/test
results do not establish native Linux display, GPU or headset compatibility.
Some earlier WSL runs were interrupted; the completed native/compiler and
focused manager results above are recorded separately from those partial runs.

## Windows headset acceptance

A bounded VirtualDesktopXR run completed 3,600 frames using the Original Xbox
variant and an isolated test save directory, then exited normally. The user
confirmed stereo/tracking, controller input and audio worked as expected. The
audio log reported zero media-buffer underruns. This was a functional check,
not a matched performance benchmark or comprehensive runtime certification.

The validated shared Windows backend was installed as a new version and made
active through the manager's checked installation-pointer publication. Previous
versions, existing data and saves were retained. The manager packages used by
the launchers were refreshed without changing global OpenXR runtime selection.

## Remaining acceptance

Native Linux X11/Wayland presentation, WiVRn headset gameplay, physical audio
hotplug behavior and broader headset/controller/runtime coverage remain pending.
The legacy Windows build option remains available until native acceptance is
complete. Shared Quest compilation/deployment does not substitute for a Quest
gameplay regression pass; standalone gameplay was not retested in this task.
