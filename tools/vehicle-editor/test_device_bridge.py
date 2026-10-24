import contextlib
import importlib.util
import io
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('bridge', Path(__file__).with_name('device_bridge.py'))
b = importlib.util.module_from_spec(spec)
spec.loader.exec_module(b)


class PackTests(unittest.TestCase):
    def test_validation_and_roundtrip(self):
        entries = {'xbox:95': {'WheelX': 2, 'DashFollowWheel': 0, 'MapPitch': 33, 'RadioX': 35, 'RadioRoll': -180, 'RadioSize': 125}}
        self.assertEqual(b.parse(b.serialize(entries)), entries)
        for text in ('[Placement]\nVersion=2', '[Placement]\nVersion=1\n[xbox:95]\nWheelX=nan',
                     '[Placement]\nVersion=1\n[xbox:95]\nWheelVisible=.3',
                     '[Placement]\nVersion=1\n[xbox:95]\nWheelRadius=1',
                     '[Placement]\nVersion=1\n[xbox:95]\nUnknown=3',
                     '[Placement]\nVersion=1\n[xbox:95]\nWheelX=1\nWheelX=2'):
            with self.assertRaises(b.BridgeError):
                b.parse(text)

    def test_preserve_unrelated_remote_changes(self):
        base = {'classic:95': {'WheelX': 0, 'WheelY': 0}, 'xbox:95': {'WheelX': 1}}
        remote = {'classic:95': {'WheelX': 0, 'WheelY': 9}, 'xbox:95': {'WheelX': 8}}
        local = {'classic:95': {'WheelX': 3, 'WheelY': 0}, 'xbox:95': {'WheelX': 1}}
        result, conflicts = b.merge(base, remote, local, ['classic:95'])
        self.assertEqual(result, {'classic:95': {'WheelX': 3, 'WheelY': 9}, 'xbox:95': {'WheelX': 8}})
        self.assertEqual(conflicts, [])

    def test_conflicts_and_resolution(self):
        base = {'xbox:95': {'WheelX': 0}}
        remote = {'xbox:95': {'WheelX': 1}}
        local = {'xbox:95': {'WheelX': 2}}
        with self.assertRaises(b.BridgeError):
            b.merge(base, remote, local, ['xbox:95'])
        self.assertEqual(b.merge(base, remote, local, ['xbox:95'], 'local')[0], local)
        self.assertEqual(b.merge(base, remote, local, ['xbox:95'], 'remote')[0], remote)

    def test_reset_and_concurrent_new_field(self):
        base = {'xbox:95': {'WheelX': 0}}
        remote = {'xbox:95': {'WheelX': 0, 'MapX': 5}}
        self.assertEqual(b.merge(base, remote, {}, ['xbox:95'])[0], {'xbox:95': {'MapX': 5}})


class DiscoveryTests(unittest.TestCase):
    def discover(self, output, model=b'Quest 3', index=None):
        def run(args, **kwargs):
            return subprocess.CompletedProcess(args, 0, output if args[1:3] == ['devices', '-l'] else model, b'')
        with patch.object(b.subprocess, 'run', side_effect=run):
            return b.Device('adb', index)

    def test_no_eligible_device(self):
        for output, model in [(b'List of devices attached\n', b''),
                              (b'List of devices attached\ntest-target unauthorized\n', b'Quest 3'),
                              (b'List of devices attached\ntest-target device\n', b'Unrelated phone'),
                              (b'List of devices attached\ntest-target offline\n', b'Quest 3')]:
            with self.assertRaises(b.BridgeError) as error:
                self.discover(output, model)
            self.assertNotIn('test-target', str(error.exception))

    def test_multiple_devices_require_selection(self):
        output = b'List of devices attached\ntest-one device\ntest-two device\n'
        with self.assertRaises(b.BridgeError):
            self.discover(output)
        self.assertEqual(self.discover(output, index=2).serial, 'test-two')

    def test_missing_adb_sanitized(self):
        with patch.object(b.subprocess, 'run', side_effect=OSError('sensitive transport output')):
            with self.assertRaises(b.BridgeError) as error:
                b.Device('adb')
        self.assertNotIn('sensitive', str(error.exception))


class TransferTests(unittest.TestCase):
    def device(self, remote=b.EMPTY.encode()):
        d = object.__new__(b.Device)
        d.files = {b.FILES['personal']: remote}
        d.commands = []
        d.running = False
        d.corrupt = False
        def run(*args, check=True):
            d.commands.append(args)
            if args[0] == 'push':
                d.files[args[2].split('/')[-1]] = b'corrupt' if d.corrupt else Path(args[1]).read_bytes()
            elif args[:1] == ('shell',):
                cmd = args[1]
                if cmd.startswith('if pidof'):
                    return subprocess.CompletedProcess(args, 0, b'RUNNING' if d.running else b'STOPPED', b'')
                if cmd.startswith('cp '):
                    _, src, dst = cmd.split()
                    d.files[dst.split('/')[-1]] = d.files[src.split('/')[-1]]
                if cmd.startswith('mv '):
                    _, src, dst = cmd.split()
                    d.files[dst.split('/')[-1]] = d.files.pop(src.split('/')[-1])
                if cmd.startswith('rm -f '):
                    d.files.pop(cmd.split('/')[-1], None)
            return subprocess.CompletedProcess(args, 0, b'', b'')
        d.run = run
        d.read = lambda name: d.files.get(name)
        return d

    def test_verified_install_keeps_backup(self):
        d = self.device()
        text = b.serialize({'xbox:95': {'WheelX': 5}})
        d.install(b.FILES['personal'], text, b.EMPTY.encode())
        self.assertEqual(d.files[b.FILES['personal']], text.encode())
        backups = [v for k, v in d.files.items() if '.backup-' in k]
        self.assertEqual(backups, [b.EMPTY.encode()])
        self.assertFalse(any(k.endswith('.upload') for k in d.files))

    def test_failed_upload_preserves_destination(self):
        d = self.device()
        d.corrupt = True
        with self.assertRaises(b.BridgeError):
            d.install(b.FILES['personal'], b.EMPTY, b.EMPTY.encode())
        self.assertEqual(d.files, {b.FILES['personal']: b.EMPTY.encode()})

    def test_running_game_and_changed_destination(self):
        d = self.device()
        d.running = True
        with self.assertRaises(b.BridgeError):
            d.install(b.FILES['personal'], b.EMPTY, b.EMPTY.encode())
        d.running = False
        with self.assertRaises(b.BridgeError):
            d.install(b.FILES['personal'], b.EMPTY, b'old contents')
        self.assertFalse(any(c[0] == 'push' for c in d.commands))

    def test_interrupted_upload(self):
        d = self.device()
        original = d.run
        def run(*args, **kwargs):
            if args[0] == 'push':
                raise b.BridgeError('Device operation failed')
            return original(*args, **kwargs)
        d.run = run
        with self.assertRaises(b.BridgeError):
            d.install(b.FILES['personal'], b.EMPTY, b.EMPTY.encode())
        self.assertEqual(d.files[b.FILES['personal']], b.EMPTY.encode())

    def test_restore_backup_and_preserve_current(self):
        d = self.device(b.serialize({'xbox:95': {'WheelX': 12}}).encode())
        name = b.FILES['personal'] + '.backup-20261024T120000123456Z'
        d.files[name] = b.serialize({'xbox:95': {'WheelX': 5}}).encode()
        with patch.object(b, 'Device', return_value=d), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(b.main(['restore', '--workspace', '.', '--backup', name]), 0)
        self.assertEqual(b.parse(d.files[b.FILES['personal']].decode()), {'xbox:95': {'WheelX': 5}})
        self.assertTrue(any(b'WheelX=12' in v for k, v in d.files.items() if '.backup-' in k))


if __name__ == '__main__':
    unittest.main()
