# Original Xbox direct head attachment

Local Windows work, 2026-11-06. Existing retail assets, saved selection and saves
were preserved. This change requires only an APK update.

## Behavior

Original Xbox first-person gameplay now submits the native head bone position and
complete orientation as the final camera anchor. Headset tracking remains relative
to that anchor. Classic and third-person camera paths are unchanged.

Native HAnim matrices are captured in `VrBodyIK::Apply` before presentation IK,
ownership guards and head hiding. The snapshot is stored in entity space and
reconstructed against the current entity transform, avoiding a simulation-step
lag on moving carriages. CapturePad can request an immediate native matrix refresh
before PreRender. If a hidden body never reaches PreRender, the post-physics
request refreshes again so an early animation sample cannot survive simulation.
When ragdoll owns the player, that fallback reapplies its pose before sampling.
The ordinary render lookup never overwrites a posed/hidden presentation skeleton.

The final camera branch replaces the legacy eye nudges, calibrated seat offsets,
train marker offsets/boarding turn, and interpolated crouch/knockdown adjustments.
The subsequent vehicle-basis branch cannot overwrite the bone submission. The
bone snapshot has no smoothing, fixed standing height or lateral/vertical offset.

## Validation

| Check | Result |
|---|---|
| Native head snapshot + actual final camera submission | 18,732 checks passed across eight rates; pitch/roll, carrier motion, IK/head hiding isolation, immediate refresh, Classic/third-person bypass and invalid data |
| Held-weapon frame | 142,976 production checks passed |
| Body visibility | 1,536 production context/toggle checks passed |
| Seat/train/XR/backend | 2,074 production checks passed |
| Manager | 79/79 checks passed |
| Complete ARM64 debug APK | Built and updated personal Quest 3 device |
| Startup smoke | Launched and remained running; no recent fatal reports in filtered logs |
| Standalone Windows Manager | Published to `out/manager-windows-head-attachment` with the new regression adapter |

Headset visual acceptance remains pending. Recheck train boarding, waiting and
standing travel first, followed by car entry/exit, knockdown/get-up, ordinary
locomotion, crouching and driving. Original Xbox intentionally bypasses legacy
seat offsets; Classic preserves them.
