"""Regression check for opaque C888 conversion; old vehicle-pack selection cases are retired."""
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class XboxTextures(unittest.TestCase):
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
