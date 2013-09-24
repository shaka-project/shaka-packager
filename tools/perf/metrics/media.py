# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging
import os

from metrics import Metric

class MediaMetric(Metric):
  """MediaMetric class injects and calls JS responsible for recording metrics.

  Default media metrics are collected for every media element in the page,
  such as decoded_frame_count, dropped_frame_count, decoded_video_bytes, and
  decoded_audio_bytes.
  """
  def __init__(self, tab):
    super(MediaMetric, self).__init__()
    with open(os.path.join(os.path.dirname(__file__), 'media.js')) as f:
      js = f.read()
      tab.ExecuteJavaScript(js)
    self._results = None

  def Start(self, page, tab):
    """Create the media metrics for all media elements in the document."""
    tab.ExecuteJavaScript('window.__createMediaMetricsForDocument()')

  def Stop(self, page, tab):
    self._results = tab.EvaluateJavaScript('window.__getAllMetrics()')

  def AddResults(self, tab, results):
    """Reports all recorded metrics as Telemetry perf results."""
    assert self._results
    for media_metric in self._results:
      self._AddResultsForMediaElement(media_metric, results)

  def _AddResultsForMediaElement(self, media_metric, results):
    """Reports metrics for one media element.

    Media metrics contain an ID identifying the media element and values:
    media_metric = {
      'id': 'video_1',
      'metrics': {
          'time_to_play': 120,
          'decoded_bytes': 13233,
          ...
      }
    }
    """
    def AddOneResult(metric, unit):
      metrics = media_metric['metrics']
      for m in metrics:
        if m.startswith(metric):
          special_label = m[len(metric):]
          results.Add(trace + special_label, unit, str(metrics[m]),
                      chart_name=metric, data_type='default')

    trace = media_metric['id']
    if not trace:
      logging.error('Metrics ID is missing in results.')
      return
    AddOneResult('decoded_audio_bytes', 'bytes')
    AddOneResult('decoded_video_bytes', 'bytes')
    AddOneResult('decoded_frame_count', 'frames')
    AddOneResult('dropped_frame_count', 'frames')
    AddOneResult('playback_time', 'sec')
    AddOneResult('seek', 'sec')
    AddOneResult('time_to_play', 'sec')

