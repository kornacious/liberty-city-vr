# Liberty City Vehicle Placement Editor

Standalone, non-VR Windows/Linux editor for GTA III virtual wheels and the
map, status, clock, ammo, radio/music and speedometer dashboard elements. It reads your local game assets
directly; no game process, headset or converted/exported models are required.
No game data is distributed with the tool or uploaded by it.

## Build and run

Open Liberty City VR Manager (`MANAGE.bat` or `dotnet run --project tools/manager
-c Release`) and choose **Vehicle editor**. Configure the output directory and
ZIP packaging, then **Perform**. The manager builds all three native test targets,
runs CTest, installs the editor and packages the chosen output.

Windows requires Visual Studio/Build Tools with the Desktop C++ workload and CMake 3.22+.
Linux requires C++17, CMake, Ninja, OpenGL/X11 and GLFW development dependencies.
CMake can obtain pinned GLFW 3.3.8 if it is not installed. Builds are stored under
the configured workplace; default installation is `<workplace>/vehicle-editor`.
The manager runtime is included beside the editor in `manager/`.

Device transfer invokes the manager's embedded bridge. Configure Python 3 in
manager Settings and ADB in the manager/editor as needed. Local viewing/editing
and file exports require no Python, headset or game process. The editor's device
panel now asks for a **Manager executable**, rather than a script/interpreter.
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
2. Select Classic or Modern and search the catalogue. Missing modelsets and
   unsupported vehicle types remain visible. The source label identifies actual
   geometry/texture sources, including Classic fallback.
3. Choose personal overrides or template/release defaults. Template fields show the authored layer. **Preview effective game values**
   includes higher-priority personal overrides in the viewport. A warning and an
   explicit clear-personal action identify masked templates; turn effective preview
   off to author a template independently (gizmos are disabled while masked). Field labels show
   their source; Reset removes the active layer's field and reveals the layer below.
4. Select the wheel, dashboard group, an individual HUD panel, radio/music player or speedometer. Sliders, numeric
   entry and move/rotation gizmos update the 3D preview. Size is uniform; wheel
   size is its radius. Units are centimetres, degrees and percent.
5. Use **Driver view**, **Exterior**, mouse look/orbit/pan/dolly and WASD. The field
   of view and preview eye height are camera controls, not saved placement values.
   Camera position, orientation and orbit mode persist when switching vehicle/modelset;
   Driver view and Exterior reset the view manually. Enable **Auto reset camera on
   vehicle / modelset change** above Orbit to return to the driver view on selection. Separate pan and
   look/orbit sensitivity sliders adjust mouse speed (default orbit speed is halved).
   A background color picker and Reset steering button are available. X and Y
   inversion toggles affect look/orbit and pan independently. Enable VSync to
   synchronize presentation with the display; the FPS limit accepts 0 (unlimited)
   or 1�1000. With both enabled, the slower constraint determines the frame rate.
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
dashboard defaults as the game. Classic/Modern legacy wheel settings remain
ambiguous shared fallback values until explicit entries are authored.

The speedometer has one **bottom-centre pivot** shared by Digital pod and
Classic analog. Its position, intrinsic pitch/yaw/roll and uniform size are saved
per vehicle model and modelset, using `SpeedometerX/Y/Z`,
`SpeedometerPitch/Yaw/Roll` and `SpeedometerSize`. Units are centimetres, degrees
and percent. It follows the dashboard group, including group scale, but never
physical steering rotation. Style changes keep the base of the instrument fixed;
the analog face extends higher than the digital pod.

Select **Speedometer** in Placement to preview either style, MPH or KM/h, and
a sample speed. These preview controls are editor preferences, not placement
fields. The editor uses the same procedural instrument geometry as the game,
with a sample handling estimate of 180 km/h. Actual in-game analog ranges use
each car's cruise-velocity handling estimate with 10% headroom and rounded
ranges; the scale stays independent of current speed. The needle clamps at the
end of its sweep, while the numeric reading can exceed the dial's maximum.

In game, open **Vehicles → Speedometer** or **HUD → Vehicle speedometer**.
Visibility, style and units persist in VR settings; placement edits save
immediately into the current model/modelset's personal placement pack.
Reset removes only those personal speedometer fields and reveals inherited
template/bundled values. Both styles render in immersive car driving; editor
placement in a boat does not enable an in-game boat speedometer.

Modern reads the Xbox HD assets prepared under `modelsets/modern` in the selected
game folder, with Classic fallback for missing files. Old Modern or Xbox editor
selections reopen as Modern. Modern placement records retain the `xbox:` section
prefix for compatibility with the game's HD calibration identity; existing
placement files are preserved. Shared tyre and rim textures also come from
`models/MISC.TXD` in the selected base game folder.

## Application preferences

`vehicle-editor.ini` is created beside the executable, independently of vehicle
placement packs. It remembers the game-data folder, workspace, Python/ADB paths,
camera pose, orbit/auto-reset/inversion settings, mouse sensitivities, FOV,
preview eye height, background, preview visibility and editing selections,
VSync and FPS limit. Game/workspace paths are saved as absolute paths; command
line `--game` and `--workspace` values override saved paths for that launch.
Normal launches automatically read assets from the remembered game folder.

Settings save after a short pause in changes and on exit; **Save application
settings** saves immediately. The previous file is retained as `.bak`. Keep the
portable editor in a writable folder; write failures appear below the save
button. Invalid/unsupported settings are preserved until you explicitly save
replacement preferences. Window/panel layout is stored separately beside the
executable in `vehicle-editor-ui.ini`. These files contain local paths and should
stay local, outside source control and release archives. Keep these INIs when
replacing the executable to retain preferences. Automated `--smoke` runs do not
save application preferences.

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
The current release baseline includes manually calibrated Classic and Xbox
wheel/radio entries. Xbox placements, including Speeder, are calibrated for
the GTA III Fixed XBOX Vehicles v1.0 pack. Fields without authored overrides
continue to use the existing fallback; Modern retains its previous defaults.

## Device import and deployment

Connect an authorized Quest 3. Import and deployment can run while the updated game is running. **Import** reads the
three placement layers plus legacy `vr_settings.ini` and saves a local baseline.
Existing workspace files get `.before-import` backups. Import does not copy game
assets or saves. If multiple eligible headsets exist, choose an index from the
device helper's count; identifiers are never printed.

Choose entries and use **Preview selected changes** before installing a template
or personal pack. The helper rereads the device and merges against the import
baseline, preserving unrelated device edits. Conflicts list modelset/model/field
names; select local or remote resolution explicitly if needed. Reset fields are
merged as deletions. A missing import baseline blocks transfer.

The helper uploads to a temporary path, reads back and
compares bytes, rechecks the destination, makes a timestamped device backup, then
renames the verified upload. Failed uploads retain existing settings. The updated game polls validated packs every 250 ms. A nonce and content hash
acknowledgement confirm that the running game loaded the uploaded file. If the
headset is paused/asleep or the APK is old, the helper reports a missing
acknowledgement instead of claiming live success. Closed games load the pack at
startup. Direct CLI transfers require `--live` to allow a running game; restoration
still requires closing the game. No APK uninstall, data erasure or automatic game-asset transfer
is performed. Offline Save always remains available.

The helper is also usable directly; run `LibertyManager --vehicle-bridge --help`.
To restore a backup reported by deployment, use `LibertyManager --vehicle-bridge restore
--workspace <folder> --kind personal --backup <reported-backup-filename>` (choose
`--kind template` for a template backup). Restore validates the backup and saves
the current configuration as a new backup; import again before subsequent edits.
Command output and errors are sanitized. ADB identifiers exist only inside the
helper process and its locally invoked ADB argument array.

## Validation

`ctest --test-dir <build> --output-on-failure` runs portable pack/math/data tests.
`dotnet run --project tools/manager -c Release -- --tool tools/vehicle-editor/test_device_bridge.py -- -v` tests merge conflicts,
eligibility/privacy, corrupted/interrupted transfers and backups without a device.
`--smoke 3` renders three frames; `--smoke-all` visits every supported vehicle in
available modelsets and reports failures. It reads only local game assets.
On WSLg systems with a failing GPU driver, `LIBGL_ALWAYS_SOFTWARE=1
GALLIUM_DRIVER=llvmpipe` enables a software graphics smoke test.

Desktop rendering does not reproduce the Quest's lighting or stereo optics.
Modern geometry requires a locally installed compatible overlay to validate.
No assets, headset images or settings are included in automated test fixtures.

### Live synchronization and template masking

After importing, enable **Live sync current entry to headset**. Edits are saved
and sent for the current vehicle/modelset and edit layer after a 700 ms pause;
this excludes other ticked entries. Editing remains available during transfer.
A conflict or missing acknowledgement disables live sync and shows the error.
Live sync is intentionally re-enabled per session rather than remembered.

Personal overrides always outrank templates. For example, a personal RadioRoll
of -90 continues to win over a template RadioRoll of 61.64. The effective game
preview shows this result, while template fields continue to show their authoring
values. **Clear personal overrides and edit personal layer** removes that entry's
personal fields locally; installing that personal layer (or live sync) deploys the
reset and reveals the template. Other entries remain untouched. Existing personal
settings are never silently erased by a template deployment.

The game retains the last good generation for malformed updates; removing a pack
reveals lower layers. Radio preview and rendering use shared float transforms;
legacy radio menu values retain their millimetre/integer-degree controls.
