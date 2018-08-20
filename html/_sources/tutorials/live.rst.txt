Live
====

A typical live source is UDP multicast, which is the only live protocol
packager supports directly right now.

For other unsupported protocols, you can use FFmpeg to pipe the input.
See :doc:`ffmpeg_piping` for details.

Examples
--------

The command is similar to the on-demand, see :doc:`dash` and :doc:`hls`.

Here are some examples.

* DASH::

    $ packager \
      'in=udp://225.1.1.8:8001?interface=172.29.46.122,stream=audio,init_segment=audio_init.mp4,segment_template=audio_$Number$.m4s' \
      'in=udp://225.1.1.8:8001?interface=172.29.46.122,stream=video,init_segment=h264_360p_init.mp4,segment_template=h264_360p_$Number$.m4s' \
      'in=udp://225.1.1.8:8002?interface=172.29.46.122,stream=video,init_segment=h264_480p_init.mp4,segment_template=h264_480p_$Number$.m4s' \
      'in=udp://225.1.1.8:8003?interface=172.29.46.122,stream=video,init_segment=h264_720p_init.mp4,segment_template=h264_720p_$Number$.m4s' \
      'in=udp://225.1.1.8:8004?interface=172.29.46.122,stream=video,init_segment=h264_1080p_init.mp4,segment_template=h264_1080p_$Number$.m4s' \
      --mpd_output h264.mpd


* HLS::

    $ packager \
      'in=udp://225.1.1.8:8001?interface=172.29.46.122,stream=audio,init_segment=audio_init.mp4,segment_template=audio_$Number$.m4s,playlist_name=audio.m3u8,hls_group_id=audio,hls_name=ENGLISH' \
      'in=udp://225.1.1.8:8001?interface=172.29.46.122,stream=video,init_segment=h264_360p_init.mp4,segment_template=h264_360p_$Number$.m4s,playlist_name=h264_360p.m3u8' \
      'in=udp://225.1.1.8:8002?interface=172.29.46.122,stream=video,init_segment=h264_480p_init.mp4,segment_template=h264_480p_$Number$.m4s,playlist_name=h264_480p.m3u8' \
      'in=udp://225.1.1.8:8003?interface=172.29.46.122,stream=video,init_segment=h264_720p_init.mp4,segment_template=h264_720p_$Number$.m4s,playlist_name=h264_720p.m3u8' \
      'in=udp://225.1.1.8:8004?interface=172.29.46.122,stream=video,init_segment=h264_1080p_init.mp4,segment_template=h264_1080p_$Number$.m4s,playlist_name=h264_1080p.m3u8' \
      --hls_master_playlist_output h264_master.m3u8 \
      --hls_playlist_type LIVE

.. note::

    Packager supports removing old segments automatically. See
    `preserved_segments_outside_live_window` option in
    :doc:`/options/dash_options` or :doc:`/options/hls_options` for details.

.. note::

    Shaka Packager ensures all segments referenced in DASH manifest / HLS
    playlists are available, by updating the manifest / playlists only after a
    segment is completed.

    However, if content is not served directly from packaging output location,
    extra care must be taken outside of packager to avoid updating manifest /
    playlists without updating media segments.

    Here is an example flow that avoids potential race condition. The following
    steps should be done SERIALLY AND IN ORDER in every sync loop when uploading
    manifest / playlists and media segments to content server:

      1. Upload manifest / playlists under different names
      2. Upload / Sync media segments
      3. Rename uploaded manifest / playlists back to the original names

Configuration options
---------------------

.. include:: /options/udp_file_options.rst
.. include:: /options/segment_template_formatting.rst
