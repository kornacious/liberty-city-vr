# Shared desktop validation — 2026-11-14

For subsequent native Linux playtests and Windows RC progression results, see
[platform status](../../PLATFORMS.md).

## Renderer debug viewer follow-up — local Windows

The separate renderer viewer was built with MSVC alongside the normal Windows
PCVR client, Upstate client and diagnostic. A fresh manager source assembly
applied both patches and matched the tested build's source identity.

- Desktop CTest: **15/15 passed** with the default GPU-test option disabled.
- Focused manager desktop checks: **36/36 passed**, including viewer installation
  without an OpenXR loader or gameplay executable.
- Local Classic, Original Xbox and Upstate viewer batches completed, including
  mono/stereo, 2x MSAA, side-by-side output and repeated world resets.
- A cross-island camera path completed two passes with 360 measured frames.
- Rainy ambient stereo completed two 20-second simulation passes: 2,400 measured
  frames and 300 source-associated GPU samples. Player absence was asserted.
- An existing Quest profile completed at its captured 1512x1584 stereo extent.
  These are desktop workload checks, not measurements of Quest performance.
- ImGui sections, live shading changes, preference saving, independent F1/F2
  windows and fullscreen Quit were inspected locally. Hold-to-fly capture,
  release, focus loss and UI gesture ownership have automated regression checks.
- The Xbox World LOD On comparison verified the viewer's corrected production
  distance floor of 2.2x: detailed-model selections rose from 117 to 583 at the
  same camera position. The user subsequently confirmed the viewer fix and
  comparable Quest/PCVR LOD transitions; no separate PCVR fault was established.
- The packaged viewer rendered two passes with its OpenXR loader temporarily
  absent and `XR_RUNTIME_JSON` pointing to a nonexistent manifest.
- The normal client's asset/platform startup check passed with isolated user
  data. Headset gameplay was not rerun for this viewer change.

The package uses existing prepared local content and a separate viewer user
root; it does not bundle original game assets. Native Linux viewer compilation,
X11/Wayland operation and gamepad hardware acceptance remain unvalidated in this
follow-up. No Quest build, device discovery or deployment was performed.

The earlier shared-desktop validation record follows.

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
