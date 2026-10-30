#!/usr/bin/env python3
"""Copy app-owned RenderDoc captures locally without exposing device identifiers."""
import argparse
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import sys

NAME = re.compile(r"[0-9]{8}T[0-9]{6}Z-[0-9]+(?:_[A-Za-z0-9-]+)*\.rdc\Z")
BASE = "content://com.libertyvr.quest.performance/"


def run(adb, args, timeout=30):
    result = subprocess.run([str(adb), *args], stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=timeout)
    if result.returncode:
        raise RuntimeError("ADB operation failed; private diagnostics suppressed.")
    return result.stdout


def find_device(adb):
    eligible = []
    for line in run(adb, ["devices", "-l"]).decode(errors="replace").splitlines():
        fields = line.split()
        if len(fields) > 1 and fields[1] == "device":
            try:
                model = run(adb, ["-s", fields[0], "shell", "getprop", "ro.product.model"])
            except RuntimeError:
                continue
            if model.strip() == b"Quest 3":
                eligible.append(fields[0])
    if len(eligible) != 1:
        raise RuntimeError("Connect exactly one authorized, responsive personal Quest 3 device.")
    return eligible[0]  # Local selection only; never printed or persisted.


def listing(data):
    entries = {}
    for line in data.decode(errors="replace").splitlines():
        name = re.search(r"_display_name=([^,]+)", line)
        size = re.search(r"_size=([0-9]+)", line)
        if name and size and NAME.fullmatch(name[1]) and len(name[1]) <= 192:
            entries[name[1]] = int(size[1])
    return entries


def save(output, name, expected_size, data):
    if not NAME.fullmatch(name) or len(name) > 192:
        raise ValueError("Invalid capture name.")
    if expected_size < 16 or len(data) != expected_size or data[:4] != b"RDOC":
        raise ValueError("Incomplete or invalid transfer; no destination file was written.")
    digest = hashlib.sha256(data).hexdigest()
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    target = output / name
    if target.exists():
        if hashlib.sha256(target.read_bytes()).hexdigest() != digest:
            raise ValueError("Destination already exists with different contents; left unchanged.")
    else:
        # Exclusive creation never replaces an existing capture or placeholder.
        with target.open("xb") as stream:
            stream.write(data)
    return target, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--adb", type=Path, help="Path to adb if not on PATH")
    parser.add_argument("--output", required=True, type=Path, help="Local output directory")
    parser.add_argument("--latest", type=int, default=1, help="Number of newest captures (default 1)")
    parser.add_argument("--name", help="Export one exact listed capture instead")
    args = parser.parse_args()
    if args.latest < 1 or (args.name and not NAME.fullmatch(args.name)):
        parser.error("Use a positive capture count and a valid capture filename.")
    adb = args.adb or shutil.which("adb")
    if not adb:
        parser.error("ADB unavailable; provide --adb.")
    try:
        device = find_device(adb)
        entries = listing(run(adb, ["-s", device, "shell", "content", "query", "--uri", BASE+"renderdocs"]))
        names = [args.name] if args.name else sorted(entries, reverse=True)[:args.latest]
        if not names or any(name not in entries for name in names):
            raise RuntimeError("No matching capture listed; install the capture-export update and retry.")
        for name in names:
            data = run(adb, ["-s", device, "exec-out", "content", "read", "--uri", BASE+"renderdoc/"+name], 120)
            path, digest = save(args.output, name, entries[name], data)
            print(f"Saved {path.name}: {len(data)} bytes; RDOC signature and size verified; SHA256 {digest}")
        return 0
    except (RuntimeError, ValueError) as error:
        print(str(error), file=sys.stderr)
    except (OSError, subprocess.SubprocessError):
        print("Export failed; private device identifiers and raw diagnostics suppressed.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
