# Original Xbox Claude first-person rig

Local Windows investigation and validation, 2026-11-06. Retail data stayed local.
No asset conversion or asset installation was required for this runtime fix.

## Cause and change

The user's headset images showed the camera outside Claude, with the tracked
equipment displaced from his body. The first-person camera required a non-null
`m_pFrames[PED_TORSO]` on foot. re3's skin-frame population starts at `PED_MID`,
leaving that rigid torso slot empty for the Xbox model. The converted retail
player has the complete expected 16-bone HAnim hierarchy, including head and torso.

Both the camera-inside-head policy and actual first-person anchoring now use a
shared representation-aware predicate. Skinned players require valid head/torso
indices and a matrix array; Classic uses its previous head/torso/vehicle rules.
This restores the anchor consumed by head hiding, body IK and tracked equipment
without substituting the Classic player model or changing Classic calibration.

The vehicle-transition fallback also avoids reading `AnimBlendFrameData::frame`
on a skin: that field aliases an HAnim animation key rather than an `RwFrame`.
Valid skinned transitions continue using their animated head pose.

## Validation

| Check | Result |
|---|---|
| Production player-rig fixture | 22 passed; all Classic eligibility combinations, retail-style skin without rigid slots, missing bones/matrices, and poisoned skin-frame alias |
| Production body visibility | 1,536 context/toggle checks passed |
| Production seat/train/XR/backend | 2,074 checks passed |
| Production held-weapon frame | 142,976 checks passed across nine rates, vehicle/on-foot and both skeleton paths |
| .NET Manager regressions | 79/79 passed |
| Complete ARM64 debug APK | Built successfully |
| Deployment | Updated personal Quest 3 device; existing asset profile, settings and saves preserved |
| Windows Manager | Published separately to `out/manager-windows-player-rig`; replacement of the existing default executable was denied by Windows |

The held-weapon fixture was missing the existing train-cinematic API. Its stub
was updated and coverage now checks that cinematic skeleton ownership stays native.
The Manager catalogue expectation includes the new test adapter.

Startup smoke: the installed app launched and remained running, with no recent
fatal reports in filtered logs. This does not establish visual acceptance.

Headset visual acceptance remains pending. Recheck Original Xbox on-foot eye
placement, hidden head, arms/hands, holsters, aiming/scopes, crouching and vehicle
entry/exit; repeat representative checks in Classic. No ISO reimport is needed.
