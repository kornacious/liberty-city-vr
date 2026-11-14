# Liberty City VR platform scope

Liberty City VR 0.8.5 is a source-only GTA III VR project built on re3,
OpenXR and the bundled librw Vulkan renderer. Its official repository is
[kornacious/liberty-city-vr](https://github.com/kornacious/liberty-city-vr).

| Game target | Status | Current workflow |
| --- | --- | --- |
| Standalone Quest 3, Android ARM64 | Implemented; release candidate | Manager source assembly, APK build and local headset installation. |
| Windows PCVR, x64, OpenXR + Vulkan | Experimental game integration implemented | Manager target selection, source build/checks, isolated data preparation, versioned local install, launch and runtime diagnostics; direct CMake remains available. |
| Linux PCVR, x64, OpenXR + Vulkan | Shared SDL/FFmpeg implementation; native headset acceptance pending | Separate manager target, build/install/data/save settings and XDG runtime discovery. GCC/Clang and native desktop/headset results are recorded separately. |

Windows and Linux currently host the manager and authoring tools. The desktop
vehicle placement editor is a non-VR tool. Neither establishes PCVR support.
Existing `QUEST_*.md` guides document the implemented headset target.

The shared [desktop implementation](overlay/desktop/README.md) uses SDL3 for
windows, input, Vulkan surfaces and audio output, plus FFmpeg audio decoding.
Windows and Linux reuse the same game, renderer, XR integration and spatial
mixer. The previous Windows backend remains available as a migration fallback
until native Linux X11/Wayland and WiVRn acceptance is complete. Linux builds
under WSL do not establish native Linux VR support.

Development after the 0.8.5 scope preparation adds a standalone
[Windows bring-up executable](overlay/windows/README.md). It compiles the same
librw Vulkan sources and shaders as Quest, uses shared stereo transforms, and
implements runtime/device negotiation, stereo diagnostic rendering and basic
controller grip/select actions. A separate experimental game target reuses the re3
game loop, tracked VR interactions, menus and spatial audio mixer. Neither bundles game assets.

## Shared architecture and planned implementation

Keep both game targets in this repository, sharing re3 patches, VR gameplay,
menus, semantic input actions, librw Vulkan rendering, shaders and diagnostics.
As Windows support is implemented, extract reusable build/shader definitions
and common OpenXR frame, view and action handling from the Android integration.
Keep platform startup, logging, file paths, audio-device integration and optional
runtime extensions behind platform-specific boundaries.

The intended sequence is:

1. Extract shared build, renderer and OpenXR functionality while keeping Quest working.
2. Bring up a Windows OpenXR/Vulkan stereo scene and tracked controller input.
3. Integrate the game, audio, local data paths, menus and VR interactions.
4. Extend the existing manager with explicit target selection, Windows toolchain
   checks, desktop installation/launch and OpenXR runtime diagnostics.
5. Add matched replay benchmarks and a desktop Quest compatibility profile;
   consider PC-specific graphics features after feature parity is validated.

The shared Vulkan/game source builds, logging/timing boundary, stereo math,
OpenXR game lifecycle/input and Windows diagnostic/game entry points are implemented.
The original Windows services provided local data/save paths, WASAPI effects
output, Media Foundation music and local ACM ADPCM conversion; those audio
services now remain in the optional legacy backend. Menus, New Game, tracking and
music and clean engine/siren playback have been confirmed through VirtualDesktopXR.
Manager target selection, Windows toolchain checks, versioned local installation,
launch and runtime diagnostics are implemented. Windows builds never invoke APK
installation or ADB. A desktop eye preview provides ImGui/F1–F12 display options
and focused keyboard/mouse/XInput input alongside VR controls; the user confirmed
the preview options and desktop controls work through VirtualDesktopXR. Public desktop
binary packaging remains future work. Captured Quest workload profiles and matched
replay reporting are implemented and validated with three completed runs on each
target, matching source/content/settings and no dropped recorder rows. PCVR
gameplay at 120 FPS was confirmed after correcting a headset optimizer limit;
see [benchmark workflow and limits](BENCHMARKS.md). Broader runtime/controller
compatibility and any fallback for unavailable Vulkan features need validation;
OpenXR alone does not establish support for every headset/runtime combination.

Original Xbox collections can be installed into either target from the same
verified local conversion. The vehicle editor uses shared per-vehicle/modelset
templates with separate Quest/PCVR personal overrides, conflict-aware transfer
and live reload acknowledgements. See the [editor workflow](tools/vehicle-editor/README.md#quest-and-pcvr-targets).

## Configuration, validation and compatibility

Keep host OS, game target and graphics/test profile as separate choices. Use
separate target build outputs and settings/save locations, with explicit save
transfer rather than shared writable saves. Preserve existing Quest application
IDs, storage paths, signing identities and manager configuration locations.
Existing checkout folders may retain their old names after the repository rename.

The Quest workload profile matches captured settings, output extent, MSAA and
benchmark stereo geometry with explicit platform-format and capability limits.
It reports actual execution-device measurements. Calibrated estimates and broader
capability emulation remain future work. A slowed desktop GPU
does not emulate Quest GPU architecture, drivers, memory behavior or thermals;
synthetic frame pressure and estimated timings must be labeled accordingly.
Measure Quest improvements on Quest and PCVR improvements on the tested PC/runtime.

Run target-appropriate compilation and automated checks when implementation
changes. Continue Quest deployment rules in [AGENTS.md](AGENTS.md); future
Windows validation must be reported separately. All original game data stays
local, and personal device identifiers remain private on both targets.
