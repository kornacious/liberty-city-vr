# Project Scope & Target Guidelines

## Mandatory Scope Rule
- **Target Project:** Any user request, task, feature implementation, bug fix, or code modification is ALWAYS and EXCLUSIVELY for **`liberty-city-vr-quest`** (`D:\Projects\GTAVR\liberty-city-vr-quest`).
- **Never Target `vice-city-vr-quest`:** Never treat `vice-city-vr-quest` as the project to be fixed or developed.

## Strict Restrictions on `vice-city-vr-quest`
- **Read-Only Porting Reference:** `vice-city-vr-quest` (`D:\Projects\GTAVR\vice-city-vr-quest`) is strictly an external reference repository used ONLY for inspecting implementations, reverse engineering, and porting algorithms/features over to `liberty-city-vr-quest`.
- **DO NOT MODIFY:** Under no circumstances should any files in `vice-city-vr-quest` be edited, created, moved, or deleted.
- **DO NOT COMMIT:** Never stage (`git add`) or commit (`git commit`) changes in `vice-city-vr-quest`.
- Any existing untracked or modified files in `vice-city-vr-quest` must be left untouched.

## Execution Rules for All Tasks
1. **Always Verify Target Directory:** Before modifying or creating any code, verify that the active file or directory is inside `liberty-city-vr-quest` (`D:\Projects\GTAVR\liberty-city-vr-quest`).
2. **Porting Workflow:** When porting a feature from `vice-city-vr-quest` to `liberty-city-vr-quest`:
   - Read / inspect the code in `vice-city-vr-quest`.
   - Apply, adapt, and test the ported code ONLY inside `liberty-city-vr-quest`.
   - Verify build and commit ONLY in `liberty-city-vr-quest`.

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
