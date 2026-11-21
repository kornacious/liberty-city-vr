# Complete PC and Original Xbox variants

See the [implementation validation record](ASSET_VARIANTS_VALIDATION.md) for the
automated checks, confirmed Quest menus/opening scene and remaining acceptance scope.

The shared Quest, Windows PCVR and Linux PCVR engine selects one complete game variant before
loading assets. Installations live in `variants/pc` and `variants/xbox` below the
game-data directory. An Xbox installation does not require a PC installation and
never borrows missing files from it. `ModelSet=0` means PC; the persisted value
`ModelSet=1` means Original Xbox. The old internal enum name is retained for
settings compatibility; new installations do not use `modelsets/modern`.

In **Build and install**, select the target and enable **Install PC variant**,
**Install Original Xbox variant**, or both. Each checkbox enables its folder or
ISO field. PC requires a complete local PC installation; Xbox requires the user's
local GTA III **Original Xbox US 1.0** `.iso`/`.xiso.iso`. The application builds
once for the target and each selected variant is prepared independently. Neither
checkbox means an application-only update. **Build only** skips data preparation
and deployment. Original game files and conversion products remain local.

**Appearance > Asset Source** selects PC or Original Xbox and requires a full
restart. The sole complete source is selected automatically. When both exist, the
saved selection wins; when a requested source becomes incomplete, the remaining
complete source is selected. An unavailable source is identified as **PC/Xbox base
not installed**. If neither source is usable, an asset-independent startup notice
explains how to install one before game rendering begins.

Settings, saves, custom music, skins and mod storage remain outside variant
content. Existing legacy PC data is recognized as a separate PC source until
reimported. Old visual/audio Xbox overlays are preserved but are not complete
variants: reimport the ISO through Build and install. No application uninstall,
save deletion or original-data replacement is part of this migration.

### Quest 3 texture comparison

In the Manager's Quest 3 **Build and install** page, enable **Cache ASTC textures
for Quest 3** and select each variant you want to convert. The Manager can
download a checksum-verified [Arm astcenc](https://github.com/ARM-software/astc-encoder/releases)
on x64 with tool downloads enabled. You can also supply a local encoder on
PATH or in Manager Settings. The Manager
encodes supported D3D8 textures from loose TXDs and IMG archives to ASTC 6×6 LDR,
including their authored mip levels, and installs a checksummed cache alongside
each selected variant. Encoding runs locally and adds time and storage to
installation.

On the headset, open **Appearance > Texture Format**, choose **Original** or
**ASTC 6×6**, and fully restart. The page shows the active mode. Original uses the
existing texture loader, which can upload DXT as BC on supporting devices. ASTC
uploads the cached levels directly. Textures with no matching entry, such as
changed mod textures, use the original loader. ASTC also requires support for
the Vulkan format on the active device.

Quest performance captures record the selected active path as `texture_format`.
For a format comparison, disable generated mipmaps before both runs and keep
the asset source and workload the same. ASTC uses the authored mip chain from
the cache. The original path can use runtime-generated mips or, on devices
without BC support, load some textures with only the base level.

## Preparation and integrity

The importer reads full Xbox DVD and trimmed XISO layouts directly through the
read-only XDVDFS reader. It fingerprints the supported executable and all consumed
retail inputs against `tools/modelsets/xbox-disc.json`; the executable is never
installed or run. Unsupported or modified revisions fail before conversion. It
never downloads a game image or replacement asset pack.

Conversion requires FFmpeg/ffprobe, CMake 3.22+, Ninja and a C++17 compiler. The
bundled librw and MIT-licensed lzokay decoder are built locally. Xbox LZO wrappers
are decompressed, native geometry becomes portable RenderWare geometry, and native
textures become D3D8 TXDs. Dimensions, mip levels, skinning and supported materials
are retained. Output models and dictionaries are reopened for structural checks.
Claude's `playa5` texture is exported to `models/generic/player.bmp`.

Conversion also corrects the vertically reversed gameplay-head UVs for Maria,
Curly Bob and Catalina in the US 1.0 assets. Their texture pixels are preserved;
the correction applies only to the detached gameplay heads, leaving bodies,
hands and cutscene models unchanged. Existing Xbox installations need a fresh
ISO import with **Install Original Xbox variant** enabled to receive this fix;
an application-only update cannot change previously converted models. Local mesh
inspection and converter checks cover the correction. On 2026-11-16, the user
confirmed in-game VR rendering of Maria and Curly Bob after rebuilding and
reimporting the Xbox ISO assets. Catalina's in-game confirmation remains pending.

The complete Xbox variant includes the disc mission script, gameplay/path data,
vehicle and world collision, standard animations and cutscene archives, text,
converted models/textures, particles, Neo resources and converted audio. The
`rwfilemaker.rws` tool marker and unused console executable/UI runtime are omitted.
See the [dependency audit](XBOX_STANDALONE_DEPENDENCY_AUDIT.md) for the evidence and
scope of compatibility.

Our custom settings layout remains in use. Missing legacy frontend icons have
port-owned generated replacements. The Xbox radio strip uses short station
labels and highlights the selected station. The PC radio strip uses station
logos. The port's settings backgrounds and controller resources accompany each
variant. Required port menu keys are merged into each supported Xbox GXT without
replacing existing Xbox strings. Unsupported extra locales are not imported as
playable translations.
This does not reproduce the original Xbox animated frontend.

Preparation uses unique local directories under `<workplace>/prepared-variants`.
Each variant has a full SHA256 inventory, plus `VARIANT_INFO.txt` with its identity
and file sizes. Sources and staged copies are checked before activation; Quest
uploads are also checked remotely. Activation replaces only a marked variant and
retains previous versions in sibling backups. Startup checks the complete runtime
inventory for missing/size-changed files. Quest activation is performed by the
app-owned storage bridge, since scoped storage can reject shell chmod/rename.
Reinstall a selected variant to repair it; same-size corruption is detected by manager checksum validation, not startup's
size check. The other variant and shared user state are preserved.

## Scope and acceptance

### Audio comparison

The entire Xbox audio inventory is prepared and installed with its complete
variant. No PC audio fallback is needed or selected for an Xbox installation.

The Manager fingerprints all 197 retail audio files. It decodes Xbox ADPCM sound
effects to mono PCM16 and rebuilds the SDT offsets, lengths and loop points while
preserving all 3,032 sample IDs and authored playback rates. Xbox speech/ambience
and WMA radio are decoded to lossless FLAC at their original rates and channel
counts. This adds no lossy transcoding or upsampling. Every stream is checked for
matching rate, channels and WAV frame count; decoded PCM SHA256 values must match
between every source and FLAC output before the
staged collection can replace an existing output. Generated audio is included in
the checksummed installation manifest and atomic installation/rollback workflow.

Xbox FLAC streams are used only when Xbox is active. PC uses its own complete installation,
including any previously converted PC radio. Quest plays FLAC streams through Android's
media framework; their exact durations are read from STREAMINFO at startup.

To compare, select PC, fully restart, and listen to a repeatable scene or
station. Select Original Xbox and fully restart before repeating it at the same
volume. This changes visuals as well as audio. The import preserves the decoded
retail sources; it does not assert that every Xbox sound is better. All nine Xbox
stations are 44.1 kHz, versus 32 kHz for eight PC stations and 44.1 kHz for Game
Radio. Of 3,032 indexed effects, 3,031 have matching rates and the Porsche horn
changes from 11,025 to 32,000 Hz. Encoding, recordings and mastering must be judged
separately from sample rate.

### Visual and animation scope

The updated importer adds the Xbox particle settings and Neo resources to the
shared Vulkan renderer. Reimport with the current Manager to upgrade an existing
visual/audio collection; base PC files and saves are preserved.

Particle rows are matched by name and emitted in stable engine-ID order. Xbox's
extra `CARFLAME_MOVING` type is appended rather than inserted into the PC enum;
moving burning cars select it, while stationary fires retain `CARFLAME`. The
existing 68 IDs, including those used by saved particle objects, remain stable.

**Appearance > XBOX NEO MATERIALS** is on by default with Original Xbox and can
be switched live. It enables authored second-UV world lightmaps, skinned-character
rim lighting, the 13 road-gloss masks, and vehicle diffuse/specular/Fresnel tuning.
Hour and weather blending use the three retail tuning tables. The Manager
fingerprints the raw inputs, converts the DDS effect textures to a portable TXD,
and writes a bounded binary tuning table. PC never enables these effects.
Physical scopes capture and replay each draw's lightmap/gloss descriptors too.

**XBOX RAIN DROPLETS** is a separate opt-in, off by default. This uses the retail
mask on a shared head-relative visor plane, unprojected independently for each
eye. It adds bounded rain-driven distortion during immersive gameplay, excludes
no-rain camera zones, and leaves temporal history undistorted. It is a VR
adaptation rather than a reproduction of every original camera-lens splash and
droplet simulation rule.

Vehicle highlights use authored material specularity. Reflective surroundings
continue to come from the existing **Lighting > Car Reflections** controls;
this does not introduce the original console's separate environment-camera scene
render. The Neo mask and Fresnel blend are applied to that existing reflection
source. This avoids layering two competing reflection implementations. Precise
console pixel parity and performance acceptance require headset comparison.

Effects require both the updated game build and the newly prepared asset collection.
Neo controls apply live; Asset Source changes still require a full restart.

Xbox missions, gameplay data, collision and standard animations come from the ISO.
The inspected standard animation archives and collision files match the local PC
specimen byte for byte; the script has the same structure with one integer-value
difference. Xbox ANM hand animation and console cutscene-hand XML are outside the
supported runtime. Port-generated saves retain the engine's existing format;
importing original Xbox console saves is not supported.
Existing Xbox calibration offsets are selected as a starting point and need visual
verification against the retail models in VR.

The first-person camera recognizes Xbox Claude through his posed skin hierarchy,
including its head and torso bones. PC retains its rigid-frame eligibility
rules. Original Xbox first-person gameplay derives camera position from the
native head bone, including train boarding, standing passengers, vehicle
transitions, crouching and knockdowns. The pose is captured before VR IK/head
hiding and carried with the current entity transform. Routine animated head
rotations never steer the view: the locomotion/carrier frame and tracked headset
determine orientation, keeping the on-foot horizon level. WALKING HEAD BOB enables live
positional animation; when disabled, routine idle/walk/run
poses retain a stable bone-derived local position. Standing train passengers use
the live native head position, since their body retains its native passenger
animation rather than the on-foot posture solver. Posture changes and vehicle
transitions still follow the live head immediately. Eye placement uses the Xbox
head/body fit settings described below, without legacy seat/train offsets or
crouch/knockdown easing. Existing
seat calibration is bypassed in this mode; tracked headset movement and stereo
eyes remain relative to the bone anchor. PC and third-person retain their
existing camera paths.

For Original Xbox train passengers, the camera follows the authored boarding
turn with a level horizon. Boarding completion continues directly into the
carriage-relative outward heading without another turn or physical yaw recenter.
The Xbox camera override uses that window heading instead of the passenger
entity's forward axis. Eye fit is applied along
the outward basis at the live head; a frozen first-boarded pose cannot leave the
camera behind the standing body. Physical looking remains additive while riding,
and the existing passenger position lock remains active. During train exit, the
camera retains the occupied carriage side's outward heading and head position
lock until the exit state ends, including after vehicle occupancy clears. The
ordinary on-foot camera then resumes, with Claude and its gameplay heading
initialized to the same outward direction after native exit placement. During
boarding and alighting, connected native animated hands replace the tracked VR
hands; tracking resumes when the animation state ends. PC train behavior
is unchanged.

Head hiding, body IK, tracked weapons and holsters share the resulting view
anchor. Vehicle-transition fallbacks never read skinned animation keys as rigid
frames. These runtime fixes need only a game build update; an already prepared Original
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
knockdowns retain their existing handling. PC retains its existing solver.
The body uses the camera's pre-eye-offset anatomical head target, then adds live
tracked displacement. Spine reach is resolved by moving the pelvis rather than
pulling the head short of that target. This also keeps backwards leaning from
leaving the torso ahead of the eyes; camera fit remains independent of body IK.

While standing, foot contacts retain world positions and orientations on static
ground, with foot orientation following the ground normal. Physical roomscale
travel triggers alternating inferred steps: one foot swings over roughly 220 ms
with up to 6.5 cm lift while the other supports the body. Joystick walking/running
continues using native gait. Airborne states, large turns, teleports, lost ground
and ownership gaps release contacts. Unreachable contacts release rather than
stretching a leg or moving the camera. Moving-platform planting is not implemented.
The body/step layer follows the existing Body IK toggle.

Original Xbox on-foot roomscale also moves the actual player entity before game
simulation, so AI targeting and collision sectors follow the physical movement.
A 10 cm head-lean allowance remains within the body capsule. Transfers use short
collision-checked substeps over static ground, preserve native velocity and update
animated bones. The tracking origin consumes accepted horizontal movement to
prevent double camera motion. Paused/disabled controls, vehicles, airborne and
incapacitated states do not transfer roomscale into gameplay movement. This root
synchronization also works with Body IK disabled; PC behavior is unchanged.

Button crouch is a separate fixed Xbox stance: toggle to lower the camera 45 cm
vertically from its standing reference, keeping its horizontal position. Physical
height changes are ignored until crouch is toggled off, with a fresh height
reference on release. Other input does not cancel the toggle. Physical crouching
outside this fixed stance continues using the existing tracked posture behavior.

VR Overlay > Locomotion > XBOX HEAD / BODY FIT provides live camera forward,
height and right offsets in 1 cm steps, body yaw in 1 degree steps, knockdown
movement lock and head-bone/level view. Initial eye fit is 6 cm forward, 8 cm up,
0 cm right from the camera anchor; these are tunable starting values for the
retail mesh. Positive body yaw follows the solver's existing yaw convention.
Changes automatically persist in `vr_settings.ini`. SAVE AS MY DEFAULT stores a
separate personal baseline; RESTORE MY DEFAULT recalls it, and RESET FACTORY FIT
preserves that baseline. Report the displayed values after fitting them in VR
if they should become future factory defaults. Recenter does not erase the fit.

Xbox knockdown and on-foot arrest/busted movement is head-locked by default through recovery: physical
translation cannot move the view away from the animated head. The lock releases
with a fresh physical reference, so movement made while down does not carry into
the standing view. Head rotation remains tracked. HEAD BONE view follows native
head orientation only while down/getting up; routine walking retains the level
comfort orientation. The existing Locomotion KNOCKDOWN VIEW row controls the same
Xbox option. Busted letterboxing alone does not select the director/theater camera;
formal cutscenes retain their existing ownership. PC retains its prior
knockdown settings and movement policy.

Host checks and complete compilation do not establish visual headset acceptance.
Verify all districts/LODs, shared textures, vegetation, wheels/windows, player and
pedestrian skinning, cutscenes, weapon grips/scopes, seating/steering alignment,
mission interactions and sustained frame time/memory behavior. Game assets and
generated products remain local and must never be committed or uploaded.
