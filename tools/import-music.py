#!/usr/bin/env python3
"""Local-only Quest music transfer. Requires Python 3, ADB and ffprobe.

No transcoding, deletion, overwrite or game-data transfer. Uses the release
APK's restricted provider, atomic commits and SHA-256 transfer verification.
"""
import argparse
import hashlib
import json
import pathlib
import re
import shutil
import subprocess
import sys
import unicodedata

CODECS = {'.mp3': {'mp3'}, '.m4a': {'aac'}, '.ogg': {'vorbis', 'opus'}, '.opus': {'opus'}, '.flac': {'flac'}, '.wav': {'pcm_s16le'}}
EXTENSIONS = set(CODECS)
MAX_BYTES = 512 * 1024 * 1024
MAX_DURATION = 24 * 24 * 60 * 60 * 1000
RESERVE = 1024 * 1024 * 1024
URI = 'content://com.libertyvr.quest.music'


def run(args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(65536), b''):
            h.update(block)
    return h.hexdigest()


def safe_name(path, sha):
    stem = unicodedata.normalize('NFKD', path.stem).encode('ascii', 'ignore').decode()
    stem = re.sub('[^A-Za-z0-9_.-]+', '-', stem).strip('._-')[:70] or 'track'
    return f'{stem}-{sha[:16]}{path.suffix.lower()}'


def probe(path, ffprobe):
    size = path.stat().st_size
    if size <= 0 or size > MAX_BYTES:
        raise ValueError('File must be between 1 byte and 512 MiB')
    proc = run([ffprobe, '-v', 'error', '-show_format', '-show_streams', '-of', 'json', str(path)], capture_output=True, text=True)
    data = json.loads(proc.stdout)
    streams = data.get('streams', [])
    audio = [s for s in streams if s.get('codec_type') == 'audio']
    # Embedded cover art is fine; moving video and ambiguous audio tracks are not.
    video = [s for s in streams if s.get('codec_type') == 'video' and not s.get('disposition', {}).get('attached_pic')]
    if len(audio) != 1 or video:
        raise ValueError('Expected one audio stream and no moving video')
    a = audio[0]
    if a.get('codec_name') not in CODECS.get(path.suffix.lower(), set()):
        raise ValueError('Unsupported codec or codec/extension combination')
    if a.get('channels') not in (1, 2) or int(a.get('sample_rate', 0)) not in (44100, 48000):
        raise ValueError('Use mono/stereo at 44.1 or 48 kHz')
    duration = int(float(data.get('format', {}).get('duration', a.get('duration', 0))) * 1000)
    if not 0 < duration <= MAX_DURATION:
        raise ValueError('Invalid or excessive duration')
    return size, duration


def provider(adb, method, arg=None):
    args = adb + ['shell', 'content', 'call', '--uri', URI, '--method', method]
    if arg is not None:
        args += ['--arg', arg]
    output = run(args, capture_output=True, text=True).stdout
    start = output.find('{"')
    if start < 0:
        raise RuntimeError('Music provider unavailable. Install the updated APK. ' + output.strip())
    result, _ = json.JSONDecoder().raw_decode(output[start:])
    if not result.get('ok'):
        raise RuntimeError(result.get('error', output))
    return result


def choose_device(adb_path, requested):
    output = run([adb_path, 'devices', '-l'], capture_output=True, text=True).stdout
    devices = [line.split()[0] for line in output.splitlines()[1:] if re.match(r'^\S+\s+device(?:\s|$)', line)]
    if requested:
        if requested not in devices:
            raise RuntimeError('Requested device is not authorized and connected')
        serial = requested
    elif len(devices) == 1:
        serial = devices[0]
    else:
        raise RuntimeError('Connect one authorized Quest or specify --serial')
    adb = [adb_path, '-s', serial]
    model = run(adb + ['shell', 'getprop', 'ro.product.model'], capture_output=True, text=True).stdout.strip()
    if not re.search(r'Quest (3|3S)\b', model, re.I):
        raise RuntimeError(f'Expected Quest 3/3S; found {model!r}')
    print(f'Using {model} ({serial})')
    return adb


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('files', nargs='*', type=pathlib.Path, help='Files or flat music directories')
    parser.add_argument('--serial')
    parser.add_argument('--adb', default='adb')
    parser.add_argument('--ffprobe', default='ffprobe')
    parser.add_argument('--export-board', type=pathlib.Path, help='Download the placement exported in the Radio settings page')
    parser.add_argument('--dry-run', action='store_true', help='Validate locally; no ADB or headset required')
    args = parser.parse_args(argv)
    paths = []
    for item in args.files:
        if item.is_dir():
            paths.extend(p.resolve() for p in sorted(item.iterdir()) if p.is_file() and p.suffix.lower() in EXTENSIONS)
        else:
            paths.append(item.resolve())
    if not paths and not args.export_board:
        parser.error('Select audio files/a directory or --export-board')
    accepted, rejected, seen = [], 0, set()
    if paths and not shutil.which(args.ffprobe):
        raise RuntimeError('ffprobe is required for validation. Install FFmpeg or specify --ffprobe')
    for path in paths:
        try:
            if not path.is_file() or path.suffix.lower() not in EXTENSIONS:
                raise ValueError('Missing file or unsupported extension')
            size, duration = probe(path, args.ffprobe)
            sha = digest(path)
            if sha in seen:
                print(f'SKIP duplicate: {path.name}'); continue
            seen.add(sha)
            accepted.append((path, size, duration, sha, safe_name(path, sha)))
            print(f'VALID {path.name}: {size / 1e6:.1f} MB, {duration / 60000:.1f} min')
        except (ValueError, OSError, subprocess.CalledProcessError) as error:
            rejected += 1; print(f'REJECT {path.name}: {error}', file=sys.stderr)
    if len(accepted) > 1000 or sum(t[2] for t in accepted) > MAX_DURATION:
        raise RuntimeError('Selection exceeds 1000 tracks or 24 days total duration')
    if args.dry_run:
        print(f'Dry run: {len(accepted)} valid, {rejected} rejected. No headset accessed.')
        return 1 if rejected else 0
    if paths and not accepted:
        raise RuntimeError('No valid music to import')
    adb = choose_device(args.adb, args.serial)
    if args.export_board:
        if args.export_board.exists():
            raise RuntimeError('Export destination already exists; choose a new filename')
        data = run(adb + ['exec-out', 'content', 'read', '--uri', URI + '/board-export'], capture_output=True).stdout
        if b'[RadioBoard]' not in data or b'[Vehicle.' not in data or len(data) > 65536:
            raise RuntimeError('Export unavailable. Export a saved vehicle placement from the in-game Radio settings first.')
        args.export_board.write_bytes(data)
        print(f'Exported placement: {args.export_board}')
    if not accepted:
        return 0
    info = provider(adb, 'info')
    duplicates = 0
    new = []
    remote_names = set(info.get('names', []))
    for item in accepted:
        path, size, duration, sha, name = item
        if name in remote_names and provider(adb, 'check', name + ':' + sha).get('exists'):
            duplicates += 1
            print(f'ALREADY PRESENT {path.name}')
        else:
            new.append(item)
    accepted = new
    if info['count'] + len(accepted) > 1000:
        raise RuntimeError('Existing library plus selection exceeds 1000 files')
    if info.get('duration', 0) + sum(t[2] for t in accepted) > MAX_DURATION:
        raise RuntimeError('Existing library plus selection exceeds 24 days total duration')
    if info['free'] - sum(t[1] for t in accepted) < RESERVE:
        raise RuntimeError('Import would leave less than 1 GiB free')
    imported = 0
    for path, size, duration, sha, name in accepted:
        try:
            with path.open('rb') as source:
                run(adb + ['shell', 'content', 'write', '--uri', URI + '/upload/' + name], stdin=source, capture_output=True)
            result = provider(adb, 'commit', name + ':' + sha)
            if result.get('bytes') != size:
                raise RuntimeError('Verified file size does not match local source')
            if result.get('duplicate'):
                duplicates += 1; print(f'ALREADY PRESENT {path.name}')
            else:
                imported += 1; print(f'IMPORTED {path.name}')
        except Exception:
            try:
                provider(adb, 'abort', name)
            except Exception:
                pass
            raise
    print(f'{imported} imported, {duplicates} already present, {rejected} rejected. The game rescans automatically; RESCAN is also available on the radio board.')
    print('See Radio settings for accepted/rejected counts and calibration controls.')
    return 1 if rejected else 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        print(f'Import failed: {error}', file=sys.stderr)
        sys.exit(1)
