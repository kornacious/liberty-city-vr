"""Synthetic local placement transfer tests; no game data or headset access."""
import contextlib
import io
from pathlib import Path
import tempfile
import subprocess
import os
import unittest
from unittest.mock import patch
import device_bridge as b


class PcvrTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.data = self.root / 'data'
        self.user = self.root / 'user'
        self.workspace = self.root / 'workspace'
        for path in (self.data, self.user, self.workspace):
            path.mkdir()
        (self.data / '.libertyvr-pcvr-data').write_text('fixture')
        self.device = b.LocalDevice(self.data, self.user)
        self.running = patch.object(b.LocalDevice, 'is_running', return_value=False)
        self.running.start()
        self.addCleanup(self.running.stop)

    def call(self, action, *args):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return b.main([action, '--target', 'pcvr', '--pcvr-data', str(self.data),
                           '--pcvr-user', str(self.user), '--workspace', str(self.workspace), *args])

    def test_atomic_backup_and_conflict_preservation(self):
        name = b.FILES['personal']
        text = b.serialize({'xbox:95': {'WheelX': 12, 'SpeedometerSize': 90}})
        (self.data / name).write_bytes(b.EMPTY.encode())
        backup = self.device.install(name, text, b.EMPTY.encode())
        self.assertEqual(self.device.read(name), text.encode())
        self.assertEqual(self.device.read(backup), b.EMPTY.encode())
        with self.assertRaises(b.BridgeError):
            self.device.install(name, b.EMPTY, b.EMPTY.encode())
        self.assertEqual(self.device.read(name), text.encode())
        self.assertFalse(list(self.data.glob('*.incoming-*')))

    def test_import_is_target_scoped_and_preserves_shared_template(self):
        quest = self.workspace / b.FILES['personal']
        quest.write_text(b.serialize({'classic:95': {'WheelX': 1}}))
        template = self.workspace / b.FILES['template']
        template.write_text(b.serialize({'xbox:95': {'WheelX': 7}}))
        (self.data / b.FILES['personal']).write_text(b.serialize({'xbox:95': {'WheelX': 3}}))
        self.assertEqual(self.call('import'), 0)
        self.assertEqual(b.parse(quest.read_text())['classic:95']['WheelX'], 1)
        self.assertEqual(b.parse(template.read_text())['xbox:95']['WheelX'], 7)
        local = self.workspace / 'targets/pcvr' / b.FILES['personal']
        self.assertEqual(b.parse(local.read_text())['xbox:95']['WheelX'], 3)
        self.assertEqual(self.call('import', '--import-template'), 0)
        self.assertEqual(b.parse(template.read_text()), {})

    def test_selected_merge_keeps_unrelated_target_fields(self):
        name = b.FILES['personal']
        (self.data / name).write_text(b.serialize({'xbox:95': {'WheelX': 1}, 'classic:96': {'WheelX': 8}}))
        self.assertEqual(self.call('import'), 0)
        local = self.workspace / 'targets/pcvr' / name
        local.write_text(b.serialize({'xbox:95': {'WheelX': 9}}))
        self.assertEqual(self.call('deploy', '--pack', str(local), '--sections', 'xbox:95'), 0)
        self.assertEqual(b.parse(self.device.read(name).decode()), {'xbox:95': {'WheelX': 9}, 'classic:96': {'WheelX': 8}})

    def test_endpoint_baseline_is_not_reused_for_another_installation(self):
        self.assertEqual(self.call('import'), 0)
        other = self.root / 'other'; other.mkdir()
        (other / '.libertyvr-pcvr-data').write_text('fixture')
        pack = self.workspace / 'pack.ini'; pack.write_text(b.EMPTY)
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(b.main(['preview', '--target', 'pcvr', '--pcvr-data', str(other),
                '--pcvr-user', str(self.user), '--workspace', str(self.workspace), '--pack', str(pack)]), 1)

    def test_shared_template_does_not_replace_target_personal_overrides(self):
        personal = b.serialize({'xbox:95': {'WheelX': 3}}).encode()
        (self.data / b.FILES['personal']).write_bytes(personal)
        template = self.workspace / b.FILES['template']
        template.write_text(b.serialize({'xbox:95': {'WheelX': 7, 'MapSize': 80}}))
        self.assertEqual(self.call('import'), 0)
        self.assertEqual(self.call('deploy', '--kind', 'template', '--pack', str(template), '--sections', 'xbox:95'), 0)
        self.assertEqual(self.device.read(b.FILES['personal']), personal)
        self.assertEqual(b.parse(self.device.read(b.FILES['template']).decode())['xbox:95']['WheelX'], 7)

    def test_unmarked_and_arbitrary_files_rejected(self):
        with self.assertRaises(b.BridgeError):
            b.LocalDevice(self.user, self.user)
        for name in ('../saves/save.b', 'models/gta3.img', 'vehicle_placements.bundled.ini.upload'):
            with self.assertRaises(b.BridgeError): self.device.read(name)
        with self.assertRaises(b.BridgeError):
            self.device.install(b.FILES['bundled'], b.EMPTY, None)

    def test_linked_paths_rejected(self):
        link = self.root / 'link'
        self.link(link, self.data)
        with self.assertRaises(b.BridgeError): b.LocalDevice(link, self.user)

    def test_workspace_links_rejected_before_import_writes(self):
        self.link(self.workspace / 'targets', self.data)
        self.assertEqual(self.call('import'), 1)
        self.assertFalse((self.data / 'pcvr').exists())

    def link(self, link, destination):
        try: link.symlink_to(destination, target_is_directory=True)
        except OSError:
            if os.name != 'nt': self.skipTest('Host cannot create symbolic links')
            result = subprocess.run(['cmd', '/c', 'mklink', '/J', str(link), str(destination)], capture_output=True)
            if result.returncode: self.skipTest('Host cannot create a symlink or junction')


if __name__ == '__main__':
    unittest.main()
