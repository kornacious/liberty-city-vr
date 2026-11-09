# Classic and Original Xbox asset sources on Quest

**Appearance > Asset Source** offers **Classic** (the base PC installation) and **Original Xbox**.
Original Xbox replaces the previous Modern (Xbox Version HD) profile. Its internal
setting remains `ModelSet=1` and its storage folder remains `modelsets/modern` for
compatibility. All world, vegetation, vehicle, pedestrian and weapon assets switch
together, along with sound effects, speech, ambience and all nine radio stations.
Fully close and restart the application after changing the source. The active
audio source is displayed separately from the requested source so pending restart
changes are clear. Custom music and saves are independent of this choice.

While Body IK controls Claude's arms, his opening prison jumpsuit keeps its own
hand models with either asset source. Changing into his regular outfit restores
the VR hand meshes automatically; no setting change or restart is needed.

## Import from your own disc image

Open **Original Xbox assets** in the .NET 10 Manager. Select your local GTA III
**US 1.0 original Xbox** `.iso` or `.xiso.iso`, choose an output folder named
`modern` (or leave it blank), and select **Perform**. **Build only** prepares and
validates the local output without discovering or transferring to a headset.

The Manager never downloads a game image, game assets or a replacement mod pack.
An ISO is required for each preparation run. Old saved extracted-source or download
options are rejected; clear them in the action settings if upgrading from a legacy
request. The current page clears those obsolete options when opened.

Conversion requires FFmpeg/ffprobe, CMake 3.22+, Ninja and a C++17 compiler. Windows uses Visual
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

### Audio comparison

Reimport the ISO with the updated Manager to add audio to an existing Xbox
visual collection. Older visual-only collections still work, with Classic audio;
the menu identifies this and asks for reimport. No base PC audio is overwritten.
The entire audio inventory is checked at startup. An incomplete collection uses
Classic audio as a whole rather than mixing an Xbox bank with PC speech/radio.

The Manager fingerprints all 197 retail audio files. It decodes Xbox ADPCM sound
effects to mono PCM16 and rebuilds the SDT offsets, lengths and loop points while
preserving all 3,032 sample IDs and authored playback rates. Xbox speech/ambience
and WMA radio are decoded to lossless FLAC at their original rates and channel
counts. This adds no lossy transcoding or upsampling. Every stream is checked for
matching rate, channels and WAV frame count; decoded PCM SHA256 values must match
between every source and FLAC output before the
staged collection can replace an existing output. Generated audio is included in
the checksummed installation manifest and atomic installation/rollback workflow.

Xbox FLAC streams take precedence over the base PC WAV/MP3 and compressed radio
siblings only when Xbox is active. Classic uses the existing base installation,
including any previously converted PC radio. FLAC streams play through Android's
media framework; their exact durations are read from STREAMINFO at startup.

To compare, select Classic, fully restart, and listen to a repeatable scene or
station. Select Original Xbox and fully restart before repeating it at the same
volume. This changes visuals as well as audio. The import preserves the decoded
retail sources; it does not assert that every Xbox sound is better. All nine Xbox
stations are 44.1 kHz, versus 32 kHz for eight PC stations and 44.1 kHz for Game
Radio. Of 3,032 indexed effects, 3,031 have matching rates and the Porsche horn
changes from 11,025 to 32,000 Hz. Encoding, recordings and mastering must be judged
separately from sample rate.

### Visual and animation scope

The updated importer adds the Xbox particle settings and Neo resources to the
Quest Vulkan renderer. Reimport with the current Manager to upgrade an existing
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
and writes a bounded binary tuning table. Classic never enables these effects.
Physical scopes capture and replay each draw's lightmap/gloss descriptors too.

**XBOX RAIN DROPLETS** is a separate opt-in, off by default. This uses the retail
mask on a shared head-relative visor plane, unprojected independently for each
eye. It adds bounded rain-driven distortion during immersive gameplay, excludes
no-rain camera zones, and leaves temporal history undistorted. It is a VR
adaptation rather than a reproduction of every original camera-lens splash and
droplet simulation rule.

Vehicle highlights use authored material specularity. Reflective surroundings
continue to come from the existing Quest **Lighting > Car Reflections** controls;
this does not introduce the original console's separate environment-camera scene
render. The Neo mask and Fresnel blend are applied to that existing reflection
source. This avoids layering two competing reflection implementations. Precise
console pixel parity and performance acceptance require headset comparison.

Effects require both the updated APK and the newly prepared asset collection.
Neo controls apply live; Asset Source changes still require a full restart.

PC mission scripts, handling/gameplay data, save formats, cutscene animation
archives and the Quest frontend remain on the existing foundation. Xbox ANM
cutscene-hand extensions and the Xbox console frontend are not imported. Original
Xbox actor models are included, but exact console cutscene presentation is outside
this importer. PC vehicle collision remains active, including train/boat records.
The inspected retail `anim/ped.ifp`, `anim/cuts.img` and `anim/cuts.dir` are
byte-identical between Xbox and PC; copying them would not change animations.
Xbox's extra cutscene hand models/ANM/XML remain outside the supported runtime.
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
positional animation; when disabled, routine idle/walk/run
poses retain a stable bone-derived local position. Standing train passengers use
the live native head position, since their body retains its native passenger
animation rather than the on-foot posture solver. Posture changes and vehicle
transitions still follow the live head immediately. Eye placement uses the Xbox
head/body fit settings described below, without legacy seat/train offsets or
crouch/knockdown easing. Existing
seat calibration is bypassed in this mode; tracked headset movement and stereo
eyes remain relative to the bone anchor. Classic and third-person retain their
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
hands; tracking resumes when the animation state ends. Classic train behavior
is unchanged.

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
synchronization also works with Body IK disabled; Classic behavior is unchanged.

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
formal cutscenes retain their existing ownership. Classic retains its prior
knockdown settings and movement policy.

Host checks and complete compilation do not establish visual headset acceptance.
Verify all districts/LODs, shared textures, vegetation, wheels/windows, player and
pedestrian skinning, cutscenes, weapon grips/scopes, seating/steering alignment,
mission interactions and sustained frame time/memory behavior. Game assets and
generated products remain local and must never be committed or uploaded.
