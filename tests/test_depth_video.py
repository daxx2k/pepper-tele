import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'pepper'/'bridge'))
from depth_video import render_depth


class DepthDisplayTest(unittest.TestCase):
    def test_missing_measurements_are_distinct_from_far_objects(self):
        values = [0, 1, 500, 2500, 5000, 6000]
        pixels = list(render_depth(len(values), 1, struct.pack('<6H', *values)).getdata())
        self.assertEqual(pixels[0], (0, 0, 0))
        self.assertEqual(pixels[1], (255, 255, 255))
        self.assertEqual(pixels[4], (1, 1, 1))
        self.assertEqual(pixels[5], pixels[4])
        brightness = [p[0] for p in pixels[1:]]
        self.assertEqual(brightness, sorted(brightness, reverse=True))
        self.assertTrue(all(r == g == b for r, g, b in pixels))


if __name__ == '__main__':
    unittest.main()
