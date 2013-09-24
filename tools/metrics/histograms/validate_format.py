# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Verifies that the histograms XML file is well-formatted."""

import extract_histograms


def main():
  # This will raise an exception if the file is not well-formatted.
  histograms = extract_histograms.ExtractHistograms('histograms.xml')


if __name__ == '__main__':
  main()

