# Liberty City Vehicle Placement Editor

Standalone, non-VR Windows/Linux editor for GTA III virtual wheels and the
map, status, clock, ammo and radio/music dashboard panels. It reads your local game assets
directly; no game process, headset or converted/exported models are required.
No game data is distributed with the tool or uploaded by it.

## Build and run

Windows: install Visual Studio's Desktop C++ workload, CMake 3.22+ and Ninja,
then run `BUILD_VEHICLE_EDITOR.bat -Package` from the kit. The script also
recognizes CMake/Ninja installed by the Quest build wizard. The executable and
helper are installed in `out/vehicle-editor`. A ZIP is generated in
`out/vehicle-editor-build`. The Windows build uses the static C/C++ runtime.

Linux: install a C++17 compiler, CMake, Ninja, OpenGL development libraries and
GLFW development files, then run `sh BUILD_VEHICLE_EDITOR.sh`. For example,
Debian/Ubuntu packages are `build-essential cmake ninja-build libgl1-mesa-dev
libglfw3-dev python3`. The output is `out/vehicle-editor-linux`, with a ZIP in
`out/vehicle-editor-linux-build`. Linux packages include the GLFW shared library
when linked to a shared system GLFW; they require the system's OpenGL driver,
standard C/C++ libraries and window-system libraries. Use an installed GLFW where possible; otherwise
CMake downloads and hash-verifies GLFW 3.3.8 source (X11 development headers are
then also needed). CMake 4 users building that older GLFW source may need
`-DCMAKE_POLICY_VERSION_MINIMUM=3.5`.

The Windows package can include Python as described below. Linux device transfers
use the system's `python3`; local viewing/editing does not require Python.

Python 3 and Android platform-tools/ADB are required only for the device helper.
Set their executable paths in the editor if they are not on PATH. A Windows
embeddable Python distribution can be included with
`build.ps1 -PythonRuntime <folder> -Package`; the editor detects `python/python.exe`
beside itself. No Python is needed for local viewing, editing or file export.

Launch `liberty-vehicle-editor`, choose the **GTA III folder**, and click
**Read local game assets**. Optional command line:

```text
liberty-vehicle-editor --game "/path/to/GTA III" --workspace "/path/to/placements"
```

The workspace defaults to `workspace` beside the executable. Keep it local and
outside version control. Local imports, device baselines and backups can contain
personal settings. Never put original game files in this workspace or a release.

## Editing workflow

1. Open or create a workspace. Import placement packs or a legacy INI from the
   local-file section, or import the current headset settings from the device panel.
2. Select Classic, Modern or Xbox and search the catalogue. Missing modelsets and
   unsupported vehicle types remain visible. The source label identifies actual
   geometry/texture sources, including Classic fallback.
3. Choose personal overrides or template/release defaults. Template editing
   previews that layer without personal overrides masking it. Field labels show
   their source; Reset removes the active layer's field and reveals the layer below.
4. Select the wheel, dashboard group, an individual HUD panel or the radio/music player. Sliders, numeric
   entry and move/rotation gizmos update the 3D preview. Size is uniform; wheel
   size is its radius. Units are centimetres, degrees and percent.
5. Use **Driver view**, **Exterior**, mouse look/orbit/pan/dolly and WASD. The field
   of view and preview eye height are camera controls, not saved placement values.
   Camera position, orientation and orbit mode persist when switching vehicle/modelset;
   only the Driver view and Exterior buttons reset the view. Separate pan and
   look/orbit sensitivity sliders adjust mouse speed (default orbit speed is halved).
   A background color picker and Reset steering button are available.
   The seated head animation is approximated by the adjustable eye-above-seat
   value; final viewpoint, reach and comfort need a headset check.
6. Newly authored layouts default to a dashboard fixed in vehicle space.
   Imported legacy layouts keep their neutral-wheel anchor. **Create fixed
   dashboard at wheel** establishes an independent anchor explicitly. Steering
   preview rotates the virtual wheel spokes, not dashboard panels.
7. Toggle model extras to inspect variants. Intact highest-detail components are
   rendered; damaged and lower-detail components are suppressed. Shared GTA III
   wheel models and the first paint combination from `carcols.dat` are applied.
8. Mark reviewed, then use **Next unreviewed** across installed modelsets. Tick
   multiple entries for paste or transfer. Undo/redo and per-field/entry reset
   support repeated work. Reopening the workspace/game rechecks reviewed asset
   fingerprints, including shared wheels and generic textures.
9. Save writes the personal and template packs, retaining `.bak` files. Closing
   with unsaved edits prompts to save/discard/cancel.

The sample panel text illustrates physical placement and dimensions; it does
not run gameplay, controller interaction or a live minimap. Imported vehicle
HUD settings from before `VehicleHudAnchorVersion=1` use the same migrated
dashboard defaults as the game. Classic/Xbox legacy wheel settings remain
ambiguous shared fallback values until explicit entries are authored.

## Placement packs and port behavior

The editor and port share `overlay/src/vr/VehiclePlacement.h`: shipped car/boat
defaults, pack parsing, layer resolution, wheel transforms, and panel rotations.
The port retains its legacy category/model calibration controls and wrist HUD.
Per-model wheel/seat, radio and in-vehicle panel edits write the personal placement pack.
The radio stays independently anchored to the driver seat, unaffected by wheel steering
or dashboard group transforms. RadioX/Y/Z use centimetres from that seat; RadioPitch,
RadioYaw and RadioRoll follow the shared intrinsic rotation order; RadioSize is percent.
Existing radio_board.ini and radio_board_defaults.ini remain compatibility fallbacks
(their millimetres and opposite roll direction are converted during import).
Device import reads both radio fallback files too. Local Import legacy INI recognizes
these filenames; import the defaults and personal files separately when needed.
In-headset per-model radio saves target the shared personal pack; global radio edits
retain the legacy global fallback. Reset removes personal radio fields to reveal templates.

Files under the game's `gamedata` directory:

| File | Purpose |
| --- | --- |
| `vehicle_placements.bundled.ini` | APK release baseline, refreshed from APK assets |
| `vehicle_placements.template.ini` | Replaceable installed template |
| `vehicle_placements.personal.ini` | Personal per-model/modelset overrides |

Each field resolves personal → installed template → bundled → legacy/shipped
fallback. Missing fields inherit independently. A zero offset is an explicit
value, not inheritance. Changes transferred to a headset require a full restart.
Older APKs ignore these packs; install a port build containing this integration.

```ini
[Placement]
Version=1

[xbox:95]
WheelX=0
WheelY=4
WheelZ=8
WheelPitch=-15
WheelRadius=18
DashFollowWheel=0
DashX=-35
DashY=40
DashZ=65
MapX=-10
MapPitch=5
MapSize=55
```

Sections are `classic:<model ID>`, `modern:<model ID>`, or `xbox:<model ID>`.
Wheel offsets are relative to the driver's seat plus the port's existing
33 cm forward / 30 cm up control origin. Wheel pitch/yaw/roll retain the port's
±90° limits. Seat fields are effective category + model offsets.
DefaultSeatHeight/DefaultSeatDistance belong to DEFAULT driving independently.
DashX/Y/Z are vehicle-local when DashFollowWheel=0, or neutral-wheel-local when
it is 1. Panel X/Y/Z are right/forward/up offsets in the dashboard frame;
rotations use intrinsic pitch about right, yaw about up, roll about forward.
DashSize scales the panel offsets and sizes about the dashboard anchor.

For future release defaults, copy an exported template pack to
`overlay/android/app/src/main/assets/vehicle_placements.bundled.ini` before
building the APK. The APK refreshes only its bundled baseline. Installed
templates, personal settings, game data and saves remain separate.

## Device import and deployment

Connect an authorized Quest 3 and close Liberty City VR. **Import** reads the
three placement layers plus legacy `vr_settings.ini` and saves a local baseline.
Existing workspace files get `.before-import` backups. Import does not copy game
assets or saves. If multiple eligible headsets exist, choose an index from the
device helper's count; identifiers are never printed.

Choose entries and use **Preview selected changes** before installing a template
or personal pack. The helper rereads the device and merges against the import
baseline, preserving unrelated device edits. Conflicts list modelset/model/field
names; select local or remote resolution explicitly if needed. Reset fields are
merged as deletions. A missing import baseline blocks transfer.

The helper refuses a running game, uploads to a temporary path, reads back and
compares bytes, rechecks the destination, makes a timestamped device backup, then
renames the verified upload. Failed uploads retain existing settings. Restart the
game afterward. No APK uninstall, data erasure or automatic game-asset transfer
is performed. Offline Save always remains available.

The helper is also usable directly; run `python device_bridge.py --help`.
To restore a backup reported by deployment, use `python device_bridge.py restore
--workspace <folder> --kind personal --backup <reported-backup-filename>` (choose
`--kind template` for a template backup). Restore validates the backup and saves
the current configuration as a new backup; import again before subsequent edits.
Command output and errors are sanitized. ADB identifiers exist only inside the
helper process and its locally invoked ADB argument array.

## Validation

`ctest --test-dir <build> --output-on-failure` runs portable pack/math/data tests.
`python tools/vehicle-editor/test_device_bridge.py -v` tests merge conflicts,
eligibility/privacy, corrupted/interrupted transfers and backups without a device.
`--smoke 3` renders three frames; `--smoke-all` visits every supported vehicle in
available modelsets and reports failures. It reads only local game assets.
On WSLg systems with a failing GPU driver, `LIBGL_ALWAYS_SOFTWARE=1
GALLIUM_DRIVER=llvmpipe` enables a software graphics smoke test.

Desktop rendering does not reproduce the Quest's lighting or stereo optics.
Modern geometry requires a locally installed compatible overlay to validate.
No assets, headset images or settings are included in automated test fixtures.
