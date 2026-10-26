#!/usr/bin/env python3
"""Exercise the actual PowerShell-to-Python interactive prompt pipeline."""
import argparse
import os
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--powershell', default='powershell.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='modelset-prompt-') as directory:
    work = Path(directory)
    game = work / 'game'
    (game / 'models').mkdir(parents=True)
    for name in ('gta3.img', 'gta3.dir'):
        (game / 'models' / name).write_text('untouched fixture')
    env = dict(os.environ)
    env['PATH'] = str(Path(sys.executable).parent) + os.pathsep + env.get('PATH', '')
    command = [args.powershell, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
               str(root / 'tools/prepare-modern-models.ps1'), '-GameDir', str(game),
               '-WorkDir', str(work / 'cache'), '-BuildOnly', '-LogPath', str(work / 'prepare.log')]
    process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, env=env)
    lines = queue.Queue()
    def read_lines():
        for line in process.stdout:
            lines.put(line)
        lines.put(None)
    reader = threading.Thread(target=read_lines, daemon=True)
    reader.start()
    received = []
    def await_line(text):
        deadline = time.monotonic() + 25
        while time.monotonic() < deadline:
            try:
                line = lines.get(timeout=max(0.01, deadline-time.monotonic()))
            except queue.Empty:
                break
            if line is None:
                break
            received.append(line)
            if text in line:
                return
        raise AssertionError('Expected visible prompt before sending input: ' + text + '\n' + ''.join(received))
    try:
        await_line('Download Xbox Version HD (re3), 1.45 GB, from ModDB? [y/N]')
        process.stdin.write('n\n'); process.stdin.flush()
        await_line('Path to III_Xbox_Version_HD_Supports_re3.zip:')
        process.stdin.write(str(work / 'intentionally-missing.zip') + '\n'); process.stdin.flush()
        assert process.wait(timeout=15) == 1, 'Missing archive should fail with a nonzero status'
        reader.join(timeout=5)
        assert all(p.read_text() == 'untouched fixture' for p in (game / 'models').iterdir())
        assert not (game / 'modelsets').exists()
        print('PASS both production prompts were visible before input; failure preserved game files; no network or ADB')
    finally:
        if process.poll() is None:
            process.kill(); process.wait(timeout=5)
        process.stdin.close()
        reader.join(timeout=5)
        process.stdout.close()
