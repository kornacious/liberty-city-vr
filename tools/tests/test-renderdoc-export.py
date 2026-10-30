import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("exporter", Path(__file__).parents[1]/"export-renderdoc-capture.py")
exporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exporter)


class ExportTests(unittest.TestCase):
    name = "20261027T001530Z-123456789_capture.rdc"

    def test_listing_rejects_paths_and_unrelated_files(self):
        text = f"Row: 0 _display_name={self.name}, _size=42\nRow: 1 _display_name=../../gta3.img, _size=12\n"
        self.assertEqual(exporter.listing(text.encode()), {self.name: 42})

    def test_invalid_transfer_never_creates_a_placeholder(self):
        with tempfile.TemporaryDirectory() as directory:
            for data, size in [(b"", 100), (b"permission denied", 17), (b"RDOC"+b"x"*20, 25)]:
                with self.assertRaises(ValueError):
                    exporter.save(directory, self.name, size, data)
            self.assertFalse((Path(directory)/self.name).exists())
            with self.assertRaises(ValueError):
                exporter.save(directory, "../"+self.name, 24, b"RDOC"+b"x"*20)

    def test_exclusive_write_and_existing_file_protection(self):
        with tempfile.TemporaryDirectory() as directory:
            data = b"RDOC"+b"x"*20
            path, digest = exporter.save(directory, self.name, len(data), data)
            self.assertEqual(path.read_bytes(), data)
            self.assertEqual(exporter.save(directory, self.name, len(data), data)[1], digest)
            with self.assertRaises(ValueError):
                exporter.save(directory, self.name, len(data), b"RDOC"+b"y"*20)
            self.assertEqual(path.read_bytes(), data)


if __name__ == "__main__":
    unittest.main()
