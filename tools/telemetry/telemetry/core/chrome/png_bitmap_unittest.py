# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

from telemetry.core.chrome import png_bitmap

# This is a simple base64 encoded 2x2 PNG which contains, in order, a single
# Red, Yellow, Blue, and Green pixel.
test_png = """
iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91
JpzAAAAAXNSR0IArs4c6QAAAAlwSFlzAAALEwAACx
MBAJqcGAAAABZJREFUCNdj/M/AwPCfgYGB4T/DfwY
AHAAD/iOWZXsAAAAASUVORK5CYII=
"""

class PngBitmapTest(unittest.TestCase):
  def testRead(self):
    png = png_bitmap.PngBitmap(test_png)

    self.assertEquals(2, png.width)
    self.assertEquals(2, png.height)

    png.GetPixelColor(0, 0).AssertIsRGB(255, 0, 0)
    png.GetPixelColor(1, 1).AssertIsRGB(0, 255, 0)
    png.GetPixelColor(0, 1).AssertIsRGB(0, 0, 255)
    png.GetPixelColor(1, 0).AssertIsRGB(255, 255, 0)
