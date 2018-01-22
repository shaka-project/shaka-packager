HLS
===

HTTP Live Streaming (also known as HLS) is an HTTP-based media streaming
communications protocol implemented by Apple Inc. as part of its QuickTime,
Safari, OS X, and iOS software. It resembles MPEG-DASH in that it works by
breaking the overall stream into a sequence of small HTTP-based file downloads,
each download loading one short chunk of an overall potentially unbounded
transport stream. As the stream is played, the client may select from a number
of different alternate streams containing the same material encoded at a variety
of data rates, allowing the streaming session to adapt to the available data
rate. At the start of the streaming session, HLS downloads an extended M3U
playlist containing the metadata for the various sub-streams which are
available.

Shaka Packager supports HLS content packaging. This tutorial covers HLS
packaging of VOD content without encryption. For live content packaging, see
:doc:`live`; for content encryption, see :doc:`drm`.

Synopsis
--------

::

    $ packager {stream_descriptor with HLS specific descriptors} \
               [stream_descriptor with HLS specific descriptors] ... \
      --hls_master_playlist_output {master playlist output path} \
      [Other HLS options] \
      [Other options, e.g. DRM options, DASH options]

See `HLS specific stream descriptor fields`_ for the available HLS specific
stream descriptor fields.

See `HLS options`_ for the available HLS related options.

.. note::

    DASH and HLS options can both be specified to output DASH and HLS manifests
    at the same time. Note that it works only for MP4 outputs.

Examples
---------

The examples below uses the H264 streams created in :doc:`encoding`.

* TS output::

    $ packager \
      'in=h264_baseline_360p_600.mp4,stream=audio,segment_template=audio_$Number$.ts,playlist_name=audio.m3u8,hls_group_id=audio,hls_name=ENGLISH' \
      'in=h264_baseline_360p_600.mp4,stream=video,segment_template=h264_360p_$Number$.ts,playlist_name=h264_360p.m3u8' \
      'in=h264_main_480p_1000.mp4,stream=video,segment_template=h264_480p_$Number$.ts,playlist_name=h264_480p.m3u8' \
      'in=h264_main_720p_3000.mp4,stream=video,segment_template=h264_720p_$Number$.ts,playlist_name=h264_720p.m3u8' \
      'in=h264_high_1080p_6000.mp4,stream=video,segment_template=h264_1080p_$Number$.ts,playlist_name=h264_1080p.m3u8' \
      --hls_master_playlist_output h264_master.m3u8

The above packaging command creates five single track TS streams
(4 video, 1 audio) and HLS playlists, which describe the streams.

* MP4 output is also supported::

    $ packager \
      'in=h264_baseline_360p_600.mp4,stream=audio,init_segment=audio_init.mp4,segment_template=audio_$Number$.m4s,playlist_name=audio.m3u8,hls_group_id=audio,hls_name=ENGLISH' \
      'in=h264_baseline_360p_600.mp4,stream=video,init_segment=h264_360p_init.mp4,segment_template=h264_360p_$Number$.m4s,playlist_name=h264_360p.m3u8' \
      'in=h264_main_480p_1000.mp4,stream=video,init_segment=h264_480p_init.mp4,segment_template=h264_480p_$Number$.m4s,playlist_name=h264_480p.m3u8' \
      'in=h264_main_720p_3000.mp4,stream=video,init_segment=h264_720p_init.mp4,segment_template=h264_720p_$Number$.m4s,playlist_name=h264_720p.m3u8' \
      'in=h264_main_1080p_6000.mp4,stream=video,init_segment=h264_1080p_init.mp4,segment_template=h264_1080p_$Number$.m4s,playlist_name=h264_1080p.m3u8' \
      --hls_master_playlist_output h264_master.m3u8

The above packaging command creates five groups of streams (each with an init
segment and a series of media segments) and HLS playlists, which describe the
streams.

* Single file MP4 output is also supported::

    $ packager \
      in=h264_baseline_360p_600.mp4,stream=audio,output=audio.mp4,playlist_name=audio.m3u8,hls_group_id=audio,hls_name=ENGLISH \
      in=h264_baseline_360p_600.mp4,stream=video,output=h264_360p.mp4,playlist_name=h264_360p.m3u8 \
      in=h264_main_480p_1000.mp4,stream=video,output=h264_480p.mp4,playlist_name=h264_480p.m3u8 \
      in=h264_main_720p_3000.mp4,stream=video,output=h264_720p.mp4,playlist_name=h264_720p.m3u8 \
      in=h264_high_1080p_6000.mp4,stream=video,output=h264_1080p.mp4,playlist_name=h264_1080p.m3u8 \
      --hls_master_playlist_output h264_master.m3u8

The above packaging command creates five single file MP4 streams and HLS
playlists, which describe the streams.

.. include:: /tutorials/dash_hls_example.rst

.. include:: /options/hls_stream_descriptors.rst

.. include:: /options/hls_options.rst

.. include:: /options/segment_template_formatting.rst
