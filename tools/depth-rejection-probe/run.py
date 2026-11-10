"""Deploy/run only the generated-geometry APK; keep device identifiers local.

Uses only standard Python for an unhooked run. --renderdoc additionally uses
Meta's installed renderdoc_mcp package, with all vendor output suppressed.
"""
import argparse
import contextlib
import io
import json
import logging
import os
from pathlib import Path
import subprocess
import sys
import time

PACKAGE = 'com.libertyvr.depthprobe'
CASES = ('open', 'occluded', 'no_depth_write')


class DiagnosticFailure(Exception):
    """Only locally authored, identifier-free error messages."""


def run(args):
    adb = Path(args.sdk).resolve() / 'platform-tools/adb.exe'

    def command(*values, timeout=30):
        return subprocess.run([str(adb), *values], capture_output=True, timeout=timeout)

    eligible = []
    for attempt in range(3):
        discovery = command('devices', '-l').stdout.decode(errors='replace')
        eligible = []
        for line in discovery.splitlines()[1:]:
            fields = line.split()
            if len(fields) < 2 or fields[1] != 'device':
                continue
            candidate = fields[0]
            model = command('-s', candidate, 'shell', 'getprop', 'ro.product.model')
            if model.stdout.strip() == b'Quest 3':
                eligible.append(candidate)
        if eligible:
            break
        if attempt < 2:
            time.sleep(1)
    if len(eligible) != 1:
        raise DiagnosticFailure('Exactly one authorized Quest 3 is required; installation skipped.')
    target = eligible[0]

    def device(*values, timeout=30):
        return command('-s', target, *values, timeout=timeout)

    installed = device('install', '-r', str(Path(args.apk).resolve()), timeout=60)
    if installed.returncode or b'Success' not in installed.stdout:
        raise DiagnosticFailure('Diagnostic update failed; no application was uninstalled.')
    device('shell', 'am', 'force-stop', PACKAGE)
    device('shell', 'run-as', PACKAGE, 'mkdir', '-p', 'files')
    device('shell', 'run-as', PACKAGE, 'rm', '-f', 'files/results.json', 'files/error.txt')
    if args.transition:
        device('shell', 'run-as', PACKAGE, 'touch', 'files/transition.flag')
    else:
        device('shell', 'run-as', PACKAGE, 'rm', '-f', 'files/transition.flag')
    try:
        if args.renderdoc:
            logging.disable(logging.CRITICAL)
            install = Path(args.renderdoc).resolve()
            sys.path.insert(0, str(install / 'MCP'))
            os.environ['RENDERDOC_CMD_PATH'] = str(install / 'renderdoccmd.exe')
            os.environ['PATH'] = str(adb.parent) + os.pathsep + os.environ.get('PATH', '')
            for key in ('HTTP_PROXY', 'HTTPS_PROXY', 'ALL_PROXY'):
                os.environ[key] = 'http://127.0.0.1:9'
            os.environ['NO_PROXY'] = '127.0.0.1,localhost,::1'
            # Vendor diagnostics may include device arguments; never emit them.
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                from renderdoc_mcp import renderdoc_api
                launched = renderdoc_api.adb_launch_drawcall_profiling(
                    package=PACKAGE, device=target, activity=PACKAGE + '.DepthProbeActivity')
            if not launched.get('success'):
                raise DiagnosticFailure('RenderDoc diagnostic launch failed; vendor output withheld.')
        else:
            launched = device('shell', 'am', 'start', '-W', '-n', PACKAGE + '/.DepthProbeActivity')
            if launched.returncode:
                raise DiagnosticFailure('Diagnostic launch failed.')
        deadline = time.monotonic() + 120
        report = None
        while time.monotonic() < deadline:
            value = device('exec-out', 'run-as', PACKAGE, 'cat', 'files/results.json', timeout=10)
            try:
                report = json.loads(value.stdout)
            except (ValueError, TypeError):
                pass
            if report:
                break
            error = device('exec-out', 'run-as', PACKAGE, 'cat', 'files/error.txt', timeout=10)
            if error.stdout.startswith(b'Depth control failed:'):
                raise DiagnosticFailure('Native diagnostic failed; inspect its local error.txt.')
            time.sleep(1)
        if not report or report.get('synthetic') is not True:
            raise DiagnosticFailure('No valid synthetic report within the bounded wait.')
        if report.get('suite') != ('transition' if args.transition else 'baseline'):
            raise DiagnosticFailure('Diagnostic suite did not match the requested run.')
        destination = Path(args.output).resolve()
        destination.mkdir(parents=True, exist_ok=True)
        (destination / 'results.json').write_text(json.dumps(report, indent=2))
        exported = 0
        cases = tuple('transition_b%d_w%d_k%d' % ((v >> 1) & 1, v & 1, (v >> 2) & 1) for v in range(8)) if args.transition else CASES
        if args.renderdoc:
            for case in cases:
                name = 'depth-control-' + case + '_capture.rdc'
                data = device('exec-out', 'run-as', PACKAGE, 'cat', 'files/' + name)
                if data.stdout[:4] == b'RDOC':
                    (destination / name).write_bytes(data.stdout)
                    exported += 1
        print(json.dumps({'target': 'personal Quest 3 device', 'pixels_pass': report.get('pixels_pass'),
                          'renderdoc_attached': report.get('renderdoc_attached'),
                          'synthetic_captures_exported': exported, 'local_results': str(destination)}))
        return 0 if report.get('pixels_pass') and (not args.renderdoc or exported == len(cases)) else 1
    finally:
        device('shell', 'am', 'force-stop', PACKAGE)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', required=True)
    parser.add_argument('--apk', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--transition', action='store_true', help='Run the eight-case D24S8 blend/write/kill transition suite')
    parser.add_argument('--renderdoc', help='Meta installation directory; close its Quest replay context first')
    options = parser.parse_args()
    try:
        sys.exit(run(options))
    except DiagnosticFailure as exc:
        print(str(exc), file=sys.stderr)
        sys.exit(1)
    except Exception as exc:
        # Subprocess exceptions can contain private command arguments.
        print('Diagnostic failed (' + type(exc).__name__ + '); raw command output withheld.', file=sys.stderr)
        sys.exit(1)
