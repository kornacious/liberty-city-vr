#!/usr/bin/env python3
"""Prepare the pinned GTA III Xbox Version HD re3 pack for Xbox or Modern."""
import argparse
import hashlib
import html
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import time
import urllib.parse
import urllib.request
import uuid
import zipfile

TOOLS = Path(__file__).resolve().parent
PACK = json.loads((TOOLS / 'xbox-pack.json').read_text())
BUILDER_VERSION = PACK['builderVersion']
NAME = re.compile(r'[a-z0-9_@]{1,19}\.(dff|txd)\Z')


def progress(message):
    print(message, flush=True)


def prompt(message):
    # PowerShell forwards native output one line at a time. A bare input()
    # prompt has no newline, so it can be hidden while we wait for the answer.
    progress(message)
    return input()


def report_item(index, total, phase, name):
    if index == 1 or index == total or index % 20 == 0:
        progress(f'{phase}: {index}/{total} - {name}')


def report_bytes(done, total, label, previous, final=False):
    now = time.monotonic()
    if final or now - previous >= 1:
        percent = min(100, done * 100 // total) if total else 0
        progress(f'{label}: {percent}% ({done // (1024 * 1024)}/{total // (1024 * 1024)} MiB)')
        return now
    return previous


def copy_with_progress(source, target, total, label):
    done, previous = 0, 0
    for block in iter(lambda: source.read(1024 * 1024), b''):
        target.write(block)
        done += len(block)
        if total >= 64 * 1024 * 1024:
            previous = report_bytes(done, total, label, previous)
    if total >= 64 * 1024 * 1024:
        report_bytes(done, total, label, previous, final=True)


def sha(path, label=None):
    digest = hashlib.sha256()
    done, previous = 0, 0
    total = Path(path).stat().st_size
    if label:
        progress(label + '...')
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
            done += len(block)
            if label and total >= 64 * 1024 * 1024:
                previous = report_bytes(done, total, label, previous)
    if label and total >= 64 * 1024 * 1024:
        report_bytes(done, total, label, previous, final=True)
    return digest.hexdigest().upper()


def assert_archive(path):
    if Path(path).stat().st_size != PACK['archiveBytes'] or sha(path, 'Checking source archive SHA256') != PACK['archiveSha256']:
        raise ValueError('Source archive does not match the supported ModDB re3 release (size/SHA256).')


def source_root(path):
    path = Path(path).resolve()
    progress('Locating the supported re3 source folder...')
    candidates = [path] + [p.parent.parent for p in path.rglob('*') if p.name.lower() == 'xbox_vehicles.img']
    valid = {}
    for candidate in candidates:
        files = {p.relative_to(candidate).as_posix().lower(): p for p in candidate.rglob('*') if p.is_file()}
        if all(rel in files for rel in PACK['files']):
            valid[candidate] = files
    if len(valid) != 1:
        raise ValueError('Select the extracted Xbox Version HD (re3) folder; older vehicle packs are unsupported.')
    root, files = next(iter(valid.items()))
    for index, (rel, expected) in enumerate(PACK['files'].items(), 1):
        file = files[rel]
        report_item(index, len(PACK['files']), 'Checking source files', rel)
        label = 'Checking ' + rel if expected['bytes'] >= 64 * 1024 * 1024 else None
        if file.is_symlink() or not file.resolve().is_relative_to(root) or file.stat().st_size != expected['bytes'] or sha(file, label) != expected['sha256']:
            raise ValueError('Source file does not match pinned re3 pack: ' + rel)
    return root, {rel: files[rel] for rel in PACK['files']}


def extract_archive(archive, destination):
    """Extract only allowlisted assets, never executables, saves or scripts."""
    assert_archive(archive)
    destination = Path(destination).resolve()
    destination.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as source:
        entries = {i.filename.lower(): i for i in source.infolist()}
        for index, rel in enumerate(PACK['files'], 1):
            report_item(index, len(PACK['files']), 'Extracting source files', rel)
            entry = entries.get((PACK['sourceFolder'] + '/' + rel).lower())
            if entry is None:
                raise ValueError('Missing pack entry: ' + rel)
            target = destination / rel
            if target.is_symlink() or not target.resolve().is_relative_to(destination):
                raise ValueError('Unsafe extraction target.')
            target.parent.mkdir(parents=True, exist_ok=True)
            with source.open(entry) as src, target.open('wb') as dst:
                copy_with_progress(src, dst, entry.file_size, 'Extracting ' + rel)
    source_root(destination)


def archive_entries(img, directory, allow_empty=False):
    data = Path(directory).read_bytes()
    length = Path(img).stat().st_size
    if not data or len(data) % 32:
        raise ValueError('Invalid IMG v1 directory length.')
    names = set()
    for sector, count, raw in struct.iter_unpack('<II24s', data):
        name = raw.split(b'\0')[0].decode('ascii').lower()
        if not NAME.fullmatch(name) or name in names or (count < 1 and not allow_empty) or (sector + count) * 2048 > length:
            raise ValueError('Invalid IMG v1 entry: ' + name)
        names.add(name)
        if count == 0:
            continue  # Four unused placeholder models in the pinned base IMG.
        yield name, sector, count


def assert_img(root):
    root = Path(root)
    expected = 0
    for name, sector, count in archive_entries(root / 'models/gta3.img', root / 'models/gta3.dir'):
        if sector != expected:
            raise ValueError('Generated IMG is not compact.')
        expected = sector + count
    if expected * 2048 != (root / 'models/gta3.img').stat().st_size:
        raise ValueError('Unexpected trailing IMG bytes.')


def valid_overlay(root, profile=None, report=False):
    root = Path(root)
    try:
        info = dict(line.split('=', 1) for line in (root / 'BUILD_INFO.txt').read_text().splitlines() if '=' in line)
        if info['Profile'] not in ('XBOX', 'MODERN') or (profile and info['Profile'] != profile.upper()):
            return False
        if info['BuilderVersion'] != BUILDER_VERSION or info['SourceSHA256'] != PACK['archiveSha256']:
            return False
        hashes = {key[7:]: value for key, value in info.items() if key.startswith('SHA256 ')}
        required = {'models/gta3.img', 'models/gta3.dir', 'vegetation_models.txt', 'vehicle_models.txt', 'asset_categories.json'}
        if not required <= hashes.keys():
            return False
        actual = {p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()}
        if actual != set(hashes) | {'BUILD_INFO.txt'}:
            return False
        for index, (rel, digest) in enumerate(hashes.items(), 1):
            p = root / rel
            if report:
                report_item(index, len(hashes), 'Verifying output files', rel)
            label = 'Verifying ' + rel if report and p.is_file() and p.stat().st_size >= 64 * 1024 * 1024 else None
            if not p.resolve().is_relative_to(root.resolve()) or p.is_symlink() or sha(p, label) != digest:
                return False
        assert_img(root)
        return True
    except (OSError, ValueError, KeyError, UnicodeError):
        return False


def remove_temporary(path, parent):
    path, parent = Path(path).absolute(), Path(parent).resolve()
    if path.parent.resolve() != parent or not re.fullmatch(r'\.xbox\.(build|source|previous)-[0-9a-f]+', path.name):
        raise ValueError('Unexpected temporary path.')
    if path.exists() and not path.is_symlink():
        shutil.rmtree(path)


def replace_directory(fresh, output, backup):
    existed = output.exists()
    if existed:
        output.rename(backup)
    try:
        fresh.rename(output)
    except OSError:
        if existed:
            backup.rename(output)
        raise
    remove_temporary(backup, output.parent)


def compressor_path():
    bundled = TOOLS / ('txdcompress.exe' if os.name == 'nt' else 'txdcompress')
    if bundled.is_file():
        return str(bundled)
    raise ValueError('Build tools/modelsets/txdcompress.cpp first; texture conversion is required.')


def build(source, output, force=False, compressor=None, profile='xbox'):
    profile = profile.lower()
    output_path = Path(output)
    linked_output = output_path.is_symlink()
    source, output = Path(source).resolve(), output_path.resolve()
    if profile not in ('xbox', 'modern') or output.name != profile or linked_output or source == output or source in output.parents or output in source.parents:
        raise ValueError('Use a separate output folder named for the requested profile (xbox or modern).')
    if output.exists():
        marker = output / 'BUILD_INFO.txt'
        if not force or not marker.is_file() or ('Profile=' + profile.upper()) not in marker.read_text().splitlines():
            raise ValueError('Refusing to replace a folder without --force and its generated profile marker.')
    _, files = source_root(source)
    progress('Source verified. Preparing the ' + profile.upper() + ' overlay...')
    compressor = str(compressor or compressor_path())
    output.parent.mkdir(parents=True, exist_ok=True)
    token = uuid.uuid4().hex
    stage = output.parent / ('.xbox.build-' + token)
    work = output.parent / ('.xbox.source-' + token)
    backup = output.parent / ('.xbox.previous-' + token)
    stage.mkdir()
    work.mkdir()
    try:
        # Later CDIMAGE packs supersede the base archive; choose one authored
        # DFF/TXD per name before converting textures for the mobile GPU.
        selected = {}
        for archive in PACK['archives']:
            progress('Reading archive directory: ' + archive)
            img = files['models/' + archive + '.img']
            for name, sector, count in archive_entries(img, files['models/' + archive + '.dir'], allow_empty=True):
                selected[name] = (img, sector, count)
        handles = {}
        try:
            for index, (name, (img, sector, count)) in enumerate(sorted(selected.items()), 1):
                report_item(index, len(selected), 'Staging model and texture entries', name)
                if img not in handles:
                    handles[img] = img.open('rb')
                stream = handles[img]
                stream.seek(sector * 2048)
                (work / name).write_bytes(stream.read(count * 2048))
        finally:
            for stream in handles.values():
                stream.close()
        for rel, file in files.items():
            if file.suffix.lower() in ('.img', '.dir'):
                continue
            target = stage / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(file, target)
        txds = sorted(list(work.glob('*.txd')) + list(stage.rglob('*.txd')))
        for index, txd in enumerate(txds, 1):
            report_item(index, len(txds), 'Converting textures', txd.name)
            result = subprocess.run([compressor, '--cap', '1024', str(txd)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if result.returncode:
                raise ValueError('Texture conversion failed: ' + txd.name)
        models = stage / 'models'
        models.mkdir(exist_ok=True)
        with (models / 'gta3.img').open('wb') as img, (models / 'gta3.dir').open('wb') as directory:
            sector = 0
            entries = sorted(work.iterdir())
            for index, file in enumerate(entries, 1):
                report_item(index, len(entries), 'Writing merged IMG', file.name)
                data = file.read_bytes()
                size = (len(data) + 2047) // 2048
                directory.write(struct.pack('<II24s', sector, size, file.name.encode('ascii')))
                img.write(data)
                img.write(b'\0' * (size * 2048 - len(data)))
                sector += size
        # These authored IDE dictionaries identify vegetation, including model
        # names without the word "tree". The default remains Classic vegetation.
        vegetation = set()
        for rel, file in files.items():
            if rel.endswith('.ide'):
                for line in file.read_text(encoding='cp1252').splitlines():
                    fields = [x.strip().lower() for x in line.split(',')]
                    if len(fields) >= 4 and fields[0].isdigit() and fields[2] in ('trees', 'trees2', 'tree1', 'tree2'):
                        vegetation.add(fields[1])
        vehicles = {name[:-4] for name, (img, _, _) in selected.items() if img.stem.lower() == 'xbox_vehicles' and name.endswith('.dff')}
        for name, values in [('vegetation', vegetation), ('vehicle', vehicles)]:
            (stage / (name + '_models.txt')).write_text('\n'.join(sorted(values)) + '\n')
        categories = {'archives': PACK['archives'], 'entries': len(selected), 'vehicles': len(vehicles), 'vegetation': len(vegetation),
                      'world': 'World models/textures, map IDE/IPL/COL, HUD/fonts/particles and splash textures',
                      'peds': 'Player and pedestrian models/textures referenced by retail IDEs',
                      'weapons': 'GTA III weapon model IDs and their texture dictionaries',
                      'excluded': 'PC binaries, scripts, settings, saves, audio, skins, neo renderer configuration; retail vehicle collisions retained'}
        (stage / 'asset_categories.json').write_text(json.dumps(categories, indent=2) + '\n')
        lines = ['Profile=' + profile.upper(), 'BuilderVersion=' + BUILDER_VERSION, 'SourceSHA256=' + PACK['archiveSha256']]
        output_files = sorted(p for p in stage.rglob('*') if p.is_file())
        for index, file in enumerate(output_files, 1):
            rel = file.relative_to(stage).as_posix()
            report_item(index, len(output_files), 'Recording output checksums', rel)
            lines.append('SHA256 ' + rel + '=' + sha(file, 'Hashing ' + rel if file.stat().st_size >= 64 * 1024 * 1024 else None))
        (stage / 'BUILD_INFO.txt').write_text('\n'.join(lines) + '\n')
        if not valid_overlay(stage, profile, report=True):
            raise ValueError('Completed overlay failed validation.')
        progress('Verification passed. Activating the completed local profile...')
        replace_directory(stage, output, backup)
        print(f'{profile.upper()} profile built and verified: {len(selected)} archive entries, {len(vehicles)} vehicles, {len(vegetation)} vegetation models.')
    finally:
        remove_temporary(stage, output.parent)
        remove_temporary(work, output.parent)


def download(destination):
    """Resolve fresh official mirrors; never silently substitute an older pack."""
    destination = Path(destination)
    if destination.is_file():
        assert_archive(destination)
        return
    progress('Requesting an official ModDB download mirror (timeout: 30 seconds)...')
    with urllib.request.urlopen(PACK['downloadPage'], timeout=30) as response:
        page = response.read(2 * 1024 * 1024).decode('utf-8', errors='replace')
    mirrors = re.findall(r'''href=["']([^"']*/downloads/mirror/224588/[^"']+)''', page)
    if not mirrors:
        raise ValueError('ModDB did not provide a download mirror. Download from ' + PACK['page'] + ' and pass --archive.')
    partial = destination.with_name(destination.name + '.partial')
    url = urllib.parse.urljoin(PACK['downloadPage'], html.unescape(mirrors[0]))
    if urllib.parse.urlparse(url).hostname not in ('www.moddb.com', 'moddb.com'):
        raise ValueError('Unexpected ModDB mirror link.')
    progress('Downloading the 1.45 GB re3 pack from ModDB...')
    with urllib.request.urlopen(url, timeout=60) as response, partial.open('wb') as stream:
        copy_with_progress(response, stream, PACK['archiveBytes'], 'Downloading archive')
    assert_archive(partial)
    partial.replace(destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('build', 'prepare', 'validate'))
    parser.add_argument('--source')
    parser.add_argument('--archive')
    parser.add_argument('--out', required=True)
    parser.add_argument('--profile', choices=('xbox', 'modern'), default='xbox')
    parser.add_argument('--work-dir')
    parser.add_argument('--force', action='store_true')
    parser.add_argument('--accept-downloads', action='store_true')
    parser.add_argument('--non-interactive', action='store_true')
    args = parser.parse_args()
    if args.command == 'validate':
        return 0 if valid_overlay(args.out, args.profile) else 1
    progress('Asset preparation started: ' + args.profile.upper())
    if args.command == 'build':
        if not args.source:
            parser.error('--source is required to build')
        build(args.source, args.out, args.force, profile=args.profile)
        return 0
    explicit = bool(args.source or args.archive)
    if not explicit:
        progress('Checking for an existing verified local profile...')
    if not explicit and valid_overlay(args.out, args.profile, report=True):
        print('Reusing the complete verified ' + args.profile.upper() + ' profile.')
        return 0
    if not args.work_dir:
        parser.error('--work-dir is required to prepare')
    work = Path(args.work_dir).resolve()
    downloads = work / 'downloads'
    downloads.mkdir(parents=True, exist_ok=True)
    source = args.source
    if not source:
        archive = Path(args.archive).resolve() if args.archive else downloads / PACK['archiveName']
        progress('Looking for source archive: ' + str(archive))
        if not archive.exists():
            accept = args.accept_downloads
            if not accept and not args.non_interactive:
                accept = prompt('Download Xbox Version HD (re3), 1.45 GB, from ModDB? [y/N]').lower() == 'y'
            if accept:
                try:
                    download(archive)
                except (OSError, ValueError) as error:
                    raise ValueError('Automatic download unavailable. Use --archive with the release from ' + PACK['page']) from error
            elif not args.non_interactive:
                archive = Path(prompt('Path to ' + PACK['archiveName'] + ':').strip().strip('"'))
        assert_archive(archive)
        source = work / 'sources' / PACK['archiveSha256'][:16]
        try:
            source_root(source)
        except (OSError, ValueError):
            extract_archive(archive, source)
    build(source, args.out, True, profile=args.profile)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print('ERROR: ' + str(error), file=sys.stderr)
        sys.exit(1)
