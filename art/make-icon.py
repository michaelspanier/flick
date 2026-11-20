#!/usr/bin/env python3
# Copyright 2026, Michael Spanier.
# Distributed under the terms of the MIT License.
#
# SPDX-License-Identifier: MIT
"""Writes the Flick icon as HVIF (Haiku Vector Icon Format).

    python3 art/make-icon.py

writes art/Flick.hvif, which Icon-O-Matic can open, and the vector_icon
resource in src/Flick.rdef. The icon follows the Haiku icon guidelines: an
isometric red camera standing on the floor, light from the top left, black
outline, shadow straight to the right. Details that do not show at 16 x 16
are hidden there by their level of detail.

Coordinates are on the 64 x 64 grid of the icon.
"""

import math
import os
import re
import struct

HERE = os.path.dirname(os.path.abspath(__file__))

# --- encoding (see src/libs/icon/flat_icon/FlatIconFormat.cpp in Haiku) ---

STYLE_SOLID = 1
STYLE_GRADIENT = 2
GRADIENT_LINEAR = 0
GRADIENT_CIRCULAR = 1
GRADIENT_FLAG_TRANSFORM = 1 << 1
PATH_FLAG_CLOSED = 1 << 1
PATH_FLAG_NO_CURVES = 1 << 3
SHAPE_PATH_SOURCE = 10
SHAPE_FLAG_HINTING = 1 << 2
SHAPE_FLAG_LOD_SCALE = 1 << 3
SHAPE_FLAG_HAS_TRANSFORMERS = 1 << 4
TRANSFORMER_STROKE = 23
JOIN_ROUND = 2
CAP_ROUND = 2


def coord(value):
	if value == int(value) and -32 <= value <= 95:
		return bytes([int(value) + 32])
	raw = int(round((min(max(value, -128.0), 192.0) + 128.0) * 102.0))
	raw |= 0x8000
	return bytes([raw >> 8, raw & 0xff])


def float24(value):
	if value == 0:
		return b"\0\0\0"
	bits = struct.unpack("<I", struct.pack("<f", value))[0]
	sign = bits >> 31
	exponent = ((bits >> 23) & 0xff) - 127
	mantissa = bits & 0x7fffff
	if exponent >= 32 or exponent < -32:
		return b"\0\0\0"
	packed = (sign << 23) | ((exponent + 32) << 17) | (mantissa >> 6)
	return bytes([packed >> 16, (packed >> 8) & 0xff, packed & 0xff])


# --- styles ---

def solid(r, g, b, a=255):
	return bytes([STYLE_SOLID, r, g, b, a])


def _gradient(kind, matrix, stops):
	data = bytes([STYLE_GRADIENT, kind, GRADIENT_FLAG_TRANSFORM, len(stops)])
	data += b"".join(float24(v) for v in matrix)
	for offset, (r, g, b, a) in stops:
		data += bytes([int(round(offset * 255)), r, g, b, a])
	return data


def linear(start, end, stops):
	"""A gradient from start to end (Haiku's runs from x -64 to 64)."""
	dx, dy = end[0] - start[0], end[1] - start[1]
	scale = math.hypot(dx, dy) / 128.0
	angle = math.atan2(dy, dx)
	c, s = math.cos(angle) * scale, math.sin(angle) * scale
	middle = ((start[0] + end[0]) / 2, (start[1] + end[1]) / 2)
	return _gradient(GRADIENT_LINEAR, (c, s, -s, c, middle[0], middle[1]),
		stops)


def radial(center, radius, stops):
	"""A circular gradient (Haiku's has the radius 64)."""
	scale = radius / 64.0
	return _gradient(GRADIENT_CIRCULAR,
		(scale, 0, 0, scale, center[0], center[1]), stops)


def rgba(r, g, b, a=255):
	return (r, g, b, a)


# --- paths ---

def polygon(points):
	data = bytes([PATH_FLAG_CLOSED | PATH_FLAG_NO_CURVES, len(points)])
	for x, y in points:
		data += coord(x) + coord(y)
	return data


def ellipse(center, radius, matrix=((1, 0), (0, 1))):
	"""A circle of radius, bent by matrix onto a plane, around center."""
	k = 0.5523 * radius
	data = bytes([PATH_FLAG_CLOSED, 4])

	def place(x, y):
		(a, b), (c, d) = matrix
		return (center[0] + a * x + b * y, center[1] + c * x + d * y)

	for angle in (0, 90, 180, 270):
		t = math.radians(angle)
		px, py = radius * math.cos(t), radius * math.sin(t)
		tx, ty = -math.sin(t) * k, math.cos(t) * k
		for x, y in ((px, py), (px - tx, py - ty), (px + tx, py + ty)):
			x, y = place(x, y)
			data += coord(round(x, 2)) + coord(round(y, 2))
	return data


# --- shapes ---

def shape(style, paths, hinting=False, min_size=None, stroke=None):
	flags = SHAPE_FLAG_HINTING if hinting else 0
	extra = b""
	if min_size is not None:
		flags |= SHAPE_FLAG_LOD_SCALE
		extra += bytes([int(round(min_size / 64.0 * 63.75)), 255])
	if stroke is not None:
		flags |= SHAPE_FLAG_HAS_TRANSFORMERS
		extra += bytes([1, TRANSFORMER_STROKE, stroke + 128,
			JOIN_ROUND | (CAP_ROUND << 4), 4])
	return bytes([SHAPE_PATH_SOURCE, style, len(paths)] + paths + [flags]) \
		+ extra


# --- the camera ---

# Isometric box: the corner nearest to the viewer is at the bottom. The
# front of the camera faces to the left, its floor edge rises 1 for 2.
NEAR = (32, 56)
LEFT = (4, 42)          # NEAR + 28 * (-1, -0.5)
RIGHT = (46, 49)        # NEAR + 14 * (1, -0.5)
HEIGHT = 24


def up(point, height=HEIGHT):
	return (point[0], point[1] - height)


BACK = (up(LEFT)[0] + 14, up(LEFT)[1] - 7)

# A point on the front: s along the width from LEFT, t up from the floor.
def front(s, t):
	return (LEFT[0] + s, LEFT[1] + s / 2.0 - t)


FRONT_PLANE = ((1, 0), (0.5, 1))    # circles on the front
TOP_PLANE = ((1, 0), (0, 0.5))      # circles on the top
OUTWARD = (-1, 0.5)                 # out of the front, towards the viewer

LENS = front(11, 11)
FLASH = [front(20, 17), front(26, 17), front(26, 21), front(20, 21)]
BUTTON = (34, 23)


def lens_at(depth):
	return (LENS[0] + OUTWARD[0] * depth, LENS[1] + OUTWARD[1] * depth)


def build():
	styles = [
		# 0 shadow
		solid(0, 0, 0, 70),
		# 1 front: lit, lighter at the top
		linear(up(LEFT), LEFT, [(0, rgba(238, 82, 66)), (1, rgba(176, 30, 36))]),
		# 2 right side: darkest where it meets the lit faces
		linear(NEAR, RIGHT, [(0, rgba(110, 14, 24)), (1, rgba(150, 34, 40))]),
		# 3 top: lighter on the left
		linear(up(LEFT), up(RIGHT),
			[(0, rgba(255, 160, 138)), (1, rgba(232, 92, 78))]),
		# 4 flash window
		linear(FLASH[3], FLASH[1],
			[(0, rgba(255, 252, 228)), (1, rgba(196, 190, 170))]),
		# 5 lens barrel, dark
		solid(48, 48, 52),
		# 6 lens ring, metal lit from the top left
		linear((lens_at(3)[0] - 6, lens_at(3)[1] - 9),
			(lens_at(3)[0] + 6, lens_at(3)[1] + 9),
			[(0, rgba(236, 236, 236)), (1, rgba(92, 92, 96))]),
		# 7 glass
		radial((lens_at(3)[0] - 1.5, lens_at(3)[1] - 1.5), 7,
			[(0, rgba(120, 150, 210)), (0.55, rgba(40, 56, 104)),
				(1, rgba(14, 16, 34))]),
		# 8 glint
		solid(255, 255, 255, 190),
		# 9 button side
		solid(90, 20, 24),
		# 10 button top
		linear((BUTTON[0] - 4, BUTTON[1]), (BUTTON[0] + 4, BUTTON[1]),
			[(0, rgba(250, 250, 250)), (1, rgba(150, 150, 154))]),
		# 11 outline
		solid(0, 0, 0),
	]

	paths = [
		# 0 shadow, straight to the right on the floor
		polygon([NEAR, RIGHT, (60, 49), (47, 56)]),
		# 1 front
		polygon([NEAR, LEFT, up(LEFT), up(NEAR)]),
		# 2 right side
		polygon([NEAR, up(NEAR), up(RIGHT), RIGHT]),
		# 3 top
		polygon([up(NEAR), up(LEFT), BACK, up(RIGHT)]),
		# 4 flash
		polygon(FLASH),
		# 5-7 lens barrel, coming out of the front
		ellipse(lens_at(0), 8.5, FRONT_PLANE),
		ellipse(lens_at(1.5), 8.5, FRONT_PLANE),
		ellipse(lens_at(3), 8.5, FRONT_PLANE),
		# 8 glass
		ellipse(lens_at(3), 6, FRONT_PLANE),
		# 9 glint, top left on the glass
		ellipse((lens_at(3)[0] - 2.2, lens_at(3)[1] - 2.6), 1.6, FRONT_PLANE),
		# 10, 11 shutter button on the top
		ellipse((BUTTON[0], BUTTON[1] + 1.5), 4, TOP_PLANE),
		ellipse(BUTTON, 4, TOP_PLANE),
		# 12 outline of the whole camera
		polygon([NEAR, LEFT, up(LEFT), BACK, up(RIGHT), RIGHT]),
		# 13 outline of the lens
		ellipse(lens_at(3), 8.5, FRONT_PLANE),
	]

	shapes = [
		shape(0, [0]),
		shape(1, [1], hinting=True),
		shape(2, [2], hinting=True),
		shape(3, [3], hinting=True),
		shape(4, [4], min_size=24),
		shape(5, [5]),
		shape(5, [6]),
		shape(6, [7]),
		shape(7, [8]),
		shape(8, [9], min_size=30),
		shape(9, [10], min_size=24),
		shape(10, [11], min_size=24),
		shape(11, [12], stroke=2),
		shape(11, [13], stroke=1, min_size=24),
	]

	data = b"ncif"
	data += bytes([len(styles)]) + b"".join(styles)
	data += bytes([len(paths)]) + b"".join(paths)
	data += bytes([len(shapes)]) + b"".join(shapes)
	return data


def rdef_resource(data):
	hexdata = data.hex().upper()
	lines = [hexdata[i:i + 64] for i in range(0, len(hexdata), 64)]
	body = "\n".join('\t$"%s"' % line for line in lines)
	return "resource vector_icon {\n%s\n};\n" % body


def write_rdef(data):
	path = os.path.join(HERE, "..", "src", "Flick.rdef")
	with open(path) as source:
		text = source.read()
	resource = rdef_resource(data)
	pattern = re.compile(r"resource vector_icon \{.*?\};\n", re.S)
	if pattern.search(text):
		text = pattern.sub(lambda match: resource, text)
	else:
		text = text.rstrip("\n") + "\n\n" + resource
	with open(path, "w") as target:
		target.write(text)


def main():
	data = build()
	with open(os.path.join(HERE, "Flick.hvif"), "wb") as target:
		target.write(data)
	write_rdef(data)
	print("%d bytes" % len(data))


if __name__ == "__main__":
	main()
