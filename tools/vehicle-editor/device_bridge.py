#!/usr/bin/env python3
"""Local-only placement transfer. Never prints device identifiers or ADB output."""
import argparse
import configparser
import datetime
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time
import uuid

ROOT = '/sdcard/Android/data/com.libertyvr.quest/files/gamedata'
PACKAGE = 'com.libertyvr.quest'
FILES = {kind: f'vehicle_placements.{kind}.ini' for kind in ('bundled', 'template', 'personal')}
EMPTY = '[Placement]\nVersion=1\n'


class BridgeError(Exception):
    pass


def limits(key):
    if key == 'WheelRadius':
        return 8, 40
    if key in ('WheelVisible', 'DashFollowWheel'):
        return 0, 1
    if re.fullmatch(r'(Dash|Map|Status|Clock|Ammo|Radio)Size', key):
        return 40, 250
    if re.fullmatch(r'(Wheel|Dash|Map|Status|Clock|Ammo|Radio)(X|Y|Z)', key):
        return -200, 200
    if re.fullmatch(r'(Wheel|Dash|Map|Status|Clock|Ammo|Radio)(Pitch|Yaw|Roll)', key):
        return (-90, 90) if key.startswith('Wheel') else (-180, 180)
    if re.fullmatch(r'(Default)?Seat(Height|Distance)', key):
        return (-200, 250) if key.endswith('Height') else (-200, 200)
    raise BridgeError('Unknown placement field')


def parse(text):
    if len(text) > 4 * 1024 * 1024:
        raise BridgeError('Placement pack exceeds limits')
    c = configparser.ConfigParser(interpolation=None, strict=True)
    c.optionxform = str
    try:
        c.read_string(text)
        if c.defaults() or c.get('Placement', 'Version') != '1' or set(c['Placement']) != {'Version'}:
            raise BridgeError('Unsupported placement version')
        result = {}
        for section in c.sections():
            if section == 'Placement':
                continue
            if not re.fullmatch(r'(classic|modern|xbox):[0-9]{1,5}', section):
                raise BridgeError('Invalid placement section')
            result[section] = {}
            for k, v in c[section].items():
                n = float(v)
                lo, hi = limits(k)
                if not math.isfinite(n) or not lo <= n <= hi or (k in ('WheelVisible', 'DashFollowWheel') and n not in (0, 1)):
                    raise BridgeError('Invalid placement value')
                result[section][k] = n
        return result
    except (ValueError, configparser.Error) as exc:
        raise BridgeError('Malformed placement pack') from exc


def serialize(entries):
    out = [EMPTY]
    for section, values in sorted(entries.items()):
        if values:
            out.append(f'\n[{section}]\n')
            out.extend(f'{k}={v:.9g}\n' for k, v in sorted(values.items()))
    text = ''.join(out)
    parse(text)
    return text


def merge(base, remote, local, sections, preference=None):
    # Missing local fields are deliberate reset-to-template operations, but only
    # within selected sections. Remote-only changes survive the three-way merge.
    out = {s: dict(v) for s, v in remote.items()}
    conflicts = []
    for s in sections:
        b, r, l = base.get(s, {}), remote.get(s, {}), local.get(s, {})
        target = out.setdefault(s, {})
        for k in set(b) | set(l):
            bv, rv, lv = b.get(k), r.get(k), l.get(k)
            if lv == bv:
                continue
            if rv != bv and rv != lv:
                conflicts.append(f'{s}/{k}')
                if preference != 'local':
                    continue
            if lv is None:
                target.pop(k, None)
            else:
                target[k] = lv
    if conflicts and preference is None:
        raise BridgeError('Conflicting entries: ' + ', '.join(sorted(conflicts)))
    return out, conflicts


class Device:
    def __init__(self, adb, index=None):
        self.adb = adb
        try:
            p = subprocess.run([adb, 'devices', '-l'], capture_output=True, timeout=15)
        except (OSError, subprocess.TimeoutExpired) as exc:
            raise BridgeError('ADB unavailable or unresponsive') from exc
        if p.returncode:
            raise BridgeError('ADB discovery failed')
        candidates = []
        for line in p.stdout.decode(errors='replace').splitlines()[1:]:
            fields = line.split()
            if len(fields) < 2 or fields[1] != 'device':
                continue
            serial = fields[0]
            try:
                model = subprocess.run([adb, '-s', serial, 'shell', 'getprop', 'ro.product.model'], capture_output=True, timeout=10)
            except (OSError, subprocess.TimeoutExpired):
                continue
            if model.returncode == 0 and model.stdout.strip().lower() == b'quest 3':
                candidates.append(serial)
        if not candidates:
            raise BridgeError('No authorized, responsive Quest 3 is connected')
        if index is None and len(candidates) > 1:
            raise BridgeError(f'{len(candidates)} eligible headsets; choose a device index (1 through {len(candidates)})')
        choice = 0 if index is None else index - 1
        if not 0 <= choice < len(candidates):
            raise BridgeError('Invalid device index')
        self.serial = candidates[choice]  # local memory only; never returned
        self.count = len(candidates)

    def run(self, *args, check=True):
        try:
            p = subprocess.run([self.adb, '-s', self.serial, *args], capture_output=True, timeout=30)
        except (OSError, subprocess.TimeoutExpired) as exc:
            raise BridgeError('Device command failed or timed out') from exc
        if check and p.returncode:
            raise BridgeError('Device operation failed; existing game data was preserved')
        return p

    def is_running(self):
        # pidof's nonzero status alone could mean a shell/transport failure.
        p = self.run('shell', f'if pidof {PACKAGE} >/dev/null; then echo RUNNING; else echo STOPPED; fi')
        if p.stdout.strip() not in (b'STOPPED', b'RUNNING'):
            raise BridgeError('Cannot determine game state')
        return p.stdout.strip() == b'RUNNING'

    def stopped(self):
        if self.is_running():
            raise BridgeError('Close Liberty City VR on the headset before transfer')

    def read(self, name):
        path = f'{ROOT}/../{name}' if name == 'radio_board_defaults.ini' else f'{ROOT}/{name}'
        p = self.run('shell', f'if [ -f {path} ]; then echo PRESENT; else echo ABSENT; fi')
        if p.stdout.strip() == b'ABSENT':
            return None
        if p.stdout.strip() != b'PRESENT':
            raise BridgeError('Cannot inspect headset placement files')
        with tempfile.TemporaryDirectory(prefix='lcvr-placement-') as directory:
            local = Path(directory) / 'pack.ini'
            self.run('pull', path, str(local))
            if local.stat().st_size > 4 * 1024 * 1024:
                raise BridgeError('Device configuration exceeds limits')
            return local.read_bytes()

    def install(self, name, text, expected, live=False):
        if not live:
            self.stopped()
        if self.read(name) != expected:
            raise BridgeError('Destination changed during preview; import and preview again')
        tmp_name = name + '.upload'
        try:
            with tempfile.TemporaryDirectory(prefix='lcvr-placement-') as directory:
                local = Path(directory) / 'pack.ini'
                content = text.encode('utf-8')
                local.write_bytes(content)
                self.run('push', str(local), f'{ROOT}/{tmp_name}')
                if self.read(tmp_name) != content:
                    raise BridgeError('Upload verification failed; destination preserved')
            if not live:
                self.stopped()
            if self.read(name) != expected:
                raise BridgeError('Destination changed during upload; import again')
            stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
            path = f'{ROOT}/{name}'
            if expected is not None:
                self.run('shell', f'cp {path} {path}.backup-{stamp}')
            self.run('shell', f'mv {ROOT}/{tmp_name} {path}')
            return name + '.backup-' + stamp if expected is not None else None
        finally:
            self.run('shell', f'rm -f {ROOT}/{tmp_name}', check=False)


    def acknowledge(self, kind, text):
        if not self.is_running():
            return False
        nonce = uuid.uuid4().hex
        with tempfile.TemporaryDirectory(prefix='lcvr-reload-') as folder:
            local = Path(folder) / 'request.txt'
            local.write_text(nonce, encoding='ascii')
            self.run('push', str(local), f'{ROOT}/vehicle_placements.reload')
        expected = str(content_hash(text.encode('utf-8')))
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            raw = self.read('vehicle_placements.live.ini')
            if raw:
                try:
                    status = configparser.ConfigParser()
                    status.read_string(raw.decode('ascii'))
                    if status.get('LivePlacement', 'Version', fallback='') == '1' and status.get('LivePlacement', 'Request', fallback='') == nonce and status.get('LivePlacement', kind, fallback='') == expected:
                        return True
                except (UnicodeError, configparser.Error):
                    pass
            time.sleep(.15)
        raise BridgeError('Pack installed, but the running game did not acknowledge it. Update the APK or resume the game; live sync has stopped.')


def content_hash(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return value


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=['status', 'import', 'preview', 'deploy', 'restore'])
    p.add_argument('--workspace', type=Path, required=True)
    p.add_argument('--adb', default='adb')
    p.add_argument('--device-index', type=int)
    p.add_argument('--kind', choices=['template', 'personal'], default='personal')
    p.add_argument('--pack', type=Path)
    p.add_argument('--sections', default='')
    p.add_argument('--prefer', choices=['local', 'remote'])
    p.add_argument('--live', action='store_true', help='Allow atomic deployment while the updated game runs and verify reload acknowledgement')
    p.add_argument('--backup', help='Exact configuration backup filename returned by deployment')
    a = p.parse_args(argv)
    try:
        d = Device(a.adb, a.device_index)
        if a.action == 'status':
            print(f'personal Quest 3 device: authorized and responsive; eligible headsets: {d.count}')
            return 0
        baseline = a.workspace / '.device-baseline'
        if a.action == 'restore':
            d.stopped()
            name = FILES[a.kind]
            if not a.backup or not re.fullmatch(re.escape(name) + r'\.backup-[0-9]{8}T[0-9]{12}Z', a.backup):
                raise BridgeError('Choose a valid backup filename for this placement layer')
            raw = d.read(a.backup)
            if raw is None:
                raise BridgeError('Configuration backup is unavailable')
            text = serialize(parse(raw.decode('utf-8-sig')))
            backup = d.install(name, text, d.read(name))
            print('Restored configuration on personal Quest 3 device. Import again before further edits. Restart the game to apply.')
            if backup:
                print('Previous configuration backup: ' + backup)
            return 0
        if a.action == 'import':
            # Stage all reads and validation before replacing any local file.
            staged = {}
            for name in [*FILES.values(), 'vr_settings.ini', 'radio_board.ini', 'radio_board_defaults.ini']:
                content = d.read(name)
                if content is not None and name in FILES.values():
                    parse(content.decode('utf-8-sig'))
                staged[name] = content
            baseline.mkdir(parents=True, exist_ok=True)
            for name, content in staged.items():
                if name in FILES.values():
                    content = content or EMPTY.encode()
                else:
                    content = content or b''
                dest = a.workspace / name
                if dest.exists():
                    shutil.copy2(dest, dest.with_suffix(dest.suffix + '.before-import'))
                dest.write_bytes(content)
                (baseline / name).write_bytes(content)
            print('Imported placement layers and legacy settings from personal Quest 3 device. Classic/Modern legacy values remain shared fallbacks.')
            return 0
        if a.pack is None:
            raise BridgeError('Choose a saved placement pack')
        name = FILES[a.kind]
        base_path = baseline / name
        if not base_path.exists():
            raise BridgeError('Import the headset configuration before previewing or deploying')
        base = parse(base_path.read_text(encoding='utf-8-sig'))
        local = parse(a.pack.read_text(encoding='utf-8-sig'))
        raw = d.read(name)
        remote = parse(raw.decode('utf-8-sig')) if raw is not None else {}
        sections = a.sections.split(',') if a.sections else sorted(set(base) | set(local))
        if any(not re.fullmatch(r'(classic|modern|xbox):[0-9]{1,5}', s) for s in sections):
            raise BridgeError('Invalid selected entries')
        merged, conflicts = merge(base, remote, local, sections, a.prefer)
        text = serialize(merged)
        changed = sum(remote.get(s, {}).get(k) != merged.get(s, {}).get(k)
                      for s in set(remote) | set(merged)
                      for k in set(remote.get(s, {})) | set(merged.get(s, {})))
        print(f'{a.kind}: {len(sections)} selected entries, {changed} changed fields, {len(conflicts)} resolved conflicts.')
        for s in sections:
            for k in sorted(set(remote.get(s, {})) | set(merged.get(s, {}))):
                before, after = remote.get(s, {}).get(k), merged.get(s, {}).get(k)
                if before != after:
                    print(f'{s}/{k}: {before} -> {after}')
        if a.kind == 'template':
            personal_raw = d.read(FILES['personal'])
            personal = parse(personal_raw.decode('utf-8-sig')) if personal_raw else {}
            masked = [f'{s}/{k}' for s in sections for k in merged.get(s, {}) if k in personal.get(s, {})]
            if masked:
                print('Personal overrides mask template fields: ' + ', '.join(masked))
        if a.action == 'deploy':
            backup = d.install(name, text, raw, live=a.live)
            (baseline / name).write_text(text, encoding='utf-8')
            if a.live and d.acknowledge(a.kind, text):
                print('Live reload acknowledged by the running game.')
            else:
                print('The installed pack will load when the game starts.')
            print('Installed on personal Quest 3 device with backup and verified upload. Transfer complete.')
            if backup:
                print('Previous configuration backup: ' + backup)
        return 0
    except (BridgeError, OSError, UnicodeError) as exc:
        # Only our deliberately sanitized messages are safe to display.
        print(str(exc) if isinstance(exc, BridgeError) else 'Local configuration operation failed', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
