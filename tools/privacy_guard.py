#!/usr/bin/env python3
"""Reject device identifiers and local private values without printing them."""

import argparse
from pathlib import Path
import re
import subprocess
import sys


RULES = (
    ("Quest device identifier", re.compile(rb"(?<![A-Za-z0-9])2G[A-Z0-9]{12}(?![A-Za-z0-9])", re.I)),
    ("private Tailscale hostname", re.compile(rb"\b[A-Za-z0-9.-]+\.ts\.net\b", re.I)),
)
OID = re.compile(r"[0-9a-f]{40}(?:[0-9a-f]{24})?\Z")


class PrivacyError(Exception):
    pass


def git(*args, data=None):
    result = subprocess.run(
        ["git", *args], input=data, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    if result.returncode:
        raise PrivacyError("Git operation failed; diagnostics suppressed for privacy.")
    return result.stdout


def denylist():
    path = Path(git("rev-parse", "--git-path", "info/privacy-denylist").decode().strip())
    if not path.exists():
        return ()
    return tuple(line for line in path.read_bytes().splitlines() if line)


def check(data, private_values):
    for label, pattern in RULES:
        if pattern.search(data):
            raise PrivacyError("Blocked sensitive data: " + label + ". Matched text suppressed.")
    if any(value.lower() in data.lower() for value in private_values):
        raise PrivacyError("Blocked a locally configured private value. Matched text suppressed.")


def scan_objects(object_ids, private_values):
    """Inspect complete blobs and commit/tag metadata, including binary blobs."""
    process = subprocess.Popen(
        ["git", "cat-file", "--batch"], stdin=subprocess.PIPE,
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL
    )
    try:
        for object_id in sorted(set(object_ids)):
            if not OID.fullmatch(object_id):
                raise PrivacyError("Invalid object reference; scan stopped.")
            process.stdin.write((object_id + "\n").encode())
            process.stdin.flush()
            header = process.stdout.readline().split()
            if len(header) != 3 or header[1] not in (b"blob", b"commit", b"tag", b"tree"):
                raise PrivacyError("Unable to read a Git object; scan stopped.")
            size = int(header[2])
            content = process.stdout.read(size)
            if len(content) != size or process.stdout.read(1) != b"\n":
                raise PrivacyError("Incomplete Git object; scan stopped.")
            # Tree data covers every historical filename, even when rev-list
            # deduplicates a blob reused under a different name.
            check(content, private_values)
    finally:
        process.stdin.close()
        process.stdout.close()
        if process.poll() is None:
            process.terminate()
        process.wait()


def staged(private_values):
    objects = []
    for record in git("ls-files", "--stage", "-z").split(b"\0"):
        if not record:
            continue
        metadata, path = record.split(b"\t", 1)
        mode, object_id, stage = metadata.split()
        if stage != b"0":
            raise PrivacyError("Unmerged index; privacy scan stopped.")
        check(path, private_values)
        if mode != b"160000":
            objects.append(object_id.decode())
    scan_objects(objects, private_values)


def history(private_values, tips=None):
    if tips is None:
        objects = git("rev-list", "--objects", "--all")
    elif not tips:
        return
    else:
        objects = git("rev-list", "--objects", "--stdin", data=("\n".join(tips) + "\n").encode())
    ids = []
    for line in objects.splitlines():
        object_id, _, path = line.partition(b" ")
        check(path, private_values)
        ids.append(object_id.decode())
    scan_objects(ids, private_values)


def pre_push(private_values, lines):
    tips = []
    for line in lines:
        fields = line.split()
        if len(fields) != 4:
            raise PrivacyError("Invalid push input; privacy scan stopped.")
        local_ref, local_oid, remote_ref, remote_oid = fields
        if not OID.fullmatch(local_oid) or not OID.fullmatch(remote_oid):
            raise PrivacyError("Invalid push object; privacy scan stopped.")
        if set(local_oid) == {"0"}:
            continue
        if not remote_ref.startswith(("refs/heads/", "refs/tags/")):
            raise PrivacyError("Publishing local checkpoint or other private refs is blocked.")
        check((local_ref + " " + remote_ref).encode(), private_values)
        tips.append(local_oid)
    history(private_values, tips)


def install_hooks():
    root = Path(git("rev-parse", "--show-toplevel").decode().strip())
    for hook in ("pre-commit", "commit-msg", "pre-push"):
        if not (root / ".githooks" / hook).is_file():
            raise PrivacyError("Tracked privacy hooks are missing; installation stopped.")
    existing = subprocess.run(
        ["git", "config", "--get", "core.hooksPath"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    if existing.returncode not in (0, 1):
        raise PrivacyError("Unable to inspect hook configuration.")
    if existing.stdout.strip() not in (b"", b".githooks"):
        raise PrivacyError("An existing custom hook path requires review before installation.")
    git("config", "--local", "privacy.python", sys.executable)
    git("config", "--local", "core.hooksPath", ".githooks")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("staged", "message", "history", "pre-push", "install-hooks"))
    parser.add_argument("message_file", nargs="?")
    args = parser.parse_args(argv)
    try:
        if args.mode == "install-hooks":
            install_hooks()
        else:
            values = denylist()
            if args.mode == "staged":
                staged(values)
            elif args.mode == "message":
                if not args.message_file:
                    raise PrivacyError("A commit message file is required.")
                check(Path(args.message_file).read_bytes(), values)
            elif args.mode == "history":
                history(values)
            else:
                pre_push(values, sys.stdin.read().splitlines())
    except (PrivacyError, OSError, ValueError):
        # Never include exception text: a filename, Git error or value may be private.
        print("Privacy check failed. Sensitive content and diagnostic values are suppressed.", file=sys.stderr)
        return 1
    print("Privacy check passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
