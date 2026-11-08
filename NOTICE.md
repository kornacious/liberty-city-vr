# Third-party notices

- **MIT KEMAR HRTF measurements** — Copyright 1994 MIT Media Laboratory;
  measured by Bill Gardner and Keith Martin. The measurements are available
  without use restrictions with author attribution. The embedded, resampled
  diffuse-field responses and their reproducible source/permission details
  are documented in [SPATIAL_AUDIO.md](overlay/docs/SPATIAL_AUDIO.md).

- **librw** — MIT License, copyright aap. The `librw/` directory is a fork;
  its original license is preserved at `librw/LICENSE`. The Vulkan backend
  (`librw/src/vulkan/`) was written for this project.
- **OpenXR** — the loader is fetched at build time as the official Khronos
  `org.khronos.openxr:openxr_loader_for_android` package. The repository also
  includes Khronos OpenXR 1.1.58 headers. Both are Apache License 2.0; the
  bundled header license is preserved under `overlay/vendor/openxr-1.1.58/`.
  The Windows game and diagnostic use an installed OpenXR SDK or build the official
  Khronos OpenXR-SDK 1.1.58 loader from a SHA256-pinned source archive. Retain its
  upstream license/copyright files and JsonCpp notices with local binary builds;
  see [Windows build notes](overlay/windows/README.md).
- **Qualcomm SGSR2 research code** — adapted under the BSD 3-Clause License.
  It is disabled in the release player path; the original notice is preserved
  at `librw/src/vulkan/SGSR_LICENSE.txt`.
- **VR hand meshes** (`overlay/gamefiles/models/vrhands/`) — baked from the
  UltimateXR SDK hand models, copyright VRMADA, MIT License.
- **VR Port Foundations & Contributions**:
  - **GeT_RiCh** — Original author (based on his Discord account) of the [vice-city-vr-quest](https://github.com/dubrovskiy-yevhen-stakelogic/vice-city-vr-quest) VR port for the major implementation made for GTA Vice City using the reVC engine as a contribution for VR support.
  - **whachuvr** — For the full body IK implementation fork on top of GeT_RiCh's repository ([whachuvr/vice-city-vr-quest](https://github.com/whachuvr/vice-city-vr-quest)).
  - **re3 and reVC teams** — For their amazing effort and reverse-engineering work.
  - **Gemini 4 Argon and Gemini 3.8 Flash** — For porting GeT_RiCh and whachuvr's work from reVC / Vice City to re3 / GTA 3.
  - Their collective work offered a huge contribution for a solid starting point for the GTA III VR for standalone Quest porting efforts.
  - **OpenAI Codex** — Assisted with ongoing Liberty City VR development,
    including implementation, debugging, regression testing and documentation.
    Codex is planned to take over the ongoing coding-assistance role under the
    project maintainer's direction and review; see the
    [planned development areas](README.md#planned-development-with-codex).
- **This repository contains no complete re3 source tree and no assets from
  the original game or its publishers.** The patch files contain only the
  modifications and context required to transform a user-supplied re3 tree.

## Build, publishing and optional external tools

This inventory distinguishes bundled code from tools obtained on the builder's
computer. It does not relicense upstream code or original game data. The exact
versions/download pins live in `tools/manager/Toolchain.cs`, the Android Gradle
files and CMake configuration. The [tools README](tools/README.md) maps each
workflow to its dependencies; [manager notices](tools/manager/NOTICE.md) list
all locked NuGet dependencies, including transitive native components.

| Component | Use and attribution/license source |
| --- | --- |
| re3 | User-supplied GTA III engine at the pinned revision in BUILDING.md. Preserve that checkout's own notices; this kit includes patches, not the full engine. reVC is the Vice City ancestor, not the engine built here. |
| Git / Git for Windows MinGit | Source checkout, patching, update and release checks; [GPL-2.0](https://github.com/git/git/blob/master/COPYING). Portable distributions include additional component notices. |
| .NET SDK/runtime, MSBuild and NuGet | Manager compilation, restore and self-contained publishing; MIT-licensed projects with their own bundled third-party notices. See [dotnet](https://github.com/dotnet/runtime/blob/main/LICENSE.TXT), [MSBuild](https://github.com/dotnet/msbuild/blob/main/LICENSE) and [NuGet](https://github.com/NuGet/NuGet.Client/blob/dev/LICENSE.txt). |
| Eclipse Temurin / OpenJDK 21 | Java compiler/runtime and keytool; [GPL v2 with Classpath Exception](https://adoptium.net/docs/faq). Keep the downloaded JDK's legal directory. |
| Gradle 8.13 / Android Gradle Plugin 8.7.3 | APK assembly and packaging; Apache-2.0 projects. [Gradle license](https://github.com/gradle/gradle/blob/master/LICENSE), [Android build sources](https://android.googlesource.com/platform/tools/base/). Gradle distributions retain additional component notices, including Kotlin. |
| Android SDK command-line tools, Platform 35 and Build Tools 34.0.0 | sdkmanager, javac platform APIs, resource/dex processing, zipalign and apksigner. SDK downloads are governed by [Google's SDK terms](https://developer.android.com/studio/terms); do not describe the entire SDK distribution as open source. Components carry their own licenses/notices. |
| Android platform-tools / ADB | Local headset discovery, installation and verified file transfer; AOSP adb is Apache-2.0. Preserve the installed SDK package's NOTICE files and terms. |
| Android NDK 27.2.12479018 | Clang/LLVM, linker, libc++, native headers and optional compiler-rt ASan; LLVM components use Apache-2.0 with LLVM exceptions, with additional NDK component licenses. [NDK sources/notices](https://android.googlesource.com/platform/ndk/). Android system audio/media/Vulkan APIs are platform services, not bundled FFmpeg libraries. |
| CMake 3.22.1, CTest and CPack | Native configuration, tests and editor ZIP packaging; [BSD-3-Clause](https://github.com/Kitware/CMake/blob/master/Copyright.txt). |
| Ninja | Native build execution; [Apache-2.0](https://github.com/ninja-build/ninja/blob/master/COPYING). |
| shaderc / glslc, glslang, SPIRV-Tools and SPIRV-Headers | NDK GLSL-to-SPIR-V shader toolchain; [shaderc](https://github.com/google/shaderc/blob/main/LICENSE) and [SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools/blob/main/LICENSE) use Apache-2.0; glslang and SPIRV-Headers retain their own component notices. |
| FFmpeg / FFprobe | Optional external music/radio encoder, decoder and metadata probe; [LGPL-2.1-or-later, or GPL depending on build options](https://ffmpeg.org/legal.html). Check the installed build's license/configuration. Neither is linked into the APK or bundled in this kit. |
| lzokay | Jack Andersen, 2018, MIT; bundled LZO decompressor linked into the Xbox converter. [Retained license](tools/modelsets/vendor/lzokay/LICENSE). |
| GLFW | Editor window/input and GL context; pinned fallback 3.3.8, [zlib/libpng](https://www.glfw.org/license.html). [Retained license](tools/vehicle-editor/GLFW-LICENSE.md). |
| Dear ImGui and ImGuizmo | Dear ImGui provides editor UI and the Windows PCVR desktop preview controls; ImGuizmo provides editor transform gizmos. MIT; [ImGui license](librw/skeleton/imgui/LICENSE_imgui.txt), [ImGuizmo license](librw/skeleton/imgui/LICENSE_imguizmo.txt). Desktop installs include `IMGUI_LICENSE.txt`. |
| stb_rect_pack, stb_textedit and stb_truetype | Sean Barrett and contributors; embedded in librw's ImGui integration. The bundled versions retain their public-domain declarations in their headers. Dear ImGui's embedded ProggyClean font retains Tristan Grimmer's MIT notice in `imgui_draw.cpp`. |
| glad / Khronos GL platform declarations | Generated OpenGL loader in `librw/src/gl/glad/`; [glad generated-code terms](https://github.com/Dav1dde/glad) and Khronos permission notice in `khrplatform.h` are retained. |
| Python 3 | Retained auxiliary scripts and bridge, [PSF license](https://docs.python.org/3/license.html). |
| PowerShell 7 / Bash | Retained auxiliary runners/launchers; [MIT](https://github.com/PowerShell/PowerShell/blob/master/LICENSE.txt) / [GPL-3.0-or-later](https://www.gnu.org/software/bash/). Windows PowerShell/System.Drawing and MSVC/Visual Studio are separately supplied Microsoft tools, not open-source dependencies bundled by this kit. |
| NumPy / Pillow | Optional HRTF and hand regeneration; [BSD-3-Clause](https://github.com/numpy/numpy/blob/main/LICENSE.txt) / [HPND-style Pillow license](https://github.com/python-pillow/Pillow/blob/main/LICENSE). |
| FBX2glTF | Optional one-time hand authoring conversion; [BSD-3-Clause](https://github.com/facebookincubator/FBX2glTF). Its Autodesk FBX SDK dependency has separate terms and is not included here. |
| RenderDoc / RenderDoc Meta Fork | Optional external GPU capture/replay; bundled MIT application API header retains its notice. See [integration README](overlay/src/skel/android/renderdoc/README.md). No capture binary is shipped. |
| GCC / Clang, system OpenGL/X11/Wayland libraries | Builder-installed Linux editor/test prerequisites, subject to their installed package licenses; not part of the Quest APK. |
| Premake, Conan and PS2 toolchain helpers | Retained upstream librw workflows only; not used by the supported Quest CMake/Gradle pipeline. Their tools and dependencies are supplied separately under their upstream licenses. |

Original Xbox ISO extraction is implemented by this project's C# XDVDFS reader.
**extract-xiso and 7-Zip are not dependencies.** The retired downloaded-HD-pack
preparer and prebuilt texture compressor are no longer included. Source-only
publishing uses Git and hosting-provided source archives; GitHub CLI, Unity and
an external archive extractor are not required by the release checklist.

For local binary distributions, retain the notices from the actual downloaded
SDK/JDK/tool packages and locked manager/runtime components, as well as the editor
licenses installed by CMake. Source-kit credits alone do not replace those files.
