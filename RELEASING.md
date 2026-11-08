# Source-only release checklist

Current release: **0.8.5 — release candidate**, ready for public testing and
playing on Quest with finishable main missions. The 0.8.5 notes cover changes
since the 0.8.0 release-candidate commit `ad84866` and the project scope/URL update;
see [CHANGELOG.md](CHANGELOG.md). Windows PCVR is planned, not a releasable target.
See [PLATFORMS.md](PLATFORMS.md) for target status and validation boundaries.
Keep Android `versionName` at `0.8.5`, `versionCode` at `805`, and the shared native
version string in `overlay/src/vr/OculusVR.h` at `0.8.5`. The release candidate
status is documented separately from the numeric version used by build tools.

The local `liberty-city-vr-build` directory is a private development/build tree containing a
complete re3 checkout. **Never publish or archive that directory.** The public
release root is this `liberty-city-vr` repository only.

Before a tag or GitHub release:

1. Confirm `git diff --cached --name-only` contains no unexpected staged files.
2. Confirm the repository contains no APK/AAB/SO/DLL, signing key,
   `release-signing.properties`, game data, save, profiler CSV, log, Modern
   model overlay, `.codex*`, `.claude`, `.agents`, `build`, `dist` or cache.
   No prebuilt executable belongs in the source kit; the retired texture
   compressor is no longer an exception.
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
   manager's locked package inventory whenever dependencies change. Local
   self-contained manager/editor distributions must retain the actual runtime,
   NuGet and native component license/notice files; CMake installs editor notices.
8. Publish the commit/tag only after reviewing the complete GitHub file list.
   If desired, use GitHub's automatic source archive; do not attach an APK,
   assembled re3 tree, retail data or a generated Modern model set.

The manager assembles into a fresh output directory. Release signing fails unless
the builder supplies their own private keystore configuration. Build and test the
manager with `dotnet run --project tools/manager/tests -c Release`; verify the
self-contained Windows/Linux publishes outside version control. Run the native
privacy history check described in `tools/PRIVACY.md` before publishing. Embedded
auxiliary resources must contain source only, never game data or private values.
