# Liberty City VR — Agent Guidelines

## Scope and architecture
- Official repository: `https://github.com/kornacious/liberty-city-vr.git`. See `PLATFORMS.md` for current target status and roadmap; describe unvalidated or planned features accordingly.
- Keep shared re3 patches, VR gameplay, OpenXR integration, librw Vulkan rendering and shaders in this repository. Share common functionality and isolate platform services rather than duplicating the game or renderer.
- Distinguish host OS, game target and graphics/test profile. Keep target outputs, configuration, installation and save locations separate. Preserve application IDs, signing identities, game data and saves.
- Desktop Quest workload profiles do not emulate Quest GPU architecture, and desktop timings are not measured Quest performance. Validate performance claims on the actual target; see `BENCHMARKS.md`.

## Default target and validation
- At task start, identify the execution OS, whether execution is local or cloud, and whether actual game data is required. Recheck if dependencies change.
- Unless the user specifies otherwise, focus on native PCVR for the execution OS: Windows PCVR on Windows, Linux PCVR on Linux. For other hosts, report unsupported or missing tooling rather than assuming a supported target. WSL checks do not establish native Linux VR validation.
- Scope implementation and validation to the requested change. Shared changes use host-native PCVR by default; preserve compatibility with other supported targets without automatically building or deploying every target.
- Build and validate Quest 3 or the other desktop OS when explicitly requested by the user, or when the task explicitly fixes behavior specific to that target. Otherwise, explain why additional target validation is recommended and ask before expanding the build/deployment scope.
- For implementation tasks, run relevant automated checks and full compilation for the selected target using the available toolchain. Cross-compilation is not runtime validation. Report missing dependencies, failures and skipped checks accurately; keep host-tool, desktop-game and headset results distinct.
- Documentation-only and instruction-only tasks do not require game builds or deployment. Task-specific instructions to skip a build or installation take precedence.

## Local installation and Quest 3 deployment
- Do not discover or deploy to Quest devices during routine PCVR work, even after shared changes or an incidental APK build.
- Local Quest-specific fixes and explicit user requests authorize building and deploying the resulting APK. A recommendation alone does not authorize deployment; obtain the user's agreement first.
- When deployment is in scope and the APK builds successfully, capture `adb devices -l` locally, select only an authorized, responsive device in the `device` state, and query its model to verify it is a Quest 3. Apply the privacy rules below before any output reaches chat or shared logs.
- Install the APK produced by this task with `adb -s <locally-held-serial> install -r <apk-path>` or the equivalent project update workflow. Always select the verified device explicitly. If multiple eligible headsets are present and none was specified, ask which to use without exposing identifiers.
- If ADB or an eligible Quest 3 is unavailable, skip deployment and report why; do not wait indefinitely or install on another Android device. Here, “push to device” means ADB deployment, not a Git push.
- Preserve game data and saves. Never uninstall or erase application data to work around installation failures. Transfer game assets only when the task requires it and only through the local workflow.
- Apply the same scope rule to the other desktop OS: do not automatically build, install or deploy for it unless explicitly requested or required by a task fixing that OS specifically. Report unavailable toolchains or runtime access without claiming validation.

## Local-only game data and cloud execution
- Original game data must remain on the user's computer. Never upload, commit, transfer or fetch it into cloud environments, private repositories or cloud snapshots.
- If a cloud task requires actual game data for implementation or required validation, stop immediately and report: **Unresolvable under current conditions: this task requires local-only game data. Request this task again with This computer selected to complete it.** Explain the blocked step and any partial changes. Do not obtain the data remotely or claim completion.
- Cloud tasks without game-data dependencies should implement the change and run the selected target's available checks and compilation. Missing tools and skipped checks are not passes.
- Always skip ADB discovery, APK installation and device file transfers in cloud execution, even if a remote device is reachable. Report deployment as skipped because execution is in cloud.

## Device privacy — mandatory
- Before any commit or push in a new clone, use the .NET 10 SDK to run `dotnet run --project tools/manager -c Release -- --privacy install-hooks`. Keep hooks enabled. Before publishing, run `dotnet run --project tools/manager -c Release -- --privacy history` and resolve findings locally. See `tools/PRIVACY.md`.
- Personal device serials and other unique personal device identifiers are private, including in private repositories. Never disclose them to collaborators, other agents, external services or shared artifacts.
- Refer to the headset as **“personal Quest 3 device”** in reports, chat, documentation, commits, pull requests, issues, screenshots and shared logs. Never include actual or partially masked identifiers.
- Use identifiers only locally when needed to select a verified target. Keep them in local variables; sanitize command echo, output, errors and captured logs before they enter chat, agent context or shareable artifacts. Never paste identifiers into visible tool arguments or command text, or expose raw ADB discovery output.
- Before staging, committing or pushing, inspect proposed content and commit messages for identifiers and raw ADB output. Keep identifier lists and unredacted diagnostics outside version control and external services.
- Audit leaks locally and report only counts and sanitized locations. Never reproduce identifiers in findings, committed cleanup scripts or uploaded replacement lists. These rules apply to every contributor and agent and override examples that would expose an identifier.
