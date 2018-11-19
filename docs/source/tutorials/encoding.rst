Media Encoding
--------------

Shaka Packager does not do transcoding internally. The contents need to be
pre-encoded before passing to Shaka Packager.

General guidelines of how contents should be encoded
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- Encode multiple bitrates or resolutions of the same content. Shaka Packager
  can then package the content into DASH / HLS formats, allowing different
  bitrates of the content to be served for different network conditions,
  achieving adaptive bitrate streaming.
- Not a must, but the multibirate content is recommended to have aligned GOPs
  across the different bitrate streams. This makes bitrate switching easier and
  smoother.
- We recommend setting GOP size to 5s or less. The streams are usually
  switchable only at GOP boundaries. A smaller GOP size results in faster
  switching when network condition changes.
- In the same stream, the bitrate should be more or less the same in the
  inter-GOP level.

Sample commands to generate multi-bitrate content
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Let us say we have a 1080p original content `original.mp4` containing an audio
track in `AAC` and a video track in `H264`. The frame rate is 24. We want to
encode the contents into four resolutions: 360p, 480p, 720p and 1080p with GOP
size 72, i.e. 3 seconds.

We use `ffmpeg <https://www.ffmpeg.org/>`_ here, which is a common tool used for
transcoding.

H264 encoding
"""""""""""""

* 360p::

    $ ffmpeg -i original.mp4 -c:a copy \
      -vf "scale=-2:360" \
      -c:v libx264 -profile:v baseline -level:v 3.0 \
      -x264-params scenecut=0:open_gop=0:min-keyint=72:keyint=72 \
      -minrate 600k -maxrate 600k -bufsize 600k -b:v 600k \
      -y h264_baseline_360p_600.mp4

* 480p::

    $ ffmpeg -i original.mp4 -c:a copy \
      -vf "scale=-2:480" \
      -c:v libx264 -profile:v main -level:v 3.1 \
      -x264-params scenecut=0:open_gop=0:min-keyint=72:keyint=72 \
      -minrate 1000k -maxrate 1000k -bufsize 1000k -b:v 1000k \
      -y h264_main_480p_1000.mp4

* 720p::

    $ ffmpeg -i original.mp4 -c:a copy \
      -vf "scale=-2:720" \
      -c:v libx264 -profile:v main -level:v 4.0 \
      -x264-params scenecut=0:open_gop=0:min-keyint=72:keyint=72 \
      -minrate 3000k -maxrate 3000k -bufsize 3000k -b:v 3000k \
      -y h264_main_720p_3000.mp4

* 1080p::

    $ ffmpeg -i original.mp4 -c:a copy \
      -vf "scale=-2:1080" \
      -c:v libx264 -profile:v high -level:v 4.2 \
      -x264-params scenecut=0:open_gop=0:min-keyint=72:keyint=72 \
      -minrate 6000k -maxrate 6000k -bufsize 6000k -b:v 6000k \
      -y h264_high_1080p_6000.mp4

VP9 encoding
""""""""""""

The audio is encoded into `opus`.

* 360p::

    $ ffmpeg -i original.mp4 \
      -strict -2 -c:a opus \
      -vf "scale=-2:360" \
      -c:v libvpx-vp9 -profile:v 0 \
      -keyint_min 72 -g 72 \
      -tile-columns 4 -frame-parallel 1 -speed 1 \
      -auto-alt-ref 1 -lag-in-frames 25 \
      -b:v 300k \
      -y vp9_360p_300.webm

* 480p::

    $ ffmpeg -i original.mp4 \
      -strict -2 -c:a opus \
      -vf "scale=-2:480" \
      -c:v libvpx-vp9 -profile:v 0 \
      -keyint_min 72 -g 72 \
      -tile-columns 4 -frame-parallel 1 -speed 1 \
      -auto-alt-ref 1 -lag-in-frames 25 \
      -b:v 500k \
      -y vp9_480p_500.webm

* 720p::

    $ ffmpeg -i original.mp4 \
      -strict -2 -c:a opus \
      -vf "scale=-2:720" \
      -c:v libvpx-vp9 -profile:v 0 \
      -keyint_min 72 -g 72 \
      -tile-columns 4 -frame-parallel 1 -speed 1 \
      -auto-alt-ref 1 -lag-in-frames 25 \
      -b:v 1500k \
      -y vp9_720p_1500.webm

* 1080p::

    $ ffmpeg -i original.mp4 \
      -strict -2 -c:a opus \
      -vf "scale=-2:1080" \
      -c:v libvpx-vp9 -profile:v 0 \
      -keyint_min 72 -g 72 \
      -tile-columns 4 -frame-parallel 1 -speed 1 \
      -auto-alt-ref 1 -lag-in-frames 25 \
      -b:v 3000k \
      -y vp9_1080p_3000.webm
