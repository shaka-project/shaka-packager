# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from telemetry.page import page_measurement


_JS = 'chrome.gpuBenchmarking.printToSkPicture("{0}");'


class SkpicturePrinter(page_measurement.PageMeasurement):
  def AddCommandLineOptions(self, parser):
    parser.add_option('-s', '--skp-outdir',
                      help='Output directory for the SKP files')

  def CustomizeBrowserOptions(self, options):
    options.extra_browser_args.extend(['--enable-gpu-benchmarking',
                                       '--no-sandbox',
                                       '--enable-deferred-image-decoding',
                                       '--force-compositing-mode'])

  def MeasurePage(self, page, tab, results):
    skp_outdir = self.options.skp_outdir
    if not skp_outdir:
      raise Exception('Please specify --skp-outdir')
    outpath = os.path.abspath(
        os.path.join(skp_outdir,
                     page.url_as_file_safe_name))
    # Replace win32 path separator char '\' with '\\'.
    js = _JS.format(outpath.replace('\\', '\\\\'))
    tab.EvaluateJavaScript(js)
    results.Add('output_path', 'path', outpath)
