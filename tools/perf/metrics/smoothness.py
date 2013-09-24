# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from telemetry.core import util

class SmoothnessMetrics(object):
  def __init__(self, tab):
    self._tab = tab
    with open(
      os.path.join(os.path.dirname(__file__),
                   'smoothness.js')) as f:
      js = f.read()
      tab.ExecuteJavaScript(js)

  def Start(self):
    self._tab.ExecuteJavaScript(
        'window.__renderingStats = new __RenderingStats();'
        'window.__renderingStats.start()')

  def SetNeedsDisplayOnAllLayersAndStart(self):
    self._tab.ExecuteJavaScript(
        'chrome.gpuBenchmarking.setNeedsDisplayOnAllLayers();'
        'window.__renderingStats = new __RenderingStats();'
        'window.__renderingStats.start()')

  def Stop(self):
    self._tab.ExecuteJavaScript('window.__renderingStats.stop()')

  def BindToAction(self, action):
    # Make the scroll test start and stop measurement automatically.
    self._tab.ExecuteJavaScript(
        'window.__renderingStats = new __RenderingStats();')
    action.BindMeasurementJavaScript(self._tab,
                                     'window.__renderingStats.start();',
                                     'window.__renderingStats.stop();')

  @property
  def start_values(self):
    return self._tab.EvaluateJavaScript(
      'window.__renderingStats.getStartValues()')

  @property
  def end_values(self):
    return self._tab.EvaluateJavaScript(
      'window.__renderingStats.getEndValues()')

  @property
  def deltas(self):
    return self._tab.EvaluateJavaScript(
      'window.__renderingStats.getDeltas()')

def Average(numerator, denominator, scale = None, precision = None):
  if denominator == 0:
    return 0
  avg = float(numerator) / float(denominator)
  if scale:
    avg *= scale
  if precision:
    avg = round(avg, precision)
  return avg

def CalcFirstPaintTimeResults(results, tab):
  if tab.browser.is_content_shell:
    results.Add('first_paint', 'ms', 'unsupported')
    return

  tab.ExecuteJavaScript("""
      window.__rafFired = false;
      window.webkitRequestAnimationFrame(function() {
          window.__rafFired  = true;
      });
  """)
  util.WaitFor(lambda: tab.EvaluateJavaScript('window.__rafFired'), 60)

  first_paint_secs = tab.EvaluateJavaScript(
      'window.chrome.loadTimes().firstPaintTime - ' +
      'window.chrome.loadTimes().startLoadTime')

  results.Add('first_paint', 'ms', round(first_paint_secs * 1000, 1))

def CalcResults(benchmark_stats, results):
  s = benchmark_stats

  # Scroll Results
  results.Add('mean_frame_time', 'ms',
              Average(s.total_time, s.screen_frame_count, 1000, 3))
  results.Add('dropped_percent', '%',
              Average(s.dropped_frame_count, s.screen_frame_count,
                      100, 1),
              data_type='unimportant')
  results.Add('percent_impl_scrolled', '%',
              Average(s.impl_thread_scroll_count,
                      s.impl_thread_scroll_count +
                      s.main_thread_scroll_count,
                      100, 1),
              data_type='unimportant')
  results.Add('average_num_layers_drawn', '',
              Average(s.drawn_layers_count, s.screen_frame_count, 1, 1),
              data_type='unimportant')
  results.Add('average_num_missing_tiles', '',
              Average(s.missing_tile_count, s.screen_frame_count, 1, 1),
              data_type='unimportant')

  # Texture Upload Results
  results.Add('average_commit_time', 'ms',
              Average(s.commit_time, s.commit_count, 1000, 3),
              data_type='unimportant')
  results.Add('texture_upload_count', 'count',
              s.texture_upload_count)
  results.Add('total_texture_upload_time', 'seconds',
              s.texture_upload_time)

  # Image Decoding Results
  results.Add('total_deferred_image_decode_count', 'count',
              s.deferred_image_decode_count,
              data_type='unimportant')
  results.Add('total_image_cache_hit_count', 'count',
              s.deferred_image_cache_hits,
              data_type='unimportant')
  results.Add('average_image_gathering_time', 'ms',
              Average(s.image_gathering_time, s.image_gathering_count,
                      1000, 3),
              data_type='unimportant')
  results.Add('total_deferred_image_decoding_time', 'seconds',
              s.deferred_image_decode_time,
              data_type='unimportant')

  # Tile Analysis Results
  results.Add('total_tiles_analyzed', 'count',
              s.tile_analysis_count,
              data_type='unimportant')
  results.Add('solid_color_tiles_analyzed', 'count',
              s.solid_color_tile_analysis_count,
              data_type='unimportant')
  results.Add('average_tile_analysis_time', 'ms',
              Average(s.tile_analysis_time, s.tile_analysis_count,
                      1000, 3),
              data_type='unimportant')

  # Latency Results
  results.Add('average_latency', 'ms',
              Average(s.input_event_latency, s.input_event_count,
                      1000, 3),
              data_type='unimportant')
  results.Add('average_touch_ui_latency', 'ms',
              Average(s.touch_ui_latency, s.touch_ui_count, 1000, 3),
              data_type='unimportant')
  results.Add('average_touch_acked_latency', 'ms',
              Average(s.touch_acked_latency, s.touch_acked_count,
                      1000, 3),
              data_type='unimportant')
  results.Add('average_scroll_update_latency', 'ms',
              Average(s.scroll_update_latency, s.scroll_update_count,
                      1000, 3),
              data_type='unimportant')
