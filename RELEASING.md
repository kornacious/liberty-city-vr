# Release checklist

The **0.9.5 Release Candidate** covers Quest 3 and Windows PCVR. Linux PCVR
remains experimental pending a native retest. See
[the changelog](CHANGELOG.md#095--release-candidate) for changes since 0.9.0 and
[platform scope](PLATFORMS.md) for gameplay validation.

Keep Android `versionName` and the CMake project versions at `0.9.5`. Android
`versionCode` and OpenXR application versions use `905`. The shared native version
and `RELEASE CANDIDATE` menu label are in `overlay/src/vr/OculusVR.h`.

The local `liberty-city-vr-build` directory is a private development/build tree containing a
complete re3 checkout. **Never publish or archive that directory.** The public
release root is this `liberty-city-vr` repository only.

Before a tag or GitHub release:

1. Confirm `git diff --cached --name-only` contains no unexpected staged files.
2. Confirm the repository contains no APK/AAB/SO/DLL, signing key,
   `release-signing.properties`, game data, save, profiler CSV, log, prepared
   game variant, `.codex*`, `.claude`, `.agents`, `build`, `dist` or cache.
   Keep compiled installers in separately staged Setup release archives.
   Exclude host-generated test `.inc` files and local iteration reports;
   preserve authored librw shader/font includes and source regression tests.
3. Assemble from a clean public `mrxenginner/re3` `master` checkout at commit
   `b050e1cc63b1869e6b514ab2f1a00ef4969e0c84` into a new output directory.
4. Assemble a second time with git's line-ending conversion switched off, which
   is how every builder outside Git for Windows runs:

   ```powershell
   $env:GIT_CONFIG_SYSTEM = "<path to an empty file>"
   $env:GIT_CONFIG_GLOBAL = "<path to an empty file>"
   ```

   Git for Windows ships `core.autocrlf=true` in its system config, and that
   rewrites patch line endings during `git apply`. A patch that only works
   because of it fails for everyone else. Each hunk must carry the line endings
   of the file it targets as re3 stores it: every re3 source is LF except
   `premake5.lua`, which is CRLF.
   Compare both freshly assembled trees against the intended release sources,
   including the complete bundled librw and all overlay files. Do not replace
   newer source-kit changes with older files from a private build directory.
5. Build `:app:assembleDebug` from that new assembled tree with the toolchain
   listed in `BUILDING.md`. Do not treat an incremental `liberty-city-vr-build` build as the
   reproducibility test.
6. Inspect the generated APK locally. It should contain only application
   metadata/resources and the arm64 `liblibertyvr.so`, `libc++_shared.so` and
   Khronos OpenXR loader. Do not add the APK to this source repository.
7. Review [NOTICE.md](NOTICE.md), [tools/README.md](tools/README.md) and the
   manager and Setup lock files whenever dependencies change. Retain the actual
   .NET runtime, NuGet and native component license and notice files in each
   self-contained distribution. For Setup, include Avalonia and the target’s
   ANGLE or Linux native dependencies, plus SkiaSharp and HarfBuzzSharp notices.
   Use the pinned package metadata and the corresponding upstream license files
   when a restored package provides only a license identifier.
8. Publish the commit/tag only after reviewing the complete GitHub file list.
   Prepare the Windows x64 and Linux x64 [Setup kits](tools/setup/README.md#build-and-package-setup)
   alongside the automatic source archive. Verify a clean extraction launches
   on each available desktop. Quest users build the APK locally. Keep APKs,
   assembled re3, retail data, and prepared asset variants out of release attachments.

For a prebuilt PCVR Setup kit, compile the full Release game from the same source
revision and run CTest. Stage it with `cmake --install` under the matching
`PCVR/<rid>/app` directory in the Setup kit. Build and test `tools/modelsets`
for that target and copy only its `xbox-convert` executable beside `app`.
Include its lzokay license, the pinned dependency source archives and notices
installed by CMake, and the Setup runtime notices. Verify the extracted
executables and library resolution, and include SHA-256 checksums for the
archives. Document the Linux build baseline and any limits on runtime testing.

For the Windows PCVR RC, build the desktop game and run its CTest checks on
Windows with the [desktop build workflow](overlay/desktop/README.md). Record the
OpenXR runtime and gameplay coverage separately for each tested target.

The manager assembles into a fresh output directory. Release signing fails unless
the builder supplies their own private keystore configuration. Build and test the
manager with `dotnet run --project tools/manager/tests -c Release`; verify the
self-contained Windows/Linux publishes outside version control. Run the native
privacy history check described in `tools/PRIVACY.md` before publishing. Embedded
auxiliary resources must contain source only, never game data or private values.
