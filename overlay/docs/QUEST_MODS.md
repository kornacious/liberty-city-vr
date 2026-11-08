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
collision, water, object data and SCM through read-only overlays on Classic.
The requested Xbox source is preserved for subsequent unmodded launches.

Model-name traits provide garage doors, animated trees/banners and breakable
glass. Rotating garage doors clamp/update their initial position. Parked cars
are excluded from traffic-cap totals, persistent script debug state is retained,
FXT text overrides supplement the base GXT, and rural zones use leaf rubbish.
The package has no `policeZoneSfx.dat`, CLEO ASI or external CLEO scripts; the
base police-radio mappings remain in place. Windows ASIs are neither loaded nor
executed. Generic Windows ASI/CLEO binary compatibility is outside this adapter.

Upstate saves/settings are under the app's
`files/userfiles/upstate-liberty-1.1.2/`. Classic/Xbox saves remain in
`files/userfiles/`. Manager save transfers currently address Classic/Xbox slots;
they do not import incompatible desktop Upstate saves into the mod directory.
Disabling/deleting the mod preserves both save directories.

The shipped empty PC cull cache is not deserialized into the expanded tables.
IPL attribute zones remain active; legacy precomputed visibility cull zones are
skipped, using the Quest renderer's ordinary visibility paths. Sustained frame
time, all districts/LODs, traffic, missions, garages, save/load and transitions
still require manual headset acceptance. Compilation and package checks alone
do not establish full mission completion or performance parity.

Initial local acceptance: New Game reached gameplay on the personal Quest 3 device
and the user confirmed it on 8 November 2026. The DAT overlay lookup trims the
terminal whitespace produced by the native line reader; this has a regression
check. The shipped APK uses the normal frontend, without the private automatic
New Game driver used during diagnosis.

The Appearance menu labels the active source `CLASSIC - UPSTATE` and locks
source switching while Upstate is enabled. The saved unmodded source remains
visible and is preserved; disabling Upstate restores that source on the next
launch. Combining Upstate with Original Xbox assets remains unvalidated.

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
    "Repository": "D:/Projects/GTAVR/liberty-city-vr-quest",
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
