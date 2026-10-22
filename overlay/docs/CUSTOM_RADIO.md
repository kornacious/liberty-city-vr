# Custom soundtracks and the adjustable radio board

Custom soundtracks use Android MediaPlayer to read compressed local files
incrementally. Songs are not loaded or decoded wholesale into game memory.
Music is independent of the APK and stays in app storage across APK updates.
Uninstalling the app removes its music, settings and game data; keep originals.

## Import music from a computer

Install the updated APK first. Enable Quest USB debugging, accept the headset's
ADB authorization prompt, and install Python 3, Android Platform Tools (`adb`)
and FFmpeg (`ffprobe`). Both tools must be on PATH, or pass their full paths
with `--adb` and `--ffprobe`.

Windows:

```bat
IMPORT_MUSIC.bat "C:\Music\Driving"
```

Linux/macOS with Python and platform tools available:

```sh
./IMPORT_MUSIC.sh "/path/to/music"
```

Pass multiple files or directories; directory scanning is flat, not recursive.
For multiple connected headsets, add `--serial YOUR_SERIAL`. The tool accepts
Quest 3/3S devices in the authorized `device` state and never installs an APK,
uninstalls the app, deletes songs, or transfers original game data.

Validate without accessing any headset:

```sh
./IMPORT_MUSIC.sh --dry-run "/path/to/music"
```

The importer validates audio metadata with ffprobe, gives files safe ASCII names
with a content-hash suffix, uploads to hidden staging files through a shell-only
content provider, verifies SHA-256 on the headset, and atomically finalizes them.
Existing identical imports are skipped; different contents are never overwritten.
Failed transfers are cleaned up where possible; interruption may leave a hidden
staging file that is replaced on retry and is never indexed as music.

Finished imports trigger an automatic background rescan. The radio board and
Settings > Radio and Custom Music also offer RESCAN and show accepted/rejected
counts. Invalid tracks are logged with a rejection reason in logcat. A failed
runtime decoder is skipped and quarantined until the next scan; retries are
bounded. An empty or entirely failed library hides the custom station.

Existing installer transfers into `gamedata/mp3` remain compatible. Directly
copied files must be complete before a rescan. Use the importer for validated,
checksum-verified transfers and filenames compatible with the current glyph font.

## Formats and limits

Recommended: AAC-LC in `.m4a` at 160–192 kb/s, or MP3 at 192 kb/s. Opus in Ogg
at 128–160 kb/s saves more space; validate playback and seeking on the headset.
Do not transcode existing lossy songs just to standardize a library.

Importer-supported codecs: MP3, AAC, Vorbis, Opus, FLAC, and 16-bit PCM WAV.
Extensions: `.mp3`, `.m4a`, `.ogg`, `.opus`, `.flac`, `.wav`. Use one mono or stereo
audio stream at 44.1 or 48 kHz. Cover art is allowed; moving video, DRM and other
codecs are not supported. Native Android decoder behavior still applies.

- At most **1,000 accepted tracks**.
- At most **512 MiB per file**.
- At most **24 days total playlist duration**, below the engine's signed
  millisecond boundary. Internal totals use wider arithmetic before narrowing.
- The importer retains at least **1 GiB free space** and checks existing library
  count and duration. Upload staging also consumes space.
- Compressed 192 kb/s music uses about **86 MB/hour** (decimal units).
- The native flat scan bounds enumeration at 10,000 candidate files, then
  sorts and indexes supported tracks up to the limits above. Avoid oversized
  directories. Cached durations are invalidated by file size/mtime changes.

The complete playlist is a virtual station timeline, with independent song
indices. It does not consume a station ID per song or create a player per song.
The current/last song and position are saved periodically and on station exit.
Playback is sequential and wraps at the end; shuffle, crossfade and guaranteed
gapless transitions are not included.

## Use the radio board

The board follows the vehicle interior in first-person car/boat views, including
DEFAULT and immersive driving. It is anchored to the model's authored driver
seat using the vehicle's axes; it does not follow the hands or steering angle.
It is hidden on foot, in third-person, aircraft, cutscenes and scripted views.
Police vehicles keep the engine's police-radio restrictions.

- **Tap X** to change station. With a visible board this is a short press
  released within 0.3 seconds; the ordinary press edge remains when hidden.
- **Hold X** for at least 0.3 seconds to interact with the board.
- Point either free controller at a button, then press its trigger. A highlight
  identifies the target; hovering never activates it. A trigger already held
  for driving must be released and pressed again to click.
- While interacting, driving inputs and firing are captured. Release X to
  resume driving. A controller holding a tracked weapon cannot click the board.
- Use STATION -/+, music volume -/+, PREVIOUS, PAUSE/PLAY, NEXT and RESCAN.
  Track controls only act on the custom station; station cycling includes OFF.
- PREVIOUS restarts a song after three seconds; another press near its beginning
  selects the preceding song. User pause survives an ordinary game pause/resume.

## Adjust and share placement

Open the existing VR settings menu and choose **Radio and Custom Music**.
Select GLOBAL or the current vehicle model. Adjust right/forward/up in 5 mm steps,
pitch/yaw/roll in one-degree steps, and uniform scale in 5% steps. Changes preview
on the board and are saved only with SAVE PLACEMENT. CANCEL, leaving the page,
or changing vehicle discards the draft. The board can be hidden independently.

Reset acts on the selected target and removes its personal ownership, restoring
inheritance. The whole-placement precedence is:

1. Personal vehicle-model placement.
2. Personal global placement.
3. Bundled vehicle-model placement.
4. Bundled global placement.

A personal global preference therefore overrides community model defaults;
personal model overrides still win. Reset a personal global placement to opt
back into community defaults. Calibration is per model ID, not per spawned car.
Offsets use a fixed model seat origin and remain independent of seat/view tuning.

Save a vehicle placement, then select **EXPORT THIS VEHICLE PLACEMENT**. This
exports the effective saved placement for the current vehicle, excluding drafts,
visibility preferences and control bindings. Download it to a new local file:

```sh
./IMPORT_MUSIC.sh --export-board "/path/to/new-infernus-placement.ini"
```

The export is a versioned INI with a `[Vehicle.<model id>]` section. Position is
millimetres, rotation is canonical integer degrees `[-180,179]`, and scale is
percent `[40,250]`. Share the file with the vehicle/model-set name and a description
of the tested seated view. For a PR, add its vehicle section to:

`overlay/android/app/src/main/assets/radio_board_defaults.ini`

Check readability, dashboard/road obstruction, clipping and seated reach before
promoting a placement. Personal settings live in `gamedata/radio_board.ini`;
bundled defaults are refreshed separately with APK updates. This first release
exports layouts; it does not expose an arbitrary settings-file upload endpoint.

## Verification

Host checks use generated fixtures and fake media/pose backends; no original
game files are needed:

```sh
python3 tools/tests/test-custom-music.py
python3 tools/tests/test-radio-input.py
python3 tools/tests/test-radio-driving.py
python3 tools/tests/test-import-music.py
c++ -std=c++17 -Ioverlay/src/audio -Ioverlay/src/vr \
  tools/tests/test-radio-music.cpp -o /tmp/test-radio-music
/tmp/test-radio-music
```

Before release, validate pointer alignment in both eyes, steering and focus
transitions, real codec seeking/completion, vehicle fit, headset sleep/resume,
and indexing/playback frame time on Quest hardware. Device deployment is skipped
in cloud workflows.
