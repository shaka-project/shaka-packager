# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import test

from measurements import image_decoding


class ImageDecodingToughImageCases(test.Test):
  test = image_decoding.ImageDecoding
  # TODO: Rename this page set to tough_image_cases.json
  page_set = 'page_sets/image_decoding_measurement.json'
