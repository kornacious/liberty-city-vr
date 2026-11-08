# Curated mods in the VR Manager

Open **Mods** in Liberty City / VR Manager. The initial catalog entry is
**Upstate Liberty v1.1.2**. Choose an operation, then **Perform**:

- **Status** reads installed/enabled/runtime-support state from the selected Quest 3.
- **Validate locally** prepares the package without discovering or accessing a headset.
- **Install** uploads and verifies a staged package, then replaces the previous managed
  package with rollback if activation fails.
- **Enable** verifies installed file hashes again before activation.
- **Disable** retains the files for later re-enabling.
- **Delete** removes the managed device package and enable marker. It preserves all
  saves, the supplied archive and the local prepared cache.

Select a local `UL.7z`, or leave the package path blank to download exactly:

<https://github.com/Adi23041999/UpstateLiberty/releases/download/v1.1.2/UL.7z>

Both sources must be 32,225,684 bytes with SHA256
`818C909583AFFAE9D89A5C597E28E83A1D733B7DFFEAAB8D99BA93E5194EDC76`.
This digest matches the official release asset metadata and the supplied local
archive. Other versions/repacked archives are rejected without changing the source.
Install 7-Zip (`7zz` or `7z`) or select its executable in **Settings**.
The archive and prepared payload remain under `<workplace>/mods`, outside Git.
Windows ASI executables are never executed or transferred to the headset.

## Native engine adapter

The APK includes separate Classic/Xbox and Upstate engine libraries. A small
NativeActivity startup selector loads exactly one. Upstate is selected only when
the enable marker is correct and all 44 payload files match the pinned v1.1.2
sizes and SHA256 fingerprints. Incomplete, modified or linked packages select
Classic/Xbox instead. The Upstate engine repeats validation before initialization
and stops startup if the package changed after selection.

The normal engine retains its existing fixed tables and save serialization.
Upstate has its own expanded model/entity/path/zone/garage/phone/pickup/particle
tables and fourfold save working buffer/storage size. Path indices retain signed
16-bit limits. Portable pickup, phone, zone and garage save-block sizes follow
the expanded counts; the legacy fixed-layout PC save converter is disabled only
in the isolated Upstate engine. The supplied main SCM's main segment (88,719 bytes), largest
mission (28,469 bytes) and mission count (82) fit the existing native VM limits.
The adapter routes the package's map manifest, IMG/DIR, definitions, placements,
collision, water, object data and SCM through read-only overlays on the selected
Classic or Original Xbox source. Source changes save immediately and apply on a
full process restart. Changing the asset source does not change the save format.

Both loose files and streamed archive entries use the same precedence:
Upstate, then the selected Original Xbox source, then the Classic base fallback.
Archive directories are visited by layer before registering their first-wins
model, texture and cutscene-extra entries; registration order cannot let Xbox
overwrite Upstate. Within a layer the native reverse archive order is preserved.
Xbox definitions, world collisions, audio, particles and VR model calibration
remain active for assets not replaced by Upstate. DAT filename whitespace is
trimmed before both mod and Xbox lookups.

Model-name traits provide garage doors, animated trees/banners and breakable
glass. Rotating garage doors clamp/update their initial position. Parked cars
are excluded from traffic-cap totals, persistent script debug state is retained,
FXT text overrides supplement the base GXT, and rural zones use leaf rubbish.
The package has no `policeZoneSfx.dat`, CLEO ASI or external CLEO scripts; the
base police-radio mappings remain in place. Windows ASIs are neither loaded nor
executed. Generic Windows ASI/CLEO binary compatibility is outside this adapter.

The Quest frontend's numbered saves (`1.b` through `8.b`) and `autosave.b` are
stored in `files/gamedata/`, shared across configurations. The native user-files
directory is separate for Upstate settings, but does not relocate these slots.
Disabling/deleting the mod preserves the saves. Manager save transfers validate
the base format and reject expanded Upstate saves.

The numbered slot list verifies the save's block structure, configuration
identity and checksum before identifying a foreign configuration. With Upstate
disabled, valid Upstate saves display `Slot X was saved with Upstate Liberty Mod`.
With Upstate enabled, valid base saves display `Slot X was saved with base game.`
These entries cannot enter load confirmation. Compatible saves retain their
normal mission name and date; damaged/unrecognized saves retain the corruption
label. Ordinary save overwrite confirmation remains available. No save is
converted, moved or rewritten by this inspection.

`test-save-compatibility.cpp`, included in `test-modelset-profiles.ps1`, covers
both configurations, slot labels, replay flags, checksum failures, malformed
subsystem/script blocks, truncation and trailing bytes without game assets.
On 8 November 2026, the affected local slot passed Upstate format/checksum
validation and remained byte-identical after APK installation and launch. With
Upstate disabled on the personal Quest 3 device, the user confirmed that the
new Upstate slot message displayed correctly. The reverse base-save message is
covered by host regression tests.

The shipped empty PC cull cache is not deserialized into the expanded tables.
IPL attribute zones remain active; legacy precomputed visibility cull zones are
skipped, using the Quest renderer's ordinary visibility paths. Sustained frame
time, all districts/LODs, traffic, garages, transitions and full mission completion
still require broader manual headset acceptance. Compilation and package checks alone
do not establish full mission completion or performance parity.

Initial local acceptance: New Game reached gameplay on the personal Quest 3 device
and the user confirmed it on 8 November 2026. The DAT overlay lookup trims the
terminal whitespace produced by the native line reader; this has a regression
check. The shipped APK uses the normal frontend, without the private automatic
New Game driver used during diagnosis.

Follow-up manual acceptance of Classic-based Upstate on the personal Quest 3 device, reported by the user
on 8 November 2026: normal menu startup is restored; saving and loading with
Upstate enabled works; original missions work; side missions can be activated
both in the new area and on the original islands. The user did not observe
missions specific to the new area; this is not a complete content audit.
Performance appeared comparable to the base game, with perceived dips of a few
frames per second in busier areas. These are subjective observations, without
instrumented frame-time measurements or an exhaustive mission playthrough.

The Appearance menu offers Classic and Original Xbox while Upstate is enabled,
with a note explaining that Upstate overrides the selected base assets.

Local compatibility audit of the pinned v1.1.2 package and prepared Original
Xbox US profile: 2,033 Upstate model IDs have no overlap with the 3,084 Xbox
definitions, and all 13,926 Upstate placement records resolve to matching model
IDs/names in their combined definitions. The archives share 32 entry names
(two models and 30 texture dictionaries); the loose layouts share two IPLs.
Upstate takes precedence for these overlaps. Host regression checks cover both
registration orders, source switching/restart, base fallback and Xbox audio/
calibration behavior. These checks do not establish visual/collision correctness
or headset performance for the combined configuration; those require playtesting.
The compatibility APK was installed on the personal Quest 3 device on 8 November
2026; startup logs confirmed the Upstate overlay and Original Xbox source active
together. The user then confirmed that gameplay looked correct when asked to
check the new area and an original island with the combined configuration.
No crashes, missing textures, misplaced objects or collision problems were
reported in that check. This establishes initial manual gameplay acceptance;
extended playtesting and measured performance comparisons remain outstanding.

Launch an updated APK once to publish runtime capabilities, then install/enable
Upstate through the Manager and fully restart. Older APKs remain gated. Do not
create the capability file manually. Base game files and saves stay intact.

## Device protocol

All paths below are relative to the app's `files/gamedata` directory:

- Package: `mods/upstate-liberty/`; ownership/inventory: `.lcvr-package.json`.
- Enable marker: `mods/upstate-liberty.enabled`, containing
  `upstate-liberty:1.1.2:<uppercase archive SHA256>` and a newline.
- Runtime capabilities: `mods/.runtime-capabilities`, written by the game, with
  one versioned adapter token per line. Upstate's token is `upstate-liberty/1`.

The Manager never writes the runtime-capabilities file. The startup selector
refreshes it to reflect the built adapters and rejects invalid/incomplete package
state. Missing support allows verified installation but
keeps the package disabled. Supported runtimes automatically enable on install.
Status describes desired activation for the next game launch, not proof of current
gameplay compatibility. Device updates use unique staging/backup directories;
unmanaged directories are not replaced or deleted. Re-enabling checks manifest
identity and every payload hash. Local cache files are revalidated before reuse.

Headless local validation example:

```json
{
  "Settings": {
    "Repository": "D:/Projects/GTAVR/liberty-city-vr",
    "Workplace": "D:/Projects/LCVRBuild"
  },
  "Options": {
    "ModId": "upstate-liberty",
    "ModArchive": "D:/Packages/UL.7z",
    "ModOperation": 5
  }
}
```

Run `dotnet run --project tools/manager -c Release -- --run ManageMods request.json`.
Operations: 0 status, 1 install, 2 enable, 3 disable, 4 delete, 5 local validation.
