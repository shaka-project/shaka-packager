* Single file MP4 output with DASH + HLS::

    $ packager \
      in=h264_baseline_360p_600.mp4,stream=audio,output=audio.mp4,playlist_name=audio.m3u8,hls_group_id=audio,hls_name=ENGLISH \
      in=h264_baseline_360p_600.mp4,stream=video,output=h264_360p.mp4,playlist_name=h264_360p.m3u8,iframe_playlist_name=h264_360p_iframe.m3u8 \
      in=h264_main_480p_1000.mp4,stream=video,output=h264_480p.mp4,playlist_name=h264_480p.m3u8,iframe_playlist_name=h264_480p_iframe.m3u8 \
      in=h264_main_720p_3000.mp4,stream=video,output=h264_720p.mp4,playlist_name=h264_720p.m3u8,iframe_playlist_name=h264_720p_iframe.m3u8 \
      in=h264_high_1080p_6000.mp4,stream=video,output=h264_1080p.mp4,playlist_name=h264_1080p.m3u8,iframe_playlist_name=h264_1080p_iframe.m3u8 \
      --hls_master_playlist_output h264_master.m3u8 \
      --mpd_output h264.mpd

The above packaging command creates five single file MP4 streams, and HLS
playlists as well as DASH manifests.

* Output DASH + HLS with dash_only and hls_only options::

    $ packager \
      'in=h264_baseline_360p_600.mp4,stream=audio,init_segment=audio/init.mp4,segment_template=audio/$Number$.m4s' \
      'in=input_text.vtt,stream=text,init_segment=text/init.mp4,segment_template=text/$Number$.m4s,dash_only=1' \
      'in=input_text.vtt,stream=text,segment_template=text/$Number$.vtt,hls_only=1' \
      'in=h264_baseline_360p_600.mp4,stream=video,init_segment=h264_360p/init.mp4,segment_template=h264_360p/$Number$.m4s' \
      'in=h264_main_480p_1000.mp4,stream=video,init_segment=h264_480p/init.mp4,segment_template=h264_480p/$Number$.m4s' \
      'in=h264_main_720p_3000.mp4,stream=video,init_segment=h264_720p/init.mp4,segment_template=h264_720p/$Number$.m4s' \
      'in=h264_high_1080p_6000.mp4,stream=video,init_segment=h264_1080p/init.mp4,segment_template=h264_1080p/$Number$.m4s' \
      --generate_static_live_mpd --mpd_output h264.mpd \
      --hls_master_playlist_output h264_master.m3u8

The above packaging command creates HLS playlists and DASH manifest while using
dash_only for creating segmented WebVTT in mp4 format and hls_only option for
creating WebVTT in text format.

