# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class GpuRenderingStats(object):
  def __init__(self, rendering_stats_deltas):
    rs = rendering_stats_deltas

    # Scroll Stats
    self.total_time = rs.get('totalTimeInSeconds', 0)
    self.screen_frame_count = rs.get('numFramesSentToScreen', 0)
    self.dropped_frame_count = rs.get('droppedFrameCount', 0)
    self.impl_thread_scroll_count = rs.get('numImplThreadScrolls', 0)
    self.main_thread_scroll_count = rs.get('numMainThreadScrolls', 0)
    self.drawn_layers_count = rs.get('numLayersDrawn', 0)
    self.missing_tile_count = rs.get('numMissingTiles', 0)

    # Texture Upload Stats
    self.texture_upload_count = rs.get('textureUploadCount', 0)
    self.texture_upload_time = rs.get('totalTextureUploadTimeInSeconds', 0)
    self.commit_count = rs.get('totalCommitCount', 0)
    self.commit_time = rs.get('totalCommitTimeInSeconds', 0)

    # Image Decoding Stats
    self.deferred_image_decode_count = rs.get(
        'totalDeferredImageDecodeCount', 0)
    self.deferred_image_decode_time = rs.get(
        'totalDeferredImageDecodeTimeInSeconds', 0)
    self.deferred_image_cache_hits = rs.get(
        'totalDeferredImageCacheHitCount', 0)
    self.image_gathering_count = rs.get('totalImageGatheringCount', 0)
    self.image_gathering_time = rs.get('totalImageGatheringTimeInSeconds', 0)

    # Tile Analysis Stats
    self.tile_analysis_count = rs.get('totalTilesAnalyzed', 0)
    self.tile_analysis_time = rs.get('totalTileAnalysisTimeInSeconds', 0)
    self.solid_color_tile_analysis_count = rs.get('solidColorTilesAnalyzed', 0)

    # Latency Stats
    self.input_event_count = rs.get('inputEventCount', 0)
    self.input_event_latency = rs.get('totalInputLatency', 0)
    self.touch_ui_count = rs.get('touchUICount', 0)
    self.touch_ui_latency = rs.get('totalTouchUILatency', 0)
    self.touch_acked_count = rs.get('touchAckedCount', 0)
    self.touch_acked_latency = rs.get('totalTouchAckedLatency', 0)
    self.scroll_update_count = rs.get('scrollUpdateCount', 0)
    self.scroll_update_latency = rs.get('totalScrollUpdateLatency', 0)
