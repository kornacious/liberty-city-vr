# Classic and Original Xbox assets on Quest

The asset menu offers **Classic** (the base PC installation) and **Original Xbox**.
Original Xbox replaces the previous Modern (Xbox Version HD) profile. Its internal
setting remains `ModelSet=1` and its storage folder remains `modelsets/modern` for
compatibility. All world, vegetation, vehicle, pedestrian and weapon assets switch
together. Fully restart the application after changing the profile.

## Import from your own disc image

Open **Original Xbox assets** in the .NET 10 Manager. Select your local GTA III
**US 1.0 original Xbox** `.iso` or `.xiso.iso`, choose an output folder named
`modern` (or leave it blank), and select **Perform**. **Build only** prepares and
validates the local output without discovering or transferring to a headset.

The Manager never downloads a game image, game assets or a replacement mod pack.
An ISO is required for each preparation run. Old saved extracted-source or download
options are rejected; clear them in the action settings if upgrading from a legacy
request. The current page clears those obsolete options when opened.

Conversion requires CMake 3.22+, Ninja and a C++17 compiler. Windows uses Visual
Studio C++ Build Tools; Linux uses the installed C/C++ toolchain. The bundled
librw source and MIT-licensed lzokay decoder are compiled locally. Python and
extract-xiso are not required. The image is opened read-only; the built-in XDVDFS
reader supports a full Xbox DVD game partition and trimmed XISO layouts.

The importer verifies file sizes and SHA256 values against the supported retail
baseline in `tools/modelsets/xbox-disc.json`. It identifies the asset revision by
file content, so repacking the same files into an XISO does not invalidate it.
Other regions, revisions and modified images fail before conversion. The Xbox
executable is fingerprinted for identification and is never installed or executed.

## Preparation and installation

Default output: `<workplace>/original-xbox/modelsets/modern`. Generated temporary
source copies are local and cleaned after success, failure or cancellation. The
converter decompresses the archive's Xbox LZO wrappers, converts native geometry
into portable RenderWare geometry, and converts native textures to D3D8 TXDs.
It retains original texture dimensions and mip levels; DXT textures use librw's
compressed conversion path. There is no HD replacement, resolution cap, texture
recompression or HD-mod wheel repair. Node names, skinning, UVs and supported
RenderWare material effects are carried through librw's serializers. Every output
model and dictionary is reopened for structural validation before activation.

Claude's `playa5` texture is exported as `models/generic/player.bmp` for re3's
player-skin renderer. Matching Xbox `data/default.ide`, map IDE/IPL/COL files,
shared textures, loose models and archive entries accompany the profile.
The retail archive's `rwfilemaker.rws` tool marker is omitted.

A checksummed manifest covers the complete generated output. A staged replacement
is verified before it replaces an existing marked output. Unmarked folders are
refused. Installation verifies every uploaded file and activates the complete
folder with rollback on activation failure. Base PC data and saves are preserved.
Old HD overlays are preserved until a successful replacement; the new game build
falls back to Classic while only an old HD overlay is installed.

## Scope and acceptance

This imports the original Xbox visual collection for the existing Quest Vulkan
renderer. It does not enable desktop Neo world lightmaps, gloss, rim lighting or
Xbox reflection behavior. Material support still depends on the active renderer.

PC mission scripts, handling/gameplay data, audio, save formats, cutscene animation
archives and the Quest frontend remain on the existing foundation. Xbox ANM
cutscene-hand extensions and the Xbox console frontend are not imported. Original
Xbox actor models are included, but exact console cutscene presentation is outside
this importer. PC vehicle collision remains active, including train/boat records.
Existing Xbox calibration offsets are selected as a starting point and need visual
verification against the retail models in VR.

The first-person camera recognizes Xbox Claude through his posed skin hierarchy,
including its head and torso bones. Classic retains its rigid-frame eligibility
rules. Original Xbox first-person gameplay derives camera position from the
native head bone, including train boarding, standing passengers, vehicle
transitions, crouching and knockdowns. The pose is captured before VR IK/head
hiding and carried with the current entity transform. Routine animated head
rotations never steer the view: the locomotion/carrier frame and tracked headset
determine orientation, keeping the on-foot horizon level. WALKING HEAD BOB enables live
positional animation; when disabled, routine idle/walk/run and standing train
poses retain a stable bone-derived local position. Posture changes and vehicle
transitions still follow the live head immediately. Eye placement uses the Xbox
head/body fit settings described below, without legacy seat/train offsets or
crouch/knockdown easing. Existing
seat calibration is bypassed in this mode; tracked headset movement and stereo
eyes remain relative to the bone anchor. Classic and third-person retain their
existing camera paths.

Head hiding, body IK, tracked weapons and holsters share the resulting view
anchor. Vehicle-transition fallbacks never read skinned animation keys as rigid
frames. These runtime fixes need only an APK update; an already prepared Original
Xbox profile does not need rebuilding.

With on-foot Body IK enabled, Original Xbox Claude also uses a tracked posture
layer. Physical crouching lowers the pelvis and bends the knees while retaining
reachable foot contacts. Forward/sideways roomscale displacement
is shared between pelvis and spine, with shoulders following before the tracked
arm solve. Lean is limited to 28 cm; larger roomscale movement carries the body.
Extra physical crouch is limited by leg length and does not add lowering already
supplied by the game's crouch animation. Recenter while comfortably upright to
establish the physical reference. This layer does not change the camera, collision
capsule or game crouch rules. Vehicles, train passengers, jumps, transitions and
knockdowns retain their existing handling. Classic retains its existing solver.
The body uses the camera's pre-eye-offset anatomical head target, then adds live
tracked displacement. Spine reach is resolved by moving the pelvis rather than
pulling the head short of that target. This also keeps backwards leaning from
leaving the torso ahead of the eyes; camera fit remains independent of body IK.

While standing, foot contacts retain world positions and orientations on static
ground, with foot orientation following the ground normal. Walking/running,
airborne states, large turns, steps, teleports, lost ground and ownership gaps
release those contacts. Unreachable contacts release rather than stretching a
leg or moving the camera. The native gait continues during locomotion; this is
standing foot planting, not a replacement walking/stepping system. Moving-platform
planting is not implemented. The layer follows the existing Body IK toggle.

VR Overlay > Locomotion > XBOX HEAD / BODY FIT provides live camera forward,
height and right offsets in 1 cm steps, body yaw in 1 degree steps, knockdown
movement lock and head-bone/level view. Initial eye fit is 6 cm forward, 8 cm up,
0 cm right from the camera anchor; these are tunable starting values for the
retail mesh. Positive body yaw follows the solver's existing yaw convention.
Changes automatically persist in `vr_settings.ini`. SAVE AS MY DEFAULT stores a
separate personal baseline; RESTORE MY DEFAULT recalls it, and RESET FACTORY FIT
preserves that baseline. Report the displayed values after fitting them in VR
if they should become future factory defaults. Recenter does not erase the fit.

Xbox knockdown movement is head-locked by default through getting up: physical
translation cannot move the view away from the animated head. The lock releases
with a fresh physical reference, so movement made while down does not carry into
the standing view. Head rotation remains tracked. HEAD BONE view follows native
head orientation only while down/getting up; routine walking retains the level
comfort orientation. The existing Locomotion KNOCKDOWN VIEW row controls the same
Xbox option. Classic retains its prior knockdown settings and movement policy.

Host checks and complete compilation do not establish visual headset acceptance.
Verify all districts/LODs, shared textures, vegetation, wheels/windows, player and
pedestrian skinning, cutscenes, weapon grips/scopes, seating/steering alignment,
mission interactions and sustained frame time/memory behavior. Game assets and
generated products remain local and must never be committed or uploaded.
