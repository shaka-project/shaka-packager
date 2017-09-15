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
      --hls_master_playlist_output h264_master.m3u8

.. note::

    Packager does not support removing old segments internally. The user is
    resposible for setting up a cron job to do so.

.. include:: /options/udp_file_options.rst

.. include:: /options/segment_template_formatting.rst
