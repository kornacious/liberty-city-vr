# Original Xbox standalone installation dependency audit

This records the pre-implementation audit. The subsequent complete-variant
implementation and current installation procedure are described in the
[asset guide](QUEST_MODELSETS.md); validation limits still apply.

Date: 2026-11-13. Scope: the locally supplied GTA III Original Xbox US 1.0 disc,
Quest Vulkan and experimental Windows PCVR. Repository baseline:
`6682c091829fbf1a903994d213ea999e8eeb5f34`.

## Finding

The audited disc supplies the gameplay content needed for an Xbox-derived base
installation. No indispensable PC-exclusive gameplay asset was found. The current
installer and runtime nevertheless require a PC foundation; Xbox-only installation
is **not implemented or validated as playable**.

The remaining work is principally complete data preparation, frontend resources
and text additions, and removing the runtime's unconditional Classic fallback.
The evidence does **not** justify implementing a different mission interpreter,
vehicle collision format, or animation loader for this disc.

This is a complete disc inventory and static dependency audit, not a playthrough
or a claim of console presentation parity. Runtime acceptance remains necessary.
Other Xbox regions, revisions, modified images and original Xbox saves are outside
this finding.

## Inputs and method

- Execution was local on Windows. The ISO was opened read-only. No original data
  was downloaded, uploaded, installed, or changed. No device was contacted.
- Enumerated and SHA256-hashed all **426 disc files** (768,652,096 bytes of file
  payload, excluding image padding). Every one of the existing importer's **310
  fingerprinted inputs** matched its expected size and SHA256; no mismatch.
- Compared disc files with the local PC installation. Its root contains port
  additions; it is a comparison specimen, not an independently authenticated
  pristine retail PC baseline. All byte-equality and byte-difference claims below
  refer to that specimen.
- Inspected the cached re3 source at HEAD
  `b050e1cc63b1869e6b514ab2f1a00ef4969e0c84`, current overlay code and relevant
  platform patches. Collected 520 literal file-path occurrences, then checked the
  level DAT references, IDE definitions and script-driven resource requests.
  Literal searches alone are not used as proof of dependency closure.
- Parsed both disc IMG directories with extent checks: **4,052** entries in
  `models/gta3.img`, **288** in `anim/cuts.img`.
- Validated the checksums of an existing local, current
  `ORIGINAL_XBOX_US_1_0` / `original-xbox-us-3-effects` prepared overlay: **295
  manifest payload files**, no missing or mismatched payload. This checks existing
  output integrity; the converter was not rerun and media playback was not tested.
- Built and ran a temporary .NET 10 audit utility using the manager's XDVDFS
  reader. Its code and generated metadata are local under the ignored
  `work/xbox-base-audit/` directory. No extracted retail payload is included in
  this report or added to version control.

## Dependency findings

| Family | Disc evidence | Standalone preparation required |
| --- | --- | --- |
| Level manifests | `data/default.dat` and `data/gta3.dat` are byte-identical to the PC specimen. All 56 file-bearing directives resolve on disc. | Copy both into the base; the overlay importer currently omits them from its output. |
| Map and model definitions | 3,084 distinct model IDs in the disc IDE files; every referenced texture dictionary is available. | Retain matching Xbox definitions and converted geometry together. |
| Model archive | 3,135 DFFs, 916 TXDs and one tool marker. All 4,052 entries have Xbox LZO wrappers. | Use existing decompression and RenderWare conversion, rebuild IMG/DIR and omit `rwfilemaker.rws`. Raw disc IMG cannot be used directly. |
| Loose geometry and textures | Required loose files are present, including fonts, HUD, particles and shared model clumps. | Existing native conversion; export `playa5` to `models/generic/player.bmp` as the importer already does. |
| Collision | All 22 COL files, including `models/coll/vehicles.col`, are byte-identical to the PC specimen. Vehicle file contains 59 records. | Copy collision into the complete base, including the vehicle file deliberately omitted by today's overlay importer. No Xbox-specific collision converter indicated. |
| Gameplay configuration | 19 root `data/` files and all 20 `data/paths/` files are byte-identical to the PC specimen. These include handling, object, ped, surface, timecycle, train, water and path data. | Copy the runtime inputs now supplied by the PC base. Do not require PC files merely because the overlay omits them. |
| Weapon configuration | Same 22-field row layout. Only one byte differs: flamethrower animation-loop start is 7 rather than 6, in the existing loader's frame units. | Preserve the Xbox value; the existing parser supports it. Gameplay acceptance still needed. |
| Particles and Neo effects | Existing importer already normalizes particle IDs and prepares portable Neo textures/tuning. | Reuse that preparation and its current rendering scope. No additional retail PC dependency found. |
| Mission script | Full static decoding succeeds; details below. | Copy the Xbox script unchanged into the base and extend source fingerprints. No script conversion indicated by this specimen. |
| Standard animations | `anim/ped.ifp`, `anim/cuts.img`, `anim/cuts.dir` and `anim/gta3.ini` are byte-identical to the PC specimen. | Copy from the ISO; the current importer leaves these to the PC foundation. |
| Sound effects, speech and radio | All 197 currently supported audio inputs match the importer fingerprints. | Existing ADPCM/SDT and FLAC conversion. Standalone selection must activate Xbox audio without silently depending on PC fallback audio. |
| Text | All seven disc GXT files have bounded TKEY/TDAT sections, valid aligned text offsets and terminated UTF-16 strings. English-family sizes fit the current 256 KiB buffer. | Use Xbox text as the content source and merge port menu labels. Handle locale availability explicitly. |
| Frontend textures | Disc has no `models/frontend.txd`, which the current frontend explicitly loads. The pinned source supplies `menu.txd` and controller additions, but no `frontend.txd`. | Provide a port-authored frontend dictionary/presenter, or map suitable Xbox UI resources into the current menu. This is a real resource gap for the existing frontend. |
| Intro movies | Disc has Bink `.bik` movies instead of PC `.mpg` movies. | Not required by the current shared Quest/Windows startup path, which enters frontend directly. Playback would need separate decoder/conversion work if introduced. |
| Settings, caches, saves, custom content | Not installation content that must be imported from a PC game. | Create port configuration and cache state locally; preserve user saves/music. Do not import console cache/settings blindly. |

### Mission compatibility

Both `main.scm` files are **606,553 bytes**. Their only difference is two adjacent
bytes within one `COMMAND_SET_VAR_INT` immediate: global-variable byte offset
16,108 receives **480,000** on Xbox versus **470,000** in the local PC specimen.
The assignment begins at file offset 567,044. This is a value difference, not a
different opcode or argument representation. Its gameplay meaning was not inferred
from the numeric value.

Static checks on the Xbox script:

- Main-script size 108,797 bytes; largest mission 25,299 bytes; **80 missions**.
  These fit the existing 128 KiB main / 32 KiB mission buffers and 120-mission cap.
- **186 object-table slots**, within the existing 200-slot limit. All 185 named
  object references resolve against Xbox IDE definitions.
- **79,047 instructions** decoded across the main executable section and every
  mission, with no unknown opcodes, invalid parameter encodings or boundary errors.
- **17,829 label operands** resolve to decoded instruction boundaries, accounting
  for mission-relative labels. Every used opcode has a handler case in the engine.
- All **78 distinct script-requested special model/outfit names** resolve to DFFs
  in the Xbox archive. All **79 distinct requested cutscene names** have IFPs in
  the disc cutscene archive.

The temporary decoder derives operand schemas from re3's `ScriptDebug.cpp` and
opcode numbers from `ScriptCommands.h`. It corrects one metadata omission:
`CREATE_PLAYER` stores an output parameter in its real handler, although the debug
schema omits it. Both scripts then decode fully with identical instruction layouts.
This is structural verification, not execution of every handler or mission.

No Xbox script-specific interpreter extension was identified. Retain the Xbox
script and its values; replacing it with a PC script is not necessary on this
evidence. Original console save-file compatibility is a separate problem and was
not tested. Cross-source saves created by this port still need regression tests.

### Geometry closure and apparent missing files

A naive one-DFF-per-IDE-name check initially finds 34 unmatched names. Of these,
**23 are named models inside the converted loose shared clumps** (weapons, wheels,
peds, low-detail player and train). The engine's `LoadModelFile` / `LoadClumpFile`
paths register these by frame name rather than filename.

The remaining 11 are the five `cutobj` slots, four `special` slots, `null` and
`special_thing_for_path`: dynamic/placeholder or path definitions, not eleven
missing standalone retail DFFs. Script-requested replacements resolve as checked
above.

The PC archive contains five names absent from the Xbox archive: `plaster.dff`,
`g.dff`, `stu_man.dff`, `stu_wom.dff` and `gtaelift.dff`. None is required by the
audited Xbox IDE definitions or the decoded special-model requests. Their absence
does not establish a PC dependency. Xbox has 201 archive names absent from the PC
specimen; complete preparation must retain the Xbox inventory rather than filter
it to PC filenames.

The generic COL-record walker encounters extra payload after eight records in
`peds.col`. That file is byte-identical to the PC specimen; the tail is not evidence
of a new Xbox format incompatibility. Preserve it rather than rewriting it based
on the generic walker's result.

### Text and frontend are the material resource gap

Xbox American GXT has **2,907 keys** versus **2,699** in the local PC/port American
GXT. The latter has **50 keys absent from Xbox**, all with the `FE` frontend prefix;
43 occur as candidate literal keys in the scanned engine source. Some are behind
optional build features. This is a port UI extension issue, not evidence that the
Xbox disc lacks mission dialogue. Merge required port-authored labels into Xbox
text rather than wholesale replacing it with PC text.

The disc's `japaneseunicode.gxt` does not match the engine's expected
`japanese.gxt` name and established font encoding. A valid container does not prove
Japanese glyph compatibility. Do not advertise that locale until its text/font
path is adapted and tested. Polish and Russian additions are not supplied by this
US disc either; locale choices must reflect actual installed support.

The Xbox Neo frontend DDS/ANM resources are a different presentation system. They
cannot simply be renamed to `frontend.txd`. Supporting the original console menu
is optional; a complete port-owned frontend can remove the PC menu-art dependency.
The current controller dictionaries and `menu.txd` can remain port resources.

## Current code assumptions that must change

1. **Manager base preparation** (`tools/manager/Workflows.cs`):
   `ValidateGameDirectory` requires a PC folder and `PrepareWindowsData` copies
   that folder. Add an ISO-derived complete-base preparation route for both
   targets. Keep generated installations outside retail sources and preserve saves.
2. **Importer coverage** (`OriginalXboxModels.cs`, `xbox-disc.json`): current output
   is intentionally an overlay. Fingerprint and copy the additional base runtime
   inputs, particularly scripts, gameplay/path tables, animations, text and vehicle
   collision. Stage and validate the entire installation before activation.
3. **Root archive handling** (`ModelSets.cpp`, streaming platform implementations):
   current code keeps the PC archive as image zero and registers Xbox as a second
   image. Ensure a complete Xbox installation supplies every root-level consumer,
   including PlayerSkin's direct root IMG/DIR access, without needing a PC archive.
4. **Availability and fallback** (`ModelSets.cpp`): `IsAvailable(CLASSIC)` currently
   returns true unconditionally. Availability must describe complete verified
   installations. An incomplete sole source should yield a repair message, not
   fall through into missing PC files or audio.
5. **Source identity**: Xbox skinning, calibration, audio, particles and Neo effects
   depend on the selected source identity, not just file format. Retain that identity
   when the converted archive becomes the base. Do not relabel it Classic.
6. **Frontend preparation**: supply frontend art/presentation and merge required
   port text additions. Preserve existing target-specific hands, controller assets
   and placement configuration in both preparation routes.
7. **Selection and coexistence**: track PC and Xbox completeness independently.
   Select the sole complete source on first installation; preserve an explicit
   selection when a second source is added. Switching must select a coherent
   content set, including its script/configuration, and require restart.

Xbox ANM hand animation, cutscene-hand XML, the alternative `cutscene.scm`, console
Neo menu resources, executable and console-specific auxiliary files have no
required consumer in the current gameplay path. They are not a prerequisite for
the port's existing presentation. Exact Xbox cutscene/frontend reproduction would
be a separate extension.

## Required acceptance after implementation

Use a new isolated installation prepared solely from the ISO and port resources,
with the PC source and all other game-data roots unavailable to the test process.
Trace file opens so any fallback outside that installation fails visibly.

Validate frontend/new game, opening sequence and first save/load; every district
and LOD; vehicles including train/boat collision; special characters and outfits;
cutscene actors, cameras and audio; speech, ambience and every radio station.
Exercise mission progression and later missions as well as the opening. Test
missing/corrupt installation inputs and interrupted preparation/activation.

Then add PC assets, switch in both directions, restart, and verify complete source
selection, saves, configuration, music and recovery behavior. Validate Windows
PCVR and Quest independently. Headset performance claims require Quest measurements.

No engine or installer implementation was changed in this audit. No APK/game build
or deployment was required or performed; no clean Xbox-only runtime was launched.
