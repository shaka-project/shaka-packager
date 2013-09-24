# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import sys
import os
import base64

def _EnsurePngIsInPath():
  png_path = os.path.join(os.path.dirname(__file__),
                          '..', '..', '..', 'third_party', 'png')
  if png_path not in sys.path:
    sys.path.append(png_path)

_EnsurePngIsInPath()
import png # pylint: disable=F0401

class PngColor(object):
  """Encapsulates an RGB color retreived from a PngBitmap"""

  def __init__(self, r, g, b, a=255):
    self.r = r
    self.g = g
    self.b = b
    self.a = a

  def IsEqual(self, expected_color, tolerance=0):
    """Verifies that the color is within a given tolerance of
    the expected color"""
    r_diff = abs(self.r - expected_color.r)
    g_diff = abs(self.g - expected_color.g)
    b_diff = abs(self.b - expected_color.b)
    a_diff = abs(self.a - expected_color.a)
    return (r_diff <= tolerance and g_diff <= tolerance
        and b_diff <= tolerance and a_diff <= tolerance)

  def AssertIsRGB(self, r, g, b, tolerance=0):
    assert self.IsEqual(PngColor(r, g, b), tolerance)

  def AssertIsRGBA(self, r, g, b, a, tolerance=0):
    assert self.IsEqual(PngColor(r, g, b, a), tolerance)

class PngBitmap(object):
  """Utilities for parsing and inspecting inspecting a PNG"""

  def __init__(self, base64_png):
    self._png_data = base64.b64decode(base64_png)
    self._png = png.Reader(bytes=self._png_data)
    rgba8_data = self._png.asRGBA8()
    self._width = rgba8_data[0]
    self._height = rgba8_data[1]
    self._pixels = list(rgba8_data[2])
    self._metadata = rgba8_data[3]

  @property
  def width(self):
    """Width of the snapshot"""
    return self._width

  @property
  def height(self):
    """Height of the snapshot"""
    return self._height

  def GetPixelColor(self, x, y):
    """Returns a PngColor for the pixel at (x, y)"""
    row = self._pixels[y]
    offset = x * 4
    return PngColor(row[offset], row[offset+1], row[offset+2], row[offset+3])

  def WriteFile(self, path):
    with open(path, "wb") as f:
      f.write(self._png_data)
