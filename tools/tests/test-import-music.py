#!/usr/bin/env python3
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('import_music',root/'tools/import-music.py')
module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module)

class ImportTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.path=Path(self.temp.name)/'_Tést & song.mp3'; self.path.write_bytes(b'generated test audio placeholder')
    def test_safe_name_and_content_hash(self):
        sha=module.digest(self.path); name=module.safe_name(self.path,sha)
        self.assertRegex(name,r'^[A-Za-z0-9][A-Za-z0-9_.-]{0,119}\.mp3$')
        self.assertNotIn('&',name); self.assertIn(sha[:16],name)
        self.assertEqual(sha,module.digest(self.path))
    def test_probe_rejects_unsupported_streams(self):
        good={'streams':[{'codec_type':'audio','codec_name':'mp3','channels':2,'sample_rate':'44100'}], 'format':{'duration':'10.2'}}
        with patch.object(module,'run',return_value=subprocess.CompletedProcess([],0,json.dumps(good))):
            self.assertEqual(module.probe(self.path,'ffprobe')[1],10200)
        good['streams'][0]['channels']=6
        with patch.object(module,'run',return_value=subprocess.CompletedProcess([],0,json.dumps(good))):
            with self.assertRaisesRegex(ValueError,'mono/stereo'): module.probe(self.path,'ffprobe')
    def test_dry_run_never_touches_headset(self):
        with patch.object(module,'probe',return_value=(32,10000)),patch.object(module.shutil,'which',return_value='ffprobe'),patch.object(module,'choose_device',side_effect=AssertionError('Headset accessed')),contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(module.main(['--dry-run',str(self.path)]),0)
    def workflow(self,fail=False,remote=None):
        calls=[]; sha=module.digest(self.path); name=module.safe_name(self.path,sha)
        def provider(adb,method,arg=None):
            calls.append((method,arg))
            if method=='info': return remote or {'count':0,'free':10*module.RESERVE,'duration':0,'names':[]}
            if method=='check': return {'exists':True}
            if method=='commit':
                if fail: raise RuntimeError('Transfer checksum mismatch')
                return {'bytes':self.path.stat().st_size}
            return {'ok':True}
        with patch.object(module,'probe',return_value=(self.path.stat().st_size,10000)),patch.object(module.shutil,'which',return_value='ffprobe'),patch.object(module,'choose_device',return_value=['adb','-s','TEST']),patch.object(module,'provider',side_effect=provider),patch.object(module,'run',return_value=subprocess.CompletedProcess([],0,b'')),contextlib.redirect_stdout(io.StringIO()):
            if fail:
                with self.assertRaisesRegex(RuntimeError,'checksum'): module.main([str(self.path)])
            else: self.assertEqual(module.main([str(self.path)]),0)
        return calls,name
    def test_success_checks_commit_size_and_digest(self):
        calls,name=self.workflow()
        self.assertIn(('commit',name+':'+module.digest(self.path)),calls)
        self.assertNotIn(('abort',name),calls)
    def test_failed_upload_is_aborted(self):
        calls,name=self.workflow(fail=True); self.assertIn(('abort',name),calls)
    def test_full_library_can_be_reimported_without_upload(self):
        sha=module.digest(self.path); name=module.safe_name(self.path,sha)
        calls,_=self.workflow(remote={'count':1000,'free':10*module.RESERVE,'duration':module.MAX_DURATION,'names':[name]})
        self.assertEqual([c[0] for c in calls],['info','check'])
    def test_reject_wrong_device(self):
        results=[subprocess.CompletedProcess([],0,'List of devices attached\nTEST device model:Phone\n'),subprocess.CompletedProcess([],0,'Phone')]
        with patch.object(module,'run',side_effect=results):
            with self.assertRaisesRegex(RuntimeError,'Quest'): module.choose_device('adb',None)

if __name__=='__main__': unittest.main()
