#!/usr/bin/env python3
"""Build and validate the Xbox assets overlay for Liberty City VR (Vehicles, Characters, Weapons, World)."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import urllib.request
import uuid

TOOLS = Path(__file__).resolve().parent
BUILDER_VERSION = '0.7.2-xbox-lcv-1'


def compatible_source_files(source):
    # This distributed pack also contains optional replacements. Do not flatten
    # them over the main set, or mix their geometry with unrelated dictionaries.
    main = source / 'Fixed Xbox Vehicles'
    if not main.is_dir():
        main = source / '[III] Fixed XBOX Vehicles v1.0' / 'Fixed Xbox Vehicles'
    if main.is_dir():
        source = main
    files = sorted((p for p in source.rglob('*') if p.suffix.lower() in ('.dff', '.txd')))
    police = next((p for p in files if p.name.lower() == 'police.txd'), None)
    foreign = police is not None and b'policemiami' in police.read_bytes().lower()
    fallback = {'ambulan', 'enforcer', 'firetruk', 'police', 'chopper'} if foreign else set()
    selected = {}
    for file in files:
        if file.stem.lower() in fallback:
            continue
        name = file.name.lower()
        if name in selected:
            raise ValueError(f'Ambiguous Xbox source entry: {name}; select one model folder.')
        selected[name] = file
    if fallback:
        print('Vice City emergency liveries detected; using base GTA III geometry and textures for: '
              + ', '.join(sorted(fallback)))
    return list(selected.values())

KNOWN_VEHICLES = {
    'landstal','idaho','stinger','linerun','peren','sentinel','patriot','firetruk',
    'trash','stretch','manana','infernus','blistac','pony','mule','cheetah',
    'ambulan','fbi','moonbeam','esperant','taxi','kuruma','bobcat','mrwhoop',
    'bfinject','corpse','police','enforcer','securica','banshee','predator','bus',
    'rhino','barracks','train','chopper','dodo','coach','cabbie','stallion',
    'rumpo','rcbandit','bellyup','mrwongs','mafia','yardie','yakuza','diablos',
    'columb','hoods','airtrain','deaddodo','speeder','reefer','panlvant','flatbed',
    'yankee','escape','borgnine','toyz',
    'blista','fbicar','panlant','ghost'
}
KNOWN_WEAPONS = {
    'bat','colt45','uzi','shotgun','ak47','m16','sniper','rocket','flame',
    'grenade','molotov','bomb','camera'
}


def sha(path):
    value = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest().upper()


def assert_img(root):
    root = Path(root)
    dir_path = root / 'models/gta3.dir'
    img_path = root / 'models/gta3.img'
    if not dir_path.is_file() or not img_path.is_file():
        raise ValueError(f'Missing gta3.img or gta3.dir in {root}')
    data = dir_path.read_bytes()
    length = img_path.stat().st_size
    if len(data) % 32 != 0:
        raise ValueError('Xbox archive directory length is not a multiple of 32 bytes.')
    entry_count = len(data) // 32
    if entry_count == 0:
        raise ValueError('Xbox archive directory is empty.')
    names = set()
    expected_sector = 0
    for sector, size, raw_name in struct.iter_unpack('<II24s', data):
        name = raw_name.split(b'\0')[0].decode('ascii', errors='replace').lower()
        if not re.fullmatch(r'[a-z0-9_]{1,19}\.(dff|txd)', name) or name in names or \
                size < 1 or sector != expected_sector or (sector + size) * 2048 > length:
            raise ValueError(f'Invalid Xbox archive directory entry: {name}')
        expected_sector = sector + size
        names.add(name)
    if expected_sector * 2048 != length:
        raise ValueError('Xbox IMG has an invalid trailing size.')


def valid_overlay(root):
    root = Path(root)
    try:
        info_file = root / 'BUILD_INFO.txt'
        if not info_file.is_file():
            return False
        lines = set(info_file.read_text(encoding='utf-8', errors='ignore').splitlines())
        if 'Profile=XBOX' not in lines or f'BuilderVersion={BUILDER_VERSION}' not in lines:
            return False
        if not (root / 'models/gta3.img').is_file() or not (root / 'models/gta3.dir').is_file():
            return False
        assert_img(root)
        return True
    except (OSError, ValueError, UnicodeError):
        return False


def remove_temporary(path, parent):
    if not path:
        return
    path, parent = Path(path).absolute(), Path(parent).resolve()
    if path.parent.resolve() != parent or not re.fullmatch(r'\.xbox\.(build|source|previous)-[0-9a-f]+', path.name):
        return
    if path.is_symlink() or not path.exists():
        return
    shutil.rmtree(path, ignore_errors=True)


def replace_directory(fresh, output, backup):
    had_output = output.exists()
    if had_output:
        output.rename(backup)
    try:
        fresh.rename(output)
    except OSError:
        if had_output and not output.exists():
            backup.rename(output)
        raise
    remove_temporary(backup, output.parent)


def build(source, output, force=False, compressor=None):
    source, output = Path(source).resolve(), Path(output).absolute()
    if output.name != 'xbox' or source == output or source in output.parents or output in source.parents:
        raise ValueError("Use a separate output folder named 'xbox', outside the input source.")
    if output.exists():
        if not force or 'Profile=XBOX' not in (output / 'BUILD_INFO.txt').read_text(errors='ignore').splitlines():
            raise ValueError('Refusing to replace an existing folder without -Force and a generated Xbox marker.')
    output.parent.mkdir(parents=True, exist_ok=True)
    unique = uuid.uuid4().hex
    stage = output.parent / ('.xbox.build-' + unique)
    work = output.parent / ('.xbox.source-' + unique)
    backup = output.parent / ('.xbox.previous-' + unique)
    stage.mkdir(parents=True, exist_ok=True)
    work.mkdir(parents=True, exist_ok=True)
    try:
        (stage / 'models/coll').mkdir(parents=True, exist_ok=True)
        (stage / 'models/generic').mkdir(parents=True, exist_ok=True)
        (stage / 'models').mkdir(parents=True, exist_ok=True)

        # Collect all .dff and .txd files
        source_files = compatible_source_files(source)
        if not source_files:
            raise ValueError(f'No .dff or .txd files found in {source}')

        for file in source_files:
            lower = file.name.lower()
            if lower in ('wheels.dff', 'wheels.txd'):
                shutil.copyfile(file, stage / 'models/generic' / lower)
            shutil.copyfile(file, work / lower)

        col_files = list(source.rglob('vehicles.col'))
        if col_files:
            try:
                col_data = col_files[0].read_bytes()
                has_train = False
                for i in range(0, len(col_data) - 30):
                    if col_data[i:i+4] in (b'COLL', b'COL2', b'COL3'):
                        name = col_data[i+8:i+30].split(b'\0')[0].decode('ascii', errors='ignore').lower()
                        if name == 'train':
                            has_train = True
                            break
                if has_train:
                    shutil.copyfile(col_files[0], stage / 'models/coll/vehicles.col')
                else:
                    print('Note: Skipping vehicles.col from Xbox source (missing GTA III train record, likely a foreign/Vice City collision file).')
            except Exception as e:
                print(f'Note: Skipping vehicles.col: {e}')

        if compressor:
            compressor = str(Path(compressor).resolve())
        else:
            bundled = TOOLS / ('txdcompress.exe' if os.name == 'nt' else 'txdcompress')
            if bundled.is_file():
                compressor = str(bundled)
            else:
                compiler = shutil.which('c++') or shutil.which('g++')
                if compiler and (TOOLS / 'txdcompress.cpp').is_file():
                    compressor = str(stage / 'txdcompress')
                    subprocess.run([compiler, '-O2', '-o', compressor, str(TOOLS / 'txdcompress.cpp')], check=True)
                else:
                    compressor = None

        if compressor:
            all_txds = list(work.glob('*.txd'))
            gw = stage / 'models/generic/wheels.txd'
            if gw.is_file():
                all_txds.append(gw)
            for txd in sorted(all_txds):
                subprocess.run([compressor, str(txd)], check=False)

        staged_files = sorted(work.iterdir(), key=lambda p: p.name)
        with (stage / 'models/gta3.img').open('wb') as img, (stage / 'models/gta3.dir').open('wb') as directory:
            sector = 0
            for file in staged_files:
                data = file.read_bytes()
                size = math.ceil(len(data) / 2048)
                directory.write(struct.pack('<II24s', sector, size, file.name.encode('ascii')))
                img.write(data)
                padding = size * 2048 - len(data)
                if padding > 0:
                    img.write(b'\0' * padding)
                sector += size

        dff_stems = [f.stem.lower() for f in staged_files if f.suffix.lower() == '.dff']
        vehicles = [s for s in dff_stems if s in KNOWN_VEHICLES]
        weapons = [s for s in dff_stems if s in KNOWN_WEAPONS]
        peds = [s for s in dff_stems if s not in vehicles and s not in weapons and re.match(r'^(player|cop|swat|fbi|army|medic|fireman|male\d+|female\d+|asuka|salvatore|maria|donald|kenji|misty|ray|phil|catalina|miguel|eight|8ball|luigi|joey|chunky)', s)]
        world = [s for s in dff_stems if s not in vehicles and s not in weapons and s not in peds]

        if vehicles:
            (stage / 'vehicle_models.txt').write_text('\n'.join(sorted(set(vehicles))) + '\n', encoding='utf-8')
        if peds:
            (stage / 'ped_models.txt').write_text('\n'.join(sorted(set(peds))) + '\n', encoding='utf-8')
        if weapons:
            (stage / 'weapon_models.txt').write_text('\n'.join(sorted(set(weapons))) + '\n', encoding='utf-8')

        assert_img(stage)

        lines = [
            'Liberty City VR Xbox Assets Profile',
            'Profile=XBOX',
            f'BuilderVersion={BUILDER_VERSION}',
            f'VehicleCount={len(vehicles)}',
            f'PedCount={len(peds)}',
            f'WeaponCount={len(weapons)}',
            f'WorldCount={len(world)}',
            f'ArchiveEntries={len(staged_files)}',
            f'SHA256 models/gta3.img={sha(stage / "models/gta3.img")}',
            f'SHA256 models/gta3.dir={sha(stage / "models/gta3.dir")}'
        ]
        for rel in ('models/coll/vehicles.col', 'models/generic/wheels.dff', 'models/generic/wheels.txd', 'vehicle_models.txt'):
            p = stage / rel
            if p.is_file():
                lines.append(f'SHA256 {rel}={sha(p)}')
        lines.append('Assets are supplied from original Xbox game data / conversion packs; do not redistribute.')
        (stage / 'BUILD_INFO.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')

        if not valid_overlay(stage):
            raise ValueError('Completed Xbox profile failed validation.')
        replace_directory(stage, output, backup)
        print(f'Xbox profile built and verified: {output} ({len(staged_files)} entries).')
    finally:
        remove_temporary(stage, output.parent)
        remove_temporary(work, output.parent)


def extract_archive(archive, destination):
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    for tool in ('bsdtar', 'tar'):
        exe = shutil.which(tool)
        if exe:
            res = subprocess.run([exe, '-xf', str(archive), '-C', str(destination)], capture_output=True)
            if res.returncode == 0:
                return
    for tool in ('7zz', '7z'):
        exe = shutil.which(tool)
        if exe:
            res = subprocess.run([exe, 'x', '-y', f'-o{destination}', str(archive)], capture_output=True)
            if res.returncode == 0:
                return
    if str(archive).lower().endswith('.zip'):
        import zipfile
        with zipfile.ZipFile(archive, 'r') as zf:
            zf.extractall(destination)
        return
    raise ValueError(f'Could not extract archive {archive}. Extract it manually.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('build', 'prepare', 'validate'))
    parser.add_argument('--source')
    parser.add_argument('--out', required=True)
    parser.add_argument('--archive')
    parser.add_argument('--work-dir')
    parser.add_argument('--force', action='store_true')
    parser.add_argument('--accept-downloads', action='store_true')
    parser.add_argument('--non-interactive', action='store_true')
    args = parser.parse_args()

    if args.command == 'validate':
        return 0 if valid_overlay(args.out) else 1
    if args.command == 'build':
        if not args.source:
            parser.error('--source is required to build')
        build(args.source, args.out, args.force)
        return 0

    if valid_overlay(args.out):
        print(f'Reusing the complete verified Xbox profile: {args.out}')
        return 0

    if not args.work_dir:
        parser.error('--work-dir is required to prepare')
    work = Path(args.work_dir).resolve()
    downloads = work / 'downloads'
    downloads.mkdir(parents=True, exist_ok=True)
    sources = work / 'sources'
    sources.mkdir(parents=True, exist_ok=True)

    source_dir = None
    if args.source and Path(args.source).is_dir():
        source_dir = Path(args.source).resolve()

    archive = Path(args.archive).resolve() if args.archive and Path(args.archive).is_file() else None

    # Check local downloads cache
    if not source_dir and not archive:
        for f in downloads.glob('*'):
            if f.suffix.lower() in ('.zip', '.rar', '.7z') and f.stat().st_size > 100000:
                if 'xbox' in f.name.lower() or 'fixed' in f.name.lower():
                    archive = f
                    print(f'Reusing local archive found in downloads (skipping redownload): {archive}')
                    break

    extract_dest = sources / 'xbox-pack'
    if not source_dir and not archive:
        if extract_dest.is_dir() and len(list(extract_dest.rglob('*.dff'))) > 10:
            source_dir = extract_dest
            print(f'Reusing existing extracted Xbox source folder: {source_dir}')

    if not source_dir and not archive:
        if not args.non_interactive:
            ans = input('No local Xbox archive detected. Enter path to Xbox assets archive or folder: ').strip().strip('"').strip("'")
            if ans and Path(ans).exists():
                p = Path(ans).resolve()
                if p.is_dir():
                    source_dir = p
                else:
                    archive = p

    if not source_dir and not archive:
        raise ValueError('No valid Xbox assets archive or folder was provided.')

    if not source_dir and archive:
        extract_archive(archive, extract_dest)
        source_dir = extract_dest

    build(source_dir, args.out, True)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f'ERROR: {error}', file=sys.stderr)
        sys.exit(1)
