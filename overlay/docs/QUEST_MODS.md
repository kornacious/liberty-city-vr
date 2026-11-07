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

## Runtime compatibility is still required

**The current APK does not yet implement the Upstate engine adapter.** Installing
the package therefore leaves it **disabled** and reports that runtime support is
required. Enabling is refused until a compatible APK advertises that support.
This Manager feature does not itself implement Upstate gameplay, extended engine
limits, mission-script compatibility, FXT text loading or mod-specific save routing.
It does not provide Windows ASI binary compatibility.

A compatible runtime must activate Upstate against Classic, provide separate saves,
and finish the Upstate capacity/behavior/script adaptations before advertising the
capability described below. Do not create the capability file manually to bypass
the check. Base game files and saves must remain intact. Fully restart the game
after changing mod activation. The Manager stops the game before device changes.

## Device protocol for the future engine adapter

All paths below are relative to the app's `files/gamedata` directory:

- Package: `mods/upstate-liberty/`; ownership/inventory: `.lcvr-package.json`.
- Enable marker: `mods/upstate-liberty.enabled`, containing
  `upstate-liberty:1.1.2:<uppercase archive SHA256>` and a newline.
- Runtime capabilities: `mods/.runtime-capabilities`, written by the game, with
  one versioned adapter token per line. Upstate's token is `upstate-liberty/1`.

The Manager never writes the runtime-capabilities file. A future game build must
refresh it to reflect its actual adapters, clear obsolete capabilities, and reject
invalid/incomplete package state. Missing support allows verified installation but
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
