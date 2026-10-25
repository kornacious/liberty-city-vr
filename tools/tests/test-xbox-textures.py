"""Regression checks for source selection and opaque C888 texture conversion."""
import importlib.util
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('xbox', ROOT / 'tools/modelsets/xbox-modelset.py')
xbox = importlib.util.module_from_spec(spec)
spec.loader.exec_module(xbox)


class XboxTextures(unittest.TestCase):
    def test_pack_variants_and_emergency_pairs(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            main = root / 'Fixed Xbox Vehicles/gta3.img'
            main.mkdir(parents=True)
            for name in ('police', 'ambulan', 'enforcer', 'firetruk', 'chopper', 'predator', 'taxi'):
                for ext in ('dff', 'txd'):
                    (main / f'{name}.{ext}').write_bytes(b'policemiami' if name == 'police' else b'original')
            optional = root / 'Taxi and Police models with badges'
            optional.mkdir()
            (optional / 'taxi.dff').write_bytes(b'wrong variant')
            files = {p.name: p for p in xbox.compatible_source_files(root)}
            self.assertEqual(set(files), {'predator.dff', 'predator.txd', 'taxi.dff', 'taxi.txd'})
            self.assertEqual(files['taxi.dff'].read_bytes(), b'original')

    def test_gta3_source_retained_and_duplicates_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'police.txd').write_bytes(b'GTA III')
            self.assertEqual(len(xbox.compatible_source_files(root)), 1)
            (root / 'variant').mkdir()
            (root / 'variant/police.txd').write_bytes(b'different')
            with self.assertRaisesRegex(ValueError, 'Ambiguous'):
                xbox.compatible_source_files(root)

    def test_c888_unused_alpha_is_opaque(self):
        def chunk(kind, data):
            return struct.pack('<III', kind, len(data), 0x1803ffff) + data
        compressor = ROOT / 'tools/modelsets/txdcompress.exe'
        with tempfile.TemporaryDirectory() as tmp:
            for fmt, expected in ((0x600, 1), (0x500, 5)):
                header = struct.pack('<II32s32sIIHHBBBB', 8, 0, b'boat', b'', fmt, 0,
                                     8, 8, 32, 1, 4, 0)
                pixels = bytes([64, 128, 192, 0]) * 64
                native = chunk(1, header + struct.pack('<I', len(pixels)) + pixels)
                data = chunk(22, chunk(1, struct.pack('<HH', 1, 2)) + chunk(21, native))
                path = Path(tmp) / 'boat.txd'
                path.write_bytes(data)
                subprocess.run([str(compressor), str(path)], check=True, capture_output=True)
                output = path.read_bytes()
                # TXD header, dictionary STRUCT, native header, native STRUCT header.
                start = 12 + 16 + 12 + 12
                self.assertEqual(output[start + 87], expected)
                self.assertEqual(struct.unpack_from('<I', output, start + 76)[0], expected == 5)


if __name__ == '__main__':
    unittest.main()
