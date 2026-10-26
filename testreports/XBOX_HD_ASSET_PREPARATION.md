# Xbox Version HD preparation validation

Date: 2026-10-26. Local Windows execution on branch
`codex/xbox-hd-asset-preparation`. Game/mod assets remained local and were not
staged or redistributed.

## Source and outputs

- Actual ModDB re3 ZIP: 1,552,864,677 bytes; published MD5 matched.
- SHA256: `879024BED6FB89C218F6C75A46119F95FC068A64849BC74DEDF0851D3FD210A6`.
- Pinned allowlist: 92 files. PC executables, scripts, saves, settings, optional
  audio and language files were excluded from extraction/preparation.
- Xbox and Modern each built with 4,051 merged IMG entries, 62 vehicle models,
  101 vegetation names and 90 fully hashed output files.
- Both outputs passed manifest/inventory/IMG validation. Original game
  sentinels were unchanged. A separate fresh Bash Modern build also passed.
- PowerShell 7 built Both; Windows PowerShell 5.1 and Bash reused Both after
  validation. The supported Bash entry point preserved an intentional failure
  exit code and succeeded after validating Both.

## Automated checks

| Check | Result |
|---|---|
| Synthetic Python production-builder tests | 5 passed: both profiles, precedence, integrity, extraction allowlist, source pinning, rollback, reuse, mirror and blocked HTML handling |
| Production modelset startup/category/path/calibration tests | 70 checks passed |
| Weapon grip | 2,304 checks passed |
| Wheel hand basis | 22,236 checks passed |
| Radio touch and laser cache | Passed |
| Fake-ADB privacy regression | Passed; no real-device identifiers in diagnostics |
| Edited PowerShell/Bash syntax | Passed |
| Full patch series against pristine LF upstream | 2 tests passed |
| Fresh current-source gate | 55 suites passed; 4 failed; 7 reference fixtures excluded |
| Full ARM64 debug APK | Built successfully and updated the personal Quest 3 device using `install -r` |

The four failing suites are previously reported fixture problems:
`graphics-menu-layout` (unused helper warning treated as error), `quest-melee`
(missing ModelSets fixture declarations), `ragdoll-menu` (menu extraction),
and `vehicle-seat-recenter` (outdated signatures and missing fixture types).
They are not passes and the overall regression gate remains red. The laser
cache fixture was updated for the new calibration API and passed in the final
full run. The restored Liberty City modelset suite is now part of that gate.

The real ModDB automatic-download request returned HTTP 403 in this environment.
The downloaded local ZIP supplied by the user completed real-pack validation.
Fresh mirror selection and invalid/HTML responses were covered with bounded
synthetic HTTP responses; a successful live automated download is unverified.

## Headset acceptance still needed

Only the task APK was installed. The generated asset overlays were not
transferred, selected or visually exercised on the headset. See the affected
category table in [QUEST_MODELSETS.md](../overlay/docs/QUEST_MODELSETS.md).

Check the full city/LODs and collisions, HUD/fonts/radar/loading in both eyes,
vehicle wheels/controls/seat fit, player/pedestrian skinning and ragdolls,
cutscenes, weapons/muzzles/scopes, shared-texture category mixes and sustained
GPU/memory/thermal behavior. Existing Xbox calibration overrides need visual
rechecking for this different pack. No claim of release-ready visual or
performance validation is made.

Detailed local output is under ignored `.cache/xbox-hd/`; no original assets
or private device diagnostics are included in this report.
