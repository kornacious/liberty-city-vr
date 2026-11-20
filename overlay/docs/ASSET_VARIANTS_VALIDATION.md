# Complete PC/Xbox variant validation

Date: 2026-11-13. Local Windows execution; original game assets and generated
installations remained on the user's computer. This records the implementation
following the [dependency audit](XBOX_STANDALONE_DEPENDENCY_AUDIT.md).

For subsequent RC playthrough results and Linux coverage, see [platform status](../../PLATFORMS.md).

## Gameplay regression reported 2026-11-14

The user confirmed successful installation on Quest 3 and Windows PCVR, but
reported a hard freeze with the Original Xbox variant while driving Misty to
her destination in the early Luigi mission. Both targets are affected. The
opening-path checks below do not establish mission compatibility. The retained
Windows action log stops during gameplay without a recorded crash exception;
the process had already exited when investigation began.

A subsequent live Windows capture located the game thread in
`CFont::GetStringWidth`, called while drawing a HUD help message. The Xbox
`CYCLE_CAMERA_MODE` control action differs from the engine's
`CAMERA_CHANGE_VIEW_ALL_SITUATIONS`. The old substitution routine removed only
the control-token prefix for unknown actions, leaving an unmatched `~`; the
font scanner then searched past the string terminator. This explains why an
apparently driving-related freeze appeared when the camera tutorial displayed.

Patch 94 maps the two Xbox-specific action aliases (`CYCLE_CAMERA_MODE` and
`CAR_GUN`), expands messages within their fixed buffer capacity, and consumes
unknown actions without leaving token delimiters. Font token scans now stop at
NUL, including Japanese delimiters. Existing installations need an executable/APK
update; no asset reconversion is required.

Both Windows engines and the Quest ARM64 APK compiled successfully. All eight
Windows CTests passed, including the new alias, unknown-action, truncation,
Japanese and bounded-scanner regression checks. The Windows installation and
the personal Quest 3 device were updated, preserving assets and saves. The user
subsequently confirmed the corrected Quest 3 build works at the previously
failing gameplay stage. This validates the reported freeze fix; a complete
playthrough and sustained performance checks remain outside this check.

## Implemented

- Independent complete installations under `variants/pc` and `variants/xbox`.
  Runtime content comes from one selected root; there is no cross-variant fallback.
- Unified manager Build and install controls for either/both variants, with enabled
  source fields tied to the checkboxes. The former Xbox-only action page is hidden;
  saved ISO paths are reused. Legacy CLI import actions explain the new workflow.
- Sole-source automatic selection, persisted selection when both are present,
  unavailable-source notices, and an asset-independent missing-installation screen.
- Complete ISO preparation includes scripts, gameplay/path data, collision,
  animations, text and converted visuals/audio. Existing custom settings layout
  remains; missing frontend icons are generated port resources and radio badges
  have station labels. GXT merging preserves existing Xbox strings.
- Checksummed staging and retained backups; app-owned Quest activation avoids
  scoped-storage shell chmod/rename restrictions. Shared VR/re3 settings, saves,
  skins, custom music and mod storage remain outside the selected content root.
- Portable LF inventories; startup also accepts existing CRLF inventories.
  Shared Android profile writes use temporary files and atomic replacement.

## Checks completed

| Check | Result |
| --- | --- |
| .NET manager regression suite | 131/131 passed, including coexistence, replacement protection, cancellation, inventory completeness and GXT preservation |
| Manager UI scenarios | Passed both-checkbox/path enablement, target switching, draft preservation, Cancel, launch/diagnostics and existing general UI scenarios |
| Windows Release compilation | Both game engines and diagnostic executable built; seven selected CTest checks passed |
| Windows Xbox-only startup check | Passed with no PC variant and no legacy root `data`/`models` directories; includes Windows media/platform initialization without an OpenXR session |
| Source selection and corruption | Saved PC/Xbox choices honored; missing Xbox frontend causes incomplete-source rejection rather than PC-file borrowing |
| Production routing fixtures | Complete-variant, Upstate precedence and save-configuration checks passed |
| Shared/custom settings fixtures | 151 shared-settings checks and 2,998 native-settings/layout checks passed |
| App-owned activation Java fixture | Passed identity/path validation, missing/size-changed files, unmanaged destinations, retained backups and PC/Xbox coexistence |
| Xbox converter | Built locally; LZO CTest passed; converted assets structurally reopened and audio decoded/verified during preparation |
| Quest ARM64 APK | Built successfully and updated on personal Quest 3 device |
| Quest assets | Both complete variants transferred with per-file SHA256 verification and activated through the app bridge |
| Quest Xbox smoke test | Selected `ORIGINAL XBOX`, created the OpenXR/Vulkan session and completed initial game setup; process remained running |
| Interactive Quest check | User confirmed menus and opening scene work; diagnostics showed live gameplay and model streaming |

The initial Quest check exposed CRLF handling that Windows text streams had
hidden. After correcting the reader and adding the line-ending regression, the
repeat selected Xbox correctly. A shell permission failure during the first asset
deployment was resolved with app-owned activation; verified staging was recovered
and existing installations were preserved.

The automated smoke test temporarily requested Xbox, then stopped the app and restored the
original settings bytes. The subsequent user-assisted check launched Xbox again
and left the game running with Xbox selected. No original installation or save was erased by installation. Legacy PC
data and the old Xbox overlay remain preserved; they were not used to supply
fallback files to the managed Xbox variant.

## Acceptance limits

The initial automated Quest session stayed inactive. During the subsequent
interactive check, the user confirmed the menus and opening scene work, and
diagnostics showed gameplay and model streaming. This establishes an opening-path
check, not a complete playthrough: later missions, all districts, save
interoperability between variants and sustained performance remain unverified.

Windows Vulkan discovery succeeded, but the active VirtualDesktopXR runtime
reported no connected PCVR headset. Full Windows VR gameplay was therefore not
validated. Optional renderer GPU and audio-endpoint CTest suites were not enabled;
the selected seven native checks and the separate Windows media initialization
check passed. Desktop results do not establish Quest performance.

Windows builds/data validation used a new isolated local installation; the user's
configured live Windows installation and global launch settings were preserved.
Game data, generated products, device diagnostics and private identifiers are not
included in source control.
