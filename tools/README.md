# Build, release and authoring tools

Use `MANAGE.bat` or `./MANAGE.sh` from the source-kit root. The manager builds
Liberty City VR against the pinned GTA III **re3** source, not reVC. Original
Vice City VR/reVC credits describe the port's ancestry; they are not build inputs.
The current game build target is Quest; Windows PCVR and a dual-target manager
are planned in [the platform roadmap](../PLATFORMS.md). Desktop tool support does
not imply a Windows VR game target.
See [BUILDING](../BUILDING.md), [RELEASING](../RELEASING.md) and the
[third-party inventory](../NOTICE.md).

| Workflow | Tools and libraries actually used |
| --- | --- |
| Manager compilation and standalone publishing | .NET 10 SDK, MSBuild, NuGet, Terminal.Gui and its locked transitive packages; self-contained publishes include the .NET runtime. See [manager notices](manager/NOTICE.md). |
| Quest source assembly and APK | Git (portable MinGit on Windows if needed), re3, bundled librw, Temurin JDK 21, Gradle 8.13, Android Gradle Plugin 8.7.3, Android SDK/NDK, CMake, Ninja, Clang/LLVM, libc++, shaderc/glslc and SPIR-V tools; Khronos OpenXR loader and headers. |
| Signing and local installation | JDK keytool, Android apksigner/zipalign and ADB; builder supplies their own private signing material. |
| Music metadata and station conversion | User-installed FFprobe and FFmpeg; metadata validation and AAC-LC/M4A encoding/decoding run as external processes. Neither executable is bundled or downloaded by the manager. See [custom radio](../overlay/docs/CUSTOM_RADIO.md). |
| Original Xbox ISO import | Native C# XDVDFS reader, CMake/C++17, bundled librw and MIT lzokay for LZO decompression. No extract-xiso, 7-Zip, Python, downloaded HD pack or texture compressor is required. See [converter notice](modelsets/TOOL_NOTICE.md). |
| Desktop vehicle placement editor | CMake/CTest/CPack, C++17 (MSVC on Windows, GCC/Clang on Linux), librw GL3, GLFW (3.3.8 fallback), Dear ImGui, ImGuizmo, stb and generated glad; platform OpenGL/window libraries. Python 3 and ADB are needed only for the embedded device bridge. See [editor guide](vehicle-editor/README.md). |
| Host regression and analysis tools | .NET manager checks; C++17 fixtures; Python 3, PowerShell 7 or Bash for retained embedded modules. Some checks need a freshly assembled re3 tree, JDK javac/java or NDK shader tools. See [test guide](tests/README.md). |
| HRTF table regeneration | Python 3, NumPy and locally supplied MIT KEMAR measurements; generated table is already included. |
| VR hand asset regeneration | UltimateXR MIT assets, external FBX2glTF, Python 3, Pillow and NumPy. Already baked meshes need none of these at runtime. |
| Artwork and GPU diagnostics | Windows System.Drawing for the embedded artwork utility; optional external RenderDoc/Meta Fork for captures. Only its MIT application API header is bundled. |

The retained IMG/TXD inspection, merge and injection utilities operate locally on
user-supplied files. The librw Conan/Premake/PS2 helpers belong to its upstream
multi-platform library; they are not Quest build prerequisites.

## Removed inherited workflows

The manager's native build/update/repair/music/save/ISO actions replace the old
script installers and root shell shortcuts. The old Vice City save converter
used a different binary layout and provider package. The downloaded Xbox Version
HD pack builder, its manifest, compressor binary/source and tests were retired
in favor of the local retail ISO importer. Those modules are no longer embedded
or selectable. No original game data was removed by this cleanup.

The five previously excluded test runners for developer culling, helicopter
bullets, model-set wheels, ragdoll visibility and vehicle drive-by, and their C++
fixtures, were removed because their adapters targeted missing modules, reVC
weapons/bikes or private historical snapshots. Their removal does not establish
coverage for those behaviors. Retained current-source tests, including known
adapter failures, remain explicit in [the test guide](tests/README.md).

Legacy CMake/Gradle options such as `MIAMIVR_DEV_TOOLS` and old environment
aliases remain for existing build configurations. Internal C++/GLSL code and
test adapters use `LIBERTYVR_DEV_TOOLS`. `Modern` model-set names and on-device
directories also remain as storage compatibility identifiers. Historical credits, changelogs and test reports keep
their original context. Runtime application/engine names and new signing examples
use Liberty City VR/re3.
