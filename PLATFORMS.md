# Liberty City VR platform scope

Liberty City VR 0.8.5 is a source-only GTA III VR project built on re3,
OpenXR and the bundled librw Vulkan renderer. Its official repository is
[kornacious/liberty-city-vr](https://github.com/kornacious/liberty-city-vr).

| Game target | Status | Current workflow |
| --- | --- | --- |
| Standalone Quest 3, Android ARM64 | Implemented; release candidate | Manager source assembly, APK build and local headset installation. |
| Windows PCVR, x64, OpenXR + Vulkan | Planned | No desktop game build, package, launcher or manager target in 0.8.5. |

Windows and Linux currently host the manager and authoring tools. The desktop
vehicle placement editor is a non-VR tool. Neither establishes PCVR support.
Existing `QUEST_*.md` guides document the implemented headset target.

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

These are roadmap items, not features delivered in 0.8.5. Runtime/controller
compatibility and any fallback for unavailable Vulkan features need validation;
OpenXR alone does not establish support for every headset/runtime combination.

## Configuration, validation and compatibility

Keep host OS, game target and graphics/test profile as separate choices. Use
separate target build outputs and settings/save locations, with explicit save
transfer rather than shared writable saves. Preserve existing Quest application
IDs, storage paths, signing identities and manager configuration locations.
Existing checkout folders may retain their old names after the repository rename.

The planned Quest profile should match supported capabilities, shaders, stereo
rendering, resolution, formats, MSAA and relevant quality settings where feasible.
It may report workload budgets and calibrated estimates. A slowed desktop GPU
does not emulate Quest GPU architecture, drivers, memory behavior or thermals;
synthetic frame pressure and estimated timings must be labeled accordingly.
Measure Quest improvements on Quest and PCVR improvements on the tested PC/runtime.

Run target-appropriate compilation and automated checks when implementation
changes. Continue Quest deployment rules in [AGENTS.md](AGENTS.md); future
Windows validation must be reported separately. All original game data stays
local, and personal device identifiers remain private on both targets.
