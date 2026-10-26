#!/usr/bin/env python3
"""Production preparation regressions using synthetic assets; no ADB/network."""
import copy
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest import mock
import zipfile

spec = importlib.util.spec_from_file_location('xbox', Path(__file__).resolve().parents[1] / 'modelsets/xbox-modelset.py')
x = importlib.util.module_from_spec(spec)
spec.loader.exec_module(x)

class Preparation(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.source = self.root / 'source'
        (self.source / 'models').mkdir(parents=True)
        self.saved = copy.deepcopy(x.PACK)
        self.pack = copy.deepcopy(x.PACK)
        txd = struct.pack('<III', 0x16, 0, 0x1803ffff)
        self.entries = {
            'gta3': {'building.dff': b'original building', 'building.txd': txd, 'cop.dff': b'ped', 'colt45.dff': b'weapon'},
            'xbox_models': {'building.dff': b'Xbox building'},
            'xbox_hd_textures': {'building.txd': txd},
            'xbox_vehicles': {'police.dff': b'GTA III police', 'police.txd': txd},
        }
        for archive, entries in self.entries.items():
            sector = 0
            with (self.source / ('models/' + archive + '.img')).open('wb') as img, (self.source / ('models/' + archive + '.dir')).open('wb') as directory:
                for name, data in entries.items():
                    directory.write(struct.pack('<II24s', sector, 1, name.encode()))
                    img.write(data + b'\0' * (2048-len(data)))
                    sector += 1
        (self.source / 'data/maps').mkdir(parents=True)
        (self.source / 'data/maps/generic.ide').write_text('objs\n100, treepatch, trees2, 1, 200, 4\nend\n')
        (self.source / 'models/particle.txd').write_bytes(txd)
        self.pack['files'] = {p.relative_to(self.source).as_posix(): {'bytes': p.stat().st_size, 'sha256': x.sha(p)} for p in self.source.rglob('*') if p.is_file()}
        archive = self.root / 'pack.zip'
        with zipfile.ZipFile(archive, 'w') as z:
            for p in self.source.rglob('*'):
                if p.is_file(): z.write(p, self.pack['sourceFolder'] + '/' + p.relative_to(self.source).as_posix())
            z.writestr(self.pack['sourceFolder'] + '/userfiles/GTA3sf1.b', 'private save')
            z.writestr(self.pack['sourceFolder'] + '/re3.exe', 'never execute')
        self.pack['archiveBytes'] = archive.stat().st_size
        self.pack['archiveSha256'] = x.sha(archive)
        x.PACK.clear(); x.PACK.update(self.pack)
        self.archive = archive

    def tearDown(self):
        x.PACK.clear(); x.PACK.update(self.saved)
        self.temp.cleanup()

    def build(self, name='xbox', **kwargs):
        out = self.root / 'profiles' / name
        x.build(self.source, out, profile=name, **kwargs)
        return out

    def test_profiles_precedence_and_integrity(self):
        for name in ('xbox', 'modern'):
            with self.subTest(profile=name):
                out = self.build(name)
                self.assertTrue(x.valid_overlay(out, name))
                self.assertFalse(x.valid_overlay(out, 'modern' if name == 'xbox' else 'xbox'))
                entries = {n: (s,c) for n,s,c in x.archive_entries(out/'models/gta3.img', out/'models/gta3.dir')}
                data = (out/'models/gta3.img').read_bytes()
                sector,_ = entries['building.dff']
                self.assertTrue(data[sector*2048:].startswith(b'Xbox building'))
                self.assertEqual((out/'vegetation_models.txt').read_text(), 'treepatch\n')
                self.assertEqual((out/'vehicle_models.txt').read_text(), 'police\n')
                p = out/'models/particle.txd'; original=p.read_bytes();p.write_bytes(original+b'corrupt')
                self.assertFalse(x.valid_overlay(out, name));p.write_bytes(original)
                p=out/'unrelated-save.b';p.write_text('private');self.assertFalse(x.valid_overlay(out, name));p.unlink()
                self.assertTrue(x.valid_overlay(out, name))

    def test_allowlist_extraction_and_source_validation(self):
        dest=self.root/'extract';x.extract_archive(self.archive,dest)
        self.assertFalse((dest/'re3.exe').exists());self.assertFalse((dest/'userfiles').exists())
        self.assertEqual(set(x.source_root(dest)[1]), set(self.pack['files']))
        p=dest/'models/particle.txd';p.write_bytes(b'changed')
        with self.assertRaises(ValueError):x.source_root(dest)
        with self.assertRaises(ValueError):x.assert_archive(p)

    def test_failure_preserves_output_and_source(self):
        out=self.build();before={p.relative_to(out):x.sha(p) for p in out.rglob('*') if p.is_file()}
        source_hashes={p.relative_to(self.source):x.sha(p) for p in self.source.rglob('*') if p.is_file()}
        with self.assertRaises((ValueError,OSError)):x.build(self.source,out,True,compressor=self.root/'missing')
        self.assertEqual(before,{p.relative_to(out):x.sha(p) for p in out.rglob('*') if p.is_file()})
        self.assertEqual(source_hashes,{p.relative_to(self.source):x.sha(p) for p in self.source.rglob('*') if p.is_file()})
        self.assertFalse(any(p.name.startswith('.xbox.') for p in out.parent.iterdir()))
        foreign=self.root/'foreign/xbox';foreign.mkdir(parents=True);(foreign/'private').write_text('keep')
        with self.assertRaises(ValueError):x.build(self.source,foreign,True)
        with self.assertRaises(ValueError):x.build(self.source,self.source/'xbox')

    def test_prepare_reuses_only_verified_profile(self):
        out=self.build()
        with mock.patch.object(sys,'argv',['builder','prepare','--out',str(out)]),mock.patch.object(x,'download',side_effect=AssertionError('cache download')):
            self.assertEqual(x.main(),0)
        with mock.patch.object(sys,'argv',['builder','prepare','--out',str(out),'--work-dir',str(self.root/'work'),'--archive',str(self.root/'missing.zip'),'--non-interactive']):
            with self.assertRaises(OSError):x.main()

    def test_download_validates_html_errors_and_fresh_mirror(self):
        page=b'<a href="/downloads/mirror/224588/131/fresh">Download</a>'
        target=self.root/'download.zip'
        with mock.patch.object(x.urllib.request,'urlopen',side_effect=[io.BytesIO(page),io.BytesIO(self.archive.read_bytes())]) as request:
            x.download(target)
            self.assertEqual(target.read_bytes(),self.archive.read_bytes())
            self.assertIn('/224588/131/fresh',request.call_args_list[1].args[0])
        target=self.root/'bad.zip'
        with mock.patch.object(x.urllib.request,'urlopen',side_effect=[io.BytesIO(page),io.BytesIO(b'<html>blocked</html>')]):
            with self.assertRaises(ValueError):x.download(target)
            self.assertFalse(target.exists())
        with mock.patch.object(x.urllib.request,'urlopen',return_value=io.BytesIO(b'no mirror')):
            with self.assertRaises(ValueError):x.download(target)

    def test_prompt_is_a_flushed_line_before_reading_input(self):
        output = io.StringIO()
        def answer():
            self.assertEqual(output.getvalue(), 'Choose a local archive:\n')
            return 'fixture.zip'
        with contextlib.redirect_stdout(output), mock.patch('builtins.input', side_effect=answer):
            self.assertEqual(x.prompt('Choose a local archive:'), 'fixture.zip')

    def test_long_hash_reports_completion(self):
        payload = self.root / 'large-fixture.bin'
        with payload.open('wb') as stream:
            stream.truncate(64 * 1024 * 1024)
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            digest = x.sha(payload, 'Checking fixture')
        self.assertEqual(digest, x.sha(payload))
        self.assertIn('Checking fixture: 100%', output.getvalue())

if __name__ == '__main__':
    unittest.main()
