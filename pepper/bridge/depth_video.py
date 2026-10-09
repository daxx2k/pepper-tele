"""Monochrome depth: bright near, dark far; invalid measurements are black."""
from PIL import Image

GRAY = [max(1, 255-min(v, 5000)*254//5000) for v in range(65536)]
GRAY[0] = 0


def render_depth(width, height, data):
    # One lookup in Pillow's C implementation; no Python loop per pixel.
    depth = Image.frombytes('I;16', (width, height), data).convert('I')
    return depth.point(GRAY, 'L').convert('RGB')
