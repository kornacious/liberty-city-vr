#!/usr/bin/env python3
"""Exercise privacy checks against disposable repositories with synthetic data."""

import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
GUARD = ROOT / "tools/privacy_guard.py"
DEVICE = b"2G" + b"X" * 12
HOST = b"fixture." + b"example.ts" + b".net"
SPEC = importlib.util.spec_from_file_location("privacy_guard", GUARD)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class PrivacyGuardTests(unittest.TestCase):
    def setUp(self):
        cache = ROOT / ".cache"
        cache.mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="privacy-tests-", dir=cache)
        self.repo = Path(self.temp.name)
        self.env = os.environ.copy()
        # Isolate fixtures from the developer's Git configuration and identity.
        self.env["GIT_CONFIG_NOSYSTEM"] = "1"
        self.env["GIT_CONFIG_GLOBAL"] = os.devnull
        self.env["GIT_AUTHOR_NAME"] = self.env["GIT_COMMITTER_NAME"] = "Privacy Test"
        self.env["GIT_AUTHOR_EMAIL"] = self.env["GIT_COMMITTER_EMAIL"] = "test@example.invalid"
        self.run_git("init", "-q")
        self.run_git("config", "commit.gpgSign", "false")
        (self.repo / "safe.txt").write_text("safe\n")
        self.run_git("add", "safe.txt")
        self.run_git("commit", "-q", "-m", "Safe fixture")

    def tearDown(self):
        def writable_remove(function, path, error):
            os.chmod(path, 0o700)
            function(path)
        shutil.rmtree(self.repo, onerror=writable_remove)
        self.temp.cleanup()

    def command(self, args, data=None):
        return subprocess.run(args, input=data, cwd=self.repo, env=self.env,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def run_git(self, *args, data=None):
        result = self.command(["git", *args], data)
        self.assertEqual(result.returncode, 0, "Fixture Git operation failed")
        return result.stdout

    def guard(self, *args, data=None):
        return self.command([sys.executable, str(GUARD), *args], data)

    def blocked(self, result, private=DEVICE):
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn(private, result.stdout + result.stderr)
        self.assertNotIn(b"Traceback", result.stdout + result.stderr)

    def push_input(self, oid=None, remote_ref="refs/heads/main"):
        oid = oid or self.run_git("rev-parse", "HEAD").decode().strip()
        return ("refs/heads/main " + oid + " " + remote_ref + " " + "0" * 40 + "\n").encode()

    def install(self):
        (self.repo / "tools").mkdir()
        shutil.copy2(GUARD, self.repo / "tools/privacy_guard.py")
        shutil.copytree(ROOT / ".githooks", self.repo / ".githooks")
        for hook in (self.repo / ".githooks").iterdir():
            hook.chmod(0o755)
        result = self.guard("install-hooks")
        self.assertEqual(result.returncode, 0)

    def test_clean_tree_message_history_and_push(self):
        for mode in ("staged", "history"):
            self.assertEqual(self.guard(mode).returncode, 0)
        message = self.repo / "message.txt"
        message.write_text("Installed on personal Quest 3 device\n")
        self.assertEqual(self.guard("message", str(message)).returncode, 0)
        self.assertEqual(self.guard("pre-push", data=self.push_input()).returncode, 0)

    def test_staged_blob_is_checked_even_if_working_file_was_cleaned(self):
        path = self.repo / "safe.txt"
        path.write_bytes(DEVICE)
        self.run_git("add", "safe.txt")
        path.write_text("clean working copy\n")
        self.blocked(self.guard("staged"))

    def test_unstaged_data_is_not_part_of_commit(self):
        (self.repo / "safe.txt").write_bytes(DEVICE)
        self.assertEqual(self.guard("staged").returncode, 0)

    def test_commit_message_is_blocked_without_echo(self):
        message = self.repo / "message.txt"
        message.write_bytes(b"Device " + DEVICE)
        self.blocked(self.guard("message", str(message)))

    def test_deleted_historical_data_still_blocks_push(self):
        path = self.repo / "removed.bin"
        path.write_bytes(b"\x00" + DEVICE + b"\xff")
        self.run_git("add", "removed.bin")
        self.run_git("commit", "-q", "-m", "Synthetic binary fixture")
        self.run_git("rm", "removed.bin")
        self.run_git("commit", "-q", "-m", "Remove synthetic fixture")
        self.blocked(self.guard("history"))
        self.blocked(self.guard("pre-push", data=self.push_input()))

    def test_old_commit_message_is_checked(self):
        message = self.repo / "message.txt"
        message.write_bytes(b"Synthetic " + DEVICE)
        self.run_git("commit", "-q", "--allow-empty", "-F", str(message))
        self.blocked(self.guard("pre-push", data=self.push_input()))

    def test_annotated_tag_message_is_checked(self):
        message = self.repo / "message.txt"
        message.write_bytes(b"Synthetic " + DEVICE)
        self.run_git("tag", "-a", "fixture", "-F", str(message))
        oid = self.run_git("rev-parse", "refs/tags/fixture").decode().strip()
        self.blocked(self.guard("pre-push", data=self.push_input(oid, "refs/tags/fixture")))

    def test_private_hostname_is_blocked(self):
        (self.repo / "safe.txt").write_bytes(HOST)
        self.run_git("add", "safe.txt")
        self.blocked(self.guard("staged"), HOST)

    def test_private_filename_is_blocked_without_echo(self):
        name = DEVICE.decode() + ".txt"
        (self.repo / name).write_text("safe\n")
        self.run_git("add", name)
        self.blocked(self.guard("staged"))

    def test_author_metadata_is_checked(self):
        self.env["GIT_AUTHOR_NAME"] = DEVICE.decode()
        self.run_git("commit", "-q", "--allow-empty", "-m", "Synthetic author fixture")
        self.blocked(self.guard("pre-push", data=self.push_input()))

    def test_lowercase_device_value_is_blocked(self):
        (self.repo / "safe.txt").write_bytes(DEVICE.lower())
        self.run_git("add", "safe.txt")
        self.blocked(self.guard("staged"), DEVICE.lower())

    def test_old_filename_with_reused_clean_blob_is_checked(self):
        name = DEVICE.decode() + ".txt"
        shutil.copyfile(self.repo / "safe.txt", self.repo / name)
        self.run_git("add", name)
        self.run_git("commit", "-q", "-m", "Synthetic filename fixture")
        self.run_git("rm", name)
        self.run_git("commit", "-q", "-m", "Remove synthetic filename")
        self.blocked(self.guard("pre-push", data=self.push_input()))

    def test_local_denylist_is_case_insensitive_and_not_echoed(self):
        value = b"synthetic-" + b"private-value"
        (self.repo / ".git/info/privacy-denylist").write_bytes(value + b"\n")
        (self.repo / "safe.txt").write_bytes(value.upper())
        self.run_git("add", "safe.txt")
        self.blocked(self.guard("staged"), value.upper())

    def test_private_checkpoint_namespace_is_blocked(self):
        self.blocked(self.guard("pre-push", data=self.push_input(remote_ref="refs/codex/fixture")))

    def test_push_checks_only_outgoing_history(self):
        safe = self.run_git("rev-parse", "HEAD").decode().strip()
        message = self.repo / "message.txt"
        message.write_bytes(DEVICE)
        self.run_git("commit", "-q", "--allow-empty", "-F", str(message))
        self.run_git("update-ref", "refs/codex/fixture", "HEAD")
        self.run_git("reset", "--hard", safe)
        self.assertEqual(self.guard("pre-push", data=self.push_input()).returncode, 0)
        self.blocked(self.guard("history"))

    def test_ref_deletion_does_not_publish_history(self):
        line = "(delete) " + "0" * 40 + " refs/heads/old " + "a" * 40 + "\n"
        self.assertEqual(self.guard("pre-push", data=line.encode()).returncode, 0)

    def test_malformed_input_and_git_failure_are_closed(self):
        self.blocked(self.guard("pre-push", data=b"invalid\n"))
        with self.assertRaises(MODULE.PrivacyError):
            MODULE.git("not-a-real-git-command")

    def test_existing_custom_hook_path_is_preserved(self):
        self.run_git("config", "core.hooksPath", "custom-hooks")
        (self.repo / ".githooks").mkdir()
        for name in ("pre-commit", "commit-msg", "pre-push"):
            (self.repo / ".githooks" / name).write_text("#!/bin/sh\n")
        self.blocked(self.guard("install-hooks"))
        self.assertEqual(self.run_git("config", "core.hooksPath").strip(), b"custom-hooks")

    def test_installed_git_hooks_block_real_commit_operations(self):
        self.install()
        (self.repo / "safe.txt").write_bytes(DEVICE)
        self.run_git("add", "safe.txt")
        self.blocked(self.command(["git", "commit", "-m", "Safe message"]))
        (self.repo / "safe.txt").write_text("safe update\n")
        self.run_git("add", "safe.txt")
        message = self.repo / "message.txt"
        message.write_bytes(DEVICE)
        self.blocked(self.command(["git", "commit", "-F", str(message)]))
        self.run_git("commit", "-q", "-m", "Safe update")
        self.assertEqual(self.guard("pre-push", data=self.push_input()).returncode, 0)

    def test_installed_push_hook_blocks_historical_message(self):
        message = self.repo / "message.txt"
        message.write_bytes(DEVICE)
        self.run_git("commit", "-q", "--allow-empty", "-F", str(message))
        self.install()
        remote = self.repo / "remote.git"
        self.run_git("init", "--bare", "-q", str(remote))
        result = self.command(["git", "push", str(remote), "HEAD:refs/heads/main"])
        self.blocked(result)
        refs = self.run_git("--git-dir=" + str(remote), "for-each-ref")
        self.assertEqual(refs, b"")


if __name__ == "__main__":
    unittest.main()
