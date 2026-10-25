#!/usr/bin/env python3
"""Check patch bytes and apply the full series to the pinned LF upstream tree."""

import argparse
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest


BASE = "b050e1cc63b1869e6b514ab2f1a00ef4969e0c84"
KIT = Path(__file__).resolve().parents[2]


def git(*args):
    return subprocess.run(["git", *map(str, args)], check=True, capture_output=True)


class PatchSeriesTests(unittest.TestCase):
    def test_patch_line_endings(self):
        patches = sorted((KIT / "patches").glob("*.patch"))
        self.assertTrue(patches)
        for patch in patches:
            with self.subTest(patch=patch.name):
                self.assertTrue(b"\r" not in patch.read_bytes(), "Patches must use LF")

    def test_clean_lf_upstream(self):
        # Export only tracked source at the pinned revision; never copy local
        # game data, edits, or Git metadata. Override Windows autocrlf so this
        # exercises exactly the source bytes used by a clean Linux checkout.
        with tempfile.TemporaryDirectory(prefix="lcvr-patches-") as directory:
            root = Path(directory)
            archive = root / "base.tar"
            git("-C", RE3, "-c", "core.autocrlf=false", "archive",
                "--format=tar", f"--output={archive}", BASE)
            source = root / "source"
            source.mkdir()
            with tarfile.open(archive) as tree:
                tree.extractall(source, filter="data")
            git("-C", source, "init", "-q")
            git("-C", source, "config", "core.autocrlf", "false")
            for patch in sorted((KIT / "patches").glob("*.patch")):
                try:
                    git("-C", source, "apply", "--whitespace=nowarn", patch)
                except subprocess.CalledProcessError as error:
                    # Stop here: later patches depend on earlier ones.
                    self.fail(f"{patch.name}: {error.stderr.decode(errors='replace')}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("re3", type=Path, help="Local clone containing the pinned re3 commit")
    args = parser.parse_args()
    RE3 = args.re3.resolve()
    unittest.main(argv=[__file__])
