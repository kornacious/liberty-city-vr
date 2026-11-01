> Tool migration: standalone `.ps1`, `.sh` and `.py` entrypoints are now embedded
> manager modules. Open **Tools and tests**, or run `dotnet run --project
> tools/manager -c Release -- --tool MODULE -- ARGUMENTS` from the repository root.
> Legacy filenames below identify resource modules; old direct shell invocations
> are historical. Use the manager pages for build/install, editor build, music,
> models, repair and update. See [the manager guide](../../tools/manager/README.md) for runtime requirements.
# Custom soundtracks and the adjustable radio board

Custom soundtracks use Android MediaPlayer to read compressed local files
incrementally. Songs are not loaded or decoded wholesale into game memory.
Music is independent of the APK and stays in app storage across APK updates.
Uninstalling the app removes its music, settings and game data; keep originals.

## Manage music from a computer

Open **Manage music** in the Windows/Linux manager with the updated APK installed
and an authorized Quest 3 connected. The page loads its current library. **Refresh**
reloads the list. Select a row to inspect filename, artist, album, title, track
number, genre, year, size, duration, format, sample rate, channels and bitrate.
Missing metadata fields remain empty. Captions use **Artist - Album - Title**,
using the original filename when a title tag is missing.

Click a row to select only that track. **Shift-click** another row selects both
endpoints and every track between them; **Shift+Up/Down** extends the range by
keyboard. Selected rows are highlighted and marked with a dot. Drag a selected
row to move the whole selection together, preserving its internal order while
the intervening rows shift. **Up/Down** and **Alt+Up/Alt+Down** also move the group.

**Delete** queues the selected files for removal from the headset. The list and
pending deletion count update immediately; **Perform** actually deletes them,
while **Cancel** discards pending deletion and order changes. Delete clears the
optional import field so a remembered folder cannot immediately re-import the
removed tracks. Validation only never applies deletions. The provider checks the
library revision and exact kept/deleted file set before writing; staging or
playlist-write failures restore the staged files. Keep source copies of music
you may want to import again.

**Perform**
saves the complete order atomically on the headset, while **Cancel** discards
pending edits. A library changed since Refresh must be refreshed before saving.
New imports append after the explicitly ordered tracks, and deleted files are
ignored. The custom station plays this order sequentially and wraps at the end;
a rescan retains the currently playing file and its position.

The dashboard uses the same metadata caption. Long captions scroll left to right
through the clipped title area at 40 pixels/second, pause three seconds after
passing through, then repeat. Short titles stay centered. The existing dashboard
font transliterates common Latin accents; unsupported characters display `?`.
The terminal library preserves Unicode metadata. Earlier imports whose original
filename was not recorded can recover it by re-importing the same source file;
the audio is recognized as a duplicate and is not uploaded again.

Use the optional **Import** field and its integrated browser for a supported audio
file or flat folder. **Validate only** checks local files without writing music
or playlist order. ADB and ffprobe paths are configured in **Settings**.

### Historical importer reference

Install the updated APK first. Enable Quest USB debugging, accept the headset's
ADB authorization prompt, and install Python 3, Android Platform Tools (`adb`)
and FFmpeg (`ffprobe`). The importer finds ADB on PATH, in the SDK selected by
`ANDROID_HOME`/`ANDROID_SDK_ROOT`, or (on Windows) in the standard Android SDK
location and `C:\LCVRBuild\.android-sdk` used by the build script. FFprobe must
be on PATH. You can override either tool with `--adb` and `--ffprobe`.

Windows:

```bat
IMPORT_MUSIC.bat "C:\Music\Driving"
```

If ADB is installed elsewhere, supply its full path:

```bat
IMPORT_MUSIC.bat --adb "C:\My SDK\platform-tools\adb.exe" "C:\Music\Driving"
```

An error after all `VALID` lines saying `[WinError 2]` in older importers means
ADB could not be launched. Update the importer or use `--adb` as above.

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
Uploads use `adb push` to a unique temporary file, then device-local input to
the provider, avoiding binary corruption through streamed ADB shell stdin on
Windows. The temporary copy is removed after each upload; the free-space check
also reserves room for this copy (up to the largest selected track).
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
- In **VR Settings > Radio and Custom Music > Interaction**, choose
  **LASER POINTER** (default) or **TOUCH**. This preference is saved immediately
  and applies to all vehicle models.
- In laser mode, **hold X** for at least 0.3 seconds to interact with the board.
  Point either free controller at a button, then press its trigger. A highlight
  identifies the target; hovering never activates it. A trigger already held
  for driving must be released and pressed again to click.
- In touch mode, reach toward a button with either virtual index finger and
  touch its face. X and the trigger are not required. Withdraw the finger at
  least 3 cm to press again; holding contact or sliding does not repeat a press.
  Touch follows the animated finger, so release the trigger to extend it.
  A hand holding a weapon or gripping the wheel/handlebars cannot touch buttons.
- While interacting, driving inputs and firing are captured. Release X in
  laser mode, or move your fingers away from the buttons in touch mode, to
  resume driving. Keep virtual hands enabled for touch interaction.
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
