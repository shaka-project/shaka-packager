# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

from metrics import smoothness
from metrics.gpu_rendering_stats import GpuRenderingStats
from telemetry.page import page
from telemetry.page.page_measurement_results import PageMeasurementResults

class SmoothnessMetricsUnitTest(unittest.TestCase):
  def testCalcResultsRealRenderStats(self):
    mock_rendering_stats_deltas = {
        'totalTimeInSeconds': 1.0,
        'numFramesSentToScreen': 100,
        'droppedFrameCount': 20,
        'numImplThreadScrolls': 50,
        'numMainThreadScrolls': 50,
        'numLayersDrawn': 240,
        'numMissingTiles': 10,
        'textureUploadCount': 120,
        'totalTextureUploadTimeInSeconds': 1.2,
        'totalCommitCount': 130,
        'totalCommitTimeInSeconds': 1.3,
        'totalDeferredImageDecodeCount': 140,
        'totalDeferredImageDecodeTimeInSeconds': 1.4,
        'totalDeferredImageCacheHitCount': 30,
        'totalImageGatheringCount': 150,
        'totalImageGatheringTimeInSeconds': 1.5,
        'totalTilesAnalyzed': 160,
        'totalTileAnalysisTimeInSeconds': 1.6,
        'solidColorTilesAnalyzed': 40,
        'inputEventCount': 170,
        'totalInputLatency': 1.7,
        'touchUICount': 180,
        'totalTouchUILatency': 1.8,
        'touchAckedCount': 190,
        'totalTouchAckedLatency': 1.9,
        'scrollUpdateCount': 200,
        'totalScrollUpdateLatency': 2.0}
    stats = GpuRenderingStats(mock_rendering_stats_deltas)

    res = PageMeasurementResults()
    res.WillMeasurePage(page.Page('http://foo.com/', None))
    smoothness.CalcResults(stats, res)
    res.DidMeasurePage()

    # Scroll Results
    self.assertAlmostEquals(
        1.0 / 100.0 * 1000.0,
        res.page_results[0]['mean_frame_time'].value, 2)
    self.assertAlmostEquals(
        20.0 / 100.0 * 100.0,
        res.page_results[0]['dropped_percent'].value)
    self.assertAlmostEquals(
        50.0 / (50.0 + 50.0) * 100.0,
        res.page_results[0]['percent_impl_scrolled'].value)
    self.assertAlmostEquals(
        240.0 / 100.0,
        res.page_results[0]['average_num_layers_drawn'].value)
    self.assertAlmostEquals(
        10.0 / 100.0,
        res.page_results[0]['average_num_missing_tiles'].value)

    # Texture Upload Results
    self.assertAlmostEquals(
        1.3 / 130.0 * 1000.0,
        res.page_results[0]['average_commit_time'].value)
    self.assertEquals(
        120,
        res.page_results[0]['texture_upload_count'].value)
    self.assertEquals(
        1.2,
        res.page_results[0]['total_texture_upload_time'].value)

    # Image Decoding Results
    self.assertEquals(
        140,
        res.page_results[0]['total_deferred_image_decode_count'].value)
    self.assertEquals(
        30,
        res.page_results[0]['total_image_cache_hit_count'].value)
    self.assertAlmostEquals(
        1.5 / 150.0 * 1000.0,
        res.page_results[0]['average_image_gathering_time'].value)
    self.assertEquals(
        1.4,
        res.page_results[0]['total_deferred_image_decoding_time'].value)

    # Tile Analysis Results
    self.assertEquals(
        160,
        res.page_results[0]['total_tiles_analyzed'].value)
    self.assertEquals(
        40,
        res.page_results[0]['solid_color_tiles_analyzed'].value)
    self.assertAlmostEquals(
        1.6 / 160.0 * 1000.0,
        res.page_results[0]['average_tile_analysis_time'].value)

    # Latency Results
    self.assertAlmostEquals(
        1.7 / 170.0 * 1000.0,
        res.page_results[0]['average_latency'].value)
    self.assertAlmostEquals(
        1.8 / 180.0 * 1000.0,
        res.page_results[0]['average_touch_ui_latency'].value)
    self.assertAlmostEquals(
        1.9 / 190.0 * 1000.0,
        res.page_results[0]['average_touch_acked_latency'].value)
    self.assertAlmostEquals(
        2.0 / 200.0 * 1000.0,
        res.page_results[0]['average_scroll_update_latency'].value)
