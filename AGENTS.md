# Project Scope & Target Guidelines

## Project Identity and Platform Scope
- Project: **Liberty City VR**, official repository `https://github.com/kornacious/liberty-city-vr.git`, current source-kit version **0.8.5**.
- Quest 3 / Android ARM64 is the release-candidate game target. Windows x64 PCVR has an experimental OpenXR/Vulkan game executable and a separate renderer diagnostic in `overlay/windows`. Interactive manager target selection and desktop packaging remain planned.
- Keep shared re3 patches, VR gameplay, librw Vulkan rendering and shaders in this repository. Extract common OpenXR/build functionality and isolate Android/Quest and Windows platform services as PCVR is implemented; avoid duplicating the game or renderer into a separate port.
- Planned manager support must distinguish game target from host OS and graphics/test profile, with separate build outputs, configuration and deployment workflows. Preserve existing application IDs, signing identities, game data and saves.
- A future desktop Quest compatibility profile may match capabilities and rendering settings; desktop GPU timings are not measured Quest performance. Validate performance claims on the actual target.
- Describe planned features as planned until implemented and validated. See `PLATFORMS.md` for target status and the roadmap.

## Cloud Tasks and Local-Only Game Data
- Original game data files must remain local to the user's computer. Never upload, commit, transfer, or fetch them into a cloud environment, including private repositories or cloud environment snapshots.
- At task start, identify whether execution is local or cloud and check whether the requested work requires actual game data. Recheck if a dependency on that data is discovered later.
- If a cloud task needs local-only game data to implement or validate the requested result, stop the task immediately. Report: **Unresolvable under current conditions: this task requires local-only game data. Request this task again with This computer selected to complete it.** Explain which required step needs the data and disclose any partial changes already made. Do not claim the task is complete or attempt to obtain the data remotely.
- Cloud tasks that do not require game data should implement the requested changes and run the appropriate automated tests and full compilation for the requested target using the available toolchain. Report missing dependencies or failed checks accurately; never treat skipped checks as passes.
- Cloud tasks must always skip ADB device discovery, APK installation, and device file transfers, even if a remote device could be reachable. Report device deployment as skipped because execution is in cloud.

## Local Builds and Quest 3 Deployment
- For local implementation/build tasks, run the appropriate automated tests and compilation. After a successful APK build, use `adb devices -l` to check for a connected device in the authorized `device` state, and query its model (for example, `adb -s <serial> shell getprop ro.product.model`) to verify that it is a Quest 3.
- If an authorized, responsive Quest 3 is present, deploy the APK produced by this task with `adb -s <serial> install -r <apk-path>` or the project's equivalent update workflow. Here, "push" means ADB deployment, not a Git push.
- If no eligible Quest 3 is present, or ADB is unavailable, skip deployment and report the reason; continue to report build and test results independently. Do not wait indefinitely for a device or deploy to an unrelated Android device.
- Always specify the verified device serial for deployment. If multiple eligible headsets are connected and no target is specified, ask which one to use before installing.
- Preserve existing game data and saves. Do not uninstall the application or erase its data to work around an installation failure; report the failure instead. Transfer game assets only when the requested task requires it and only through the local workflow.
- Instruction-only or documentation-only tasks do not require an APK build or deployment. Explicit task-specific instructions to skip installation take precedence over this default deployment rule.
- For future Windows PCVR implementation tasks, compile and run appropriate Windows checks separately; do not deploy a Quest APK unless the task also changes/builds that target. Keep host tool, desktop game and headset validation results distinct.

## Personal Quest 3 Device Privacy (Mandatory)
- Before any commit or push in a new clone, run `dotnet run --project tools/manager -c Release -- --privacy install-hooks` using the .NET 10 SDK. Keep the privacy hooks enabled; run `dotnet run --project tools/manager -c Release -- --privacy history` before publishing and resolve any findings locally. See `tools/PRIVACY.md` for setup and the local-only denylist.
- The user's personal Quest 3 serial number and any other unique personal device identifiers are private. Never publish, commit, upload, or disclose them to collaborators, other agents, external services, or anyone working on this project, including in a private repository.
- Use **"personal Quest 3 device"** in test results, build/deployment reports, chat responses, documentation, commit messages, pull requests, issues, screenshots, and shared logs. Never include an actual identifier or a partially masked identifier.
- Device identifiers may be used only locally when required to select the verified deployment target. Keep the actual value in a local variable and suppress or sanitize command echo, tool output, errors, and captured logs before they enter a chat, agent context, or shareable artifact. Do not paste identifiers into tool-call arguments or command text visible in the conversation. Do not expose raw `adb devices -l` output; capture it locally and report only sanitized device/model/status information.
- Before staging, committing, or pushing, inspect the staged content and proposed commit message for device identifiers and raw ADB output. Sanitize any findings before proceeding. Keep any local identifier lists or unredacted diagnostic files outside version control and external services.
- When auditing existing leaks, match identifiers locally and report only counts and sanitized locations. Never reproduce the identifier in findings, cleanup scripts committed to Git, replacement lists uploaded to a service, or cleanup commit messages.
- These privacy requirements apply to every contributor and automated agent. They take precedence over reporting examples that would otherwise expose a device serial or identifier; deployment must still select the verified target locally.
