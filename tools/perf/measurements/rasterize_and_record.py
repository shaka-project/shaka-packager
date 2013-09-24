# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from metrics import smoothness
from telemetry.page import page_measurement

class StatsCollector(object):
  def __init__(self, timeline):
    """
    Utility class for collecting rendering stats from timeline model.

    timeline -- The timeline model
    """
    self.timeline = timeline
    self.total_best_rasterize_time = 0
    self.total_best_record_time = 0
    self.total_pixels_rasterized = 0
    self.total_pixels_recorded = 0
    self.trigger_event = self.FindTriggerEvent()
    self.renderer_process = self.trigger_event.start_thread.parent

  def FindTriggerEvent(self):
    events = [s for
              s in self.timeline.GetAllEventsOfName(
                  'measureNextFrame')
              if s.parent_slice == None]
    if len(events) != 1:
      raise LookupError, 'no measureNextFrame event found'
    return events[0]

  def FindFrameNumber(self, trigger_time):
    start_event = None
    for event in self.renderer_process.IterAllSlicesOfName(
        "LayerTreeHost::UpdateLayers"):
      if event.start > trigger_time:
        if start_event == None:
          start_event = event
        elif event.start < start_event.start:
          start_event = event
    if start_event is None:
      raise LookupError, \
          'no LayterTreeHost::UpdateLayers after measureNextFrame found'
    return start_event.args["source_frame_number"]

  def GatherRasterizeStats(self, frame_number):
    for event in self.renderer_process.IterAllSlicesOfName(
        "RasterWorkerPoolTaskImpl::RunRasterOnThread"):
      if event.args["data"]["source_frame_number"] == frame_number:
        for raster_loop_event in event.GetAllSubSlicesOfName("RasterLoop"):
          best_rasterize_time = float("inf")
          for raster_event in raster_loop_event.GetAllSubSlicesOfName(
              "Picture::Raster"):
            if "num_pixels_rasterized" in raster_event.args:
              best_rasterize_time = min(best_rasterize_time,
                                        raster_event.duration)
              self.total_pixels_rasterized += \
                  raster_event.args["num_pixels_rasterized"]
          if best_rasterize_time == float('inf'):
            best_rasterize_time = 0
          self.total_best_rasterize_time += best_rasterize_time

  def GatherRecordStats(self, frame_number):
    for event in self.renderer_process.IterAllSlicesOfName(
        "PictureLayer::Update"):
      if event.args["source_frame_number"] == frame_number:
        for record_loop_event in event.GetAllSubSlicesOfName("RecordLoop"):
          best_record_time = float('inf')
          for record_event in record_loop_event.GetAllSubSlicesOfName(
              "Picture::Record"):
            best_record_time = min(best_record_time, record_event.duration)
            self.total_pixels_recorded += (
                record_event.args["data"]["width"] *
                record_event.args["data"]["height"])
          if best_record_time == float('inf'):
            best_record_time = 0
          self.total_best_record_time += best_record_time

  def GatherRenderingStats(self):
    trigger_time = self.trigger_event.start
    frame_number = self.FindFrameNumber(trigger_time)
    self.GatherRasterizeStats(frame_number)
    self.GatherRecordStats(frame_number)

def DivideIfPossibleOrZero(numerator, denominator):
  if denominator == 0:
    return 0
  return numerator / denominator

class RasterizeAndRecord(page_measurement.PageMeasurement):
  def __init__(self):
    super(RasterizeAndRecord, self).__init__('', True)
    self._metrics = None

  def AddCommandLineOptions(self, parser):
    parser.add_option('--report-all-results', dest='report_all_results',
                      action='store_true',
                      help='Reports all data collected')
    parser.add_option('--raster-record-repeat', dest='raster_record_repeat',
                      default=20,
                      help='Repetitions in raster and record loops.' +
                      'Higher values reduce variance, but can cause' +
                      'instability (timeouts, event buffer overflows, etc.).')
    parser.add_option('--start-wait-time', dest='start_wait_time',
                      default=2,
                      help='Wait time before the benchmark is started ' +
                      '(must be long enought to load all content)')
    parser.add_option('--stop-wait-time', dest='stop_wait_time',
                      default=5,
                      help='Wait time before measurement is taken ' +
                      '(must be long enough to render one frame)')

  def CustomizeBrowserOptions(self, options):
    options.extra_browser_args.append('--enable-gpu-benchmarking')
    # Run each raster task N times. This allows us to report the time for the
    # best run, effectively excluding cache effects and time when the thread is
    # de-scheduled.
    options.extra_browser_args.append(
        '--slow-down-raster-scale-factor=' + str(options.raster_record_repeat))
    # Enable impl-side-painting. Current version of benchmark only works for
    # this mode.
    options.extra_browser_args.append('--enable-impl-side-painting')
    options.extra_browser_args.append('--force-compositing-mode')
    options.extra_browser_args.append('--enable-threaded-compositing')

  def MeasurePage(self, page, tab, results):
    self._metrics = smoothness.SmoothnessMetrics(tab)

    # Rasterize only what's visible.
    tab.ExecuteJavaScript(
        'chrome.gpuBenchmarking.setRasterizeOnlyVisibleContent();')

    # Wait until the page has loaded and come to a somewhat steady state.
    # Needs to be adjusted for every device (~2 seconds for workstation).
    time.sleep(float(self.options.start_wait_time))

    # Render one frame before we start gathering a trace. On some pages, the
    # first frame requested has more variance in the number of pixels
    # rasterized.
    tab.ExecuteJavaScript("""
        window.__rafFired = false;
        window.webkitRequestAnimationFrame(function() {
          chrome.gpuBenchmarking.setNeedsDisplayOnAllLayers();
          window.__rafFired  = true;
        });
    """)

    tab.browser.StartTracing('webkit,benchmark', 60)
    self._metrics.Start()

    tab.ExecuteJavaScript("""
        console.time("measureNextFrame");
        window.__rafFired = false;
        window.webkitRequestAnimationFrame(function() {
          chrome.gpuBenchmarking.setNeedsDisplayOnAllLayers();
          window.__rafFired  = true;
        });
    """)
    # Wait until the frame was drawn.
    # Needs to be adjusted for every device and for different
    # raster_record_repeat counts.
    # TODO(ernstm): replace by call-back.
    time.sleep(float(self.options.stop_wait_time))
    tab.ExecuteJavaScript('console.timeEnd("measureNextFrame")')

    tab.browser.StopTracing()
    self._metrics.Stop()

    timeline = tab.browser.GetTraceResultAndReset().AsTimelineModel()
    collector = StatsCollector(timeline)
    collector.GatherRenderingStats()

    rendering_stats = self._metrics.end_values

    results.Add('best_rasterize_time', 'seconds',
                collector.total_best_rasterize_time / 1.e3,
                data_type='unimportant')
    results.Add('best_record_time', 'seconds',
                collector.total_best_record_time / 1.e3,
                data_type='unimportant')
    results.Add('total_pixels_rasterized', 'pixels',
                collector.total_pixels_rasterized,
                data_type='unimportant')
    results.Add('total_pixels_recorded', 'pixels',
                collector.total_pixels_recorded,
                data_type='unimportant')

    if self.options.report_all_results:
      for k, v in rendering_stats.iteritems():
        results.Add(k, '', v)
