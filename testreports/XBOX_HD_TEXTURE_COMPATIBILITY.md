# Xbox HD texture compatibility and two-profile selection

This report records an earlier importer. For 0.9.5 installation and source
selection, use the [complete PC and Original Xbox variant guide](../overlay/docs/QUEST_MODELSETS.md).

Date: 2026-10-26. Local execution on `codex/xbox-hd-asset-preparation`.
The working PC installation was inspected read-only. Original/mod assets remained
local; no asset or executable is included in the source changes.

## Desktop comparison

The working Windows copy's `gta3`, `xbox_models`, `xbox_hd_textures`,
`xbox_vehicles` IMG archives, `generic.txd` and bundled `re3.exe` match the
supported ModDB source. Its GTA3.DAT registers the additional archives in that
order. The preparation builder's later-layer precedence matches the engine's
reverse directory traversal; the merged output contains 4,051 entries.

Desktop re3 installs `CustomPipes::SetTxdFindCallback()` under
EXTENDED_PIPELINES. That callback first searches the current model dictionary,
then the global `generic` dictionary. Quest disables the desktop pipeline flag
and consequently omitted this lookup despite loading the shared dictionary.

The local audit matched DFF material texture names to the dictionaries assigned
by the PC installation's loaded IDEs. Two hundred models reference textures
absent from their own dictionaries but supplied by generic; another 351 use
generic directly. Examples include roads/pavements and the cliff/grass materials
seen as white terrain. The police and ambulance base material names are present
in their own vehicle dictionaries; their depth problem has a separate cause.

## Runtime corrections

- Install the global generic fallback in the Quest game lifecycle without
  enabling the desktop Neo pipelines. Preserve local texture precedence and
  restore the previous callback before dictionary shutdown.
- Preserve depth for fully opaque pixels in mixed vehicle paint/glass alpha
  atomics. Previously the Vulkan vehicle pass disabled depth writes for all
  such materials, undoing the engine's intended bodywork depth behavior and
  allowing later interior surfaces to overwrite the body. Translucent pixels
  still blend without depth writes. Explicit depth-off windscreens stay depth-off.
- Offer only Classic and Modern (Xbox HD). The complete pack switches together,
  including vegetation; old per-category choices no longer split models/TXDs.
  Migrate saved Xbox preset 2 to Modern 1 while retaining Xbox as the internal
  calibration identity. Do not register the duplicate Xbox archive.
- Prepare/install only `modelsets/modern`. Old Xbox/Both arguments are aliases
  for Modern. The old Xbox folder is ignored rather than deleting user data.

## Verification

| Check | Result |
|---|---|
| Production two-profile startup/routing/migration/calibration fixture | 77 checks passed |
| Production generic callback/lifecycle and vehicle alpha depth policy | 18 checks passed |
| Pinned builder synthetic cases | 7 passed |
| Opaque C888 texture conversion | Passed; obsolete old-vehicle-pack cases retired |
| Prompt-before-input pipeline | Passed in Windows PowerShell 5.1 |
| Actual Modern cache reuse | Passed in Windows PowerShell 5.1 and Bash; old Both alias prepared only Modern |
| Full patch series against pristine LF source | 2 passed |
| Complete ARM64 debug APK | Built successfully in 1m 7s and updated the personal Quest 3 device |
| Current-source gate | 56 passed, 4 failed, 7 reference fixtures excluded |

The four existing fixture failures remain `graphics-menu-layout`, `quest-melee`,
`ragdoll-menu` and `vehicle-seat-recenter`. They are not passes and the full gate
is still red. The new texture fixture is included in the current-source gate.

The existing installed Modern overlay source marker was confirmed. The pack's
asset bytes did not need rebuilding for these runtime corrections. No PC game
files, base Quest game data or saves were replaced by this task's APK update.

## Headset acceptance

These tests establish the missing texture dependency and verify the callback,
selection and depth policies; they do not constitute a visual pass of the user's
screenshots. Retest the white cliff/ground locations, police and ambulance
bodywork/windows, player/pedestrian skinning, cutscenes, weapon alignment,
other districts/LODs and sustained GPU/memory/thermal behavior. The complete HD
vegetation is now part of Modern and needs performance measurement. Desktop Neo
lighting/reflection effects are separate from texture lookup compatibility.

Detailed local diagnostics remain in ignored `.cache/xbox-hd/`.
