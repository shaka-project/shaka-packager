Ad Insertion
============

Shaka Packager does not do Ad Insertion directly, but it can precondition
content for `Dynamic Ad Insertion <http://bit.ly/2KK10DD>`_ with Google Ad
Manager.

Both DASH and HLS are supported.

Synopsis
--------

::

    $ packager <stream_descriptor> ... \
      --ad_cues <start_time[;start_time]...> \
      [Other options, e.g. DRM options, DASH options, HLS options]

Examples
--------

The examples below use the H264 streams created in :doc:`encoding`.

Three midroll cue markers are inserted at 10 minutes, 30 minutes and 50 minutes
respectively.

* DASH with live profile::

    $ packager \
      'in=h264_baseline_360p_600.mp4,stream=audio,init_segment=audio/init.mp4,segment_template=audio/$Number$.m4s' \
      'in=input_text.vtt,stream=text,init_segment=text/init.mp4,segment_template=text/$Number$.m4s' \
      'in=h264_baseline_360p_600.mp4,stream=video,init_segment=h264_360p/init.mp4,segment_template=h264_360p/$Number$.m4s' \
      'in=h264_main_480p_1000.mp4,stream=video,init_segment=h264_480p/init.mp4,segment_template=h264_480p/$Number$.m4s' \
      'in=h264_main_720p_3000.mp4,stream=video,init_segment=h264_720p/init.mp4,segment_template=h264_720p/$Number$.m4s' \
      'in=h264_high_1080p_6000.mp4,stream=video,init_segment=h264_1080p/init.mp4,segment_template=h264_1080p/$Number$.m4s' \
      --ad_cues 600;1800;3000 \
      --generate_static_live_mpd --mpd_output h264.mpd

* DASH with on-demand profile::

    $ packager \
      in=h264_baseline_360p_600.mp4,stream=audio,output=audio.mp4 \
      in=input_text.vtt,stream=text,output=output_text.mp4 \
      in=h264_baseline_360p_600.mp4,stream=video,output=h264_360p.mp4 \
      in=h264_main_480p_1000.mp4,stream=video,output=h264_480p.mp4 \
      in=h264_main_720p_3000.mp4,stream=video,output=h264_720p.mp4 \
      in=h264_high_1080p_6000.mp4,stream=video,output=h264_1080p.mp4 \
      --ad_cues 600;1800;3000 \
      --mpd_output h264.mpd

This generates six single-segment media files, one per stream, spanning multiple
periods. There may be problems handling this type of DASH contents in some
players, although it is recommended by `DASH IF IOP <http://bit.ly/2B0HL9q>`_.
Use the below option if your player does not like it.

* DASH with on-demand profile but one file per Period::

    $ packager \
      'in=h264_baseline_360p_600.mp4,stream=audio,output=audio_$Number$.mp4' \
      'in=input_text.vtt,stream=text,output=output_text_$Number$.mp4' \
      'in=h264_baseline_360p_600.mp4,stream=video,output=h264_360p_$Number$.mp4' \
      'in=h264_main_480p_1000.mp4,stream=video,output=h264_480p_$Number$.mp4' \
      'in=h264_main_720p_3000.mp4,stream=video,output=h264_720p_$Number$.mp4' \
      'in=h264_high_1080p_6000.mp4,stream=video,output=h264_1080p_$Number$.mp4' \
      --ad_cues 600;1800;3000 \
      --mpd_output h264.mpd

* HLS using transport streams::

    $ packager \
      'in=h264_baseline_360p_600.mp4,stream=audio,segment_template=audio_$Number$.aac' \
      'in=input_text.vtt,stream=text,segment_template=output_text_$Number$.vtt' \
      'in=h264_baseline_360p_600.mp4,stream=video,segment_template=h264_360p_$Number$.ts' \
      'in=h264_main_480p_1000.mp4,stream=video,segment_template=h264_480p_$Number$.ts' \
      'in=h264_main_720p_3000.mp4,stream=video,segment_template=h264_720p_$Number$.ts' \
      'in=h264_high_1080p_6000.mp4,stream=video,segment_template=h264_1080p_$Number$.ts' \
      --ad_cues 600;1800;3000 \
      --hls_master_playlist_output h264_master.m3u8

Configuration options
---------------------

.. include:: /options/ads_options.rst
