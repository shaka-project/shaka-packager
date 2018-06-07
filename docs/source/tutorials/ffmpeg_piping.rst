FFmpeg piping
=============

We can use *FFmpeg* to redirect / pipe input not supported by *packager*
to *packager*, for example, input from webcam devices, or rtp input. The concept
depicted here can be applied to other *FFmpeg* supported device or protocols.

Piping data to packager
-----------------------

There are two options to pipe data to packager.

- UDP socket

    *FFmpeg* supports writing to a UDP socket and *packager* supports reading
    from UDP sockets (See :doc:`/options/udp_file_options`)::

        $ packager 'input=udp://127.0.0.1:40000,...' ...
        $ ffmpeg ... -f mpegts udp://127.0.0.1:40000

    VP9 cannot be carried in mpegts. Another container, e.g. webm needs to be
    used when outputs VP9. In this case, transcoding has to be started after
    starting packager as the initialization segment is only transmitted in the
    beginning of WebM output.

- pipe

    Similarily, pipe is also supported in both *FFmpeg* and *packager*::

    $ mkfifo pipe1
    $ packager 'input=pipe1,...' ... --io_block_size 65536
    $ ffmpeg ... -f mpegts pipe: > pipe1

.. note::

    Option -io_block_size 65536 tells packager to use an io_block_size of 65K
    for threaded io file. This is necessary when using pipe as reading from pipe
    blocks until the specified number of bytes, which is specified in
    io_block_size for threaded io file, thus the value of io_block_size cannot
    be too large.

Encoding / capture command
--------------------------

Camera capture
^^^^^^^^^^^^^^

Refer to `FFmpeg Capture/Webcam <https://trac.ffmpeg.org/wiki/Capture/Webcam>`_
on how to use *FFmpeg* to capture webmcam inputs.

The example assumes Mac OS X 10.7 (Lion) or later. It captures from the default
audio / video devices on the machine::

    $ ffmpeg -f avfoundation -i "default" -f mpegts udp://127.0.0.1:40000

The command starts only after packager starts.

.. note::

    After encoding starts, monitor encoding speed carefully. It should always be
    1x and above. If not, adjust the encoding parameters to recude it.

RTP input
^^^^^^^^^

Assume there is an RTP input described by `saved_sdp_file`::

    $ ffmpeg -protocol_whitelist "file,rtp,udp" -i saved_sdp_file -vcodec h264 \
      -tune zerolatency -f mpegts udp://127.0.0.1:40000

.. note::

    For testing, you can generate an RTP input from a static media file::

        $ ffmpeg -re -stream_loop 100 -i <static.mp4> -vcodec copy -acodec \
          copy -f rtp rtp://239.255.0.1:1234 -sdp_file saved_sdp_file

The command starts only after packager starts.

.. note::

    After encoding starts, monitor encoding speed carefully. It should always be
    1x or above. If not, adjust the encoding parameters to increase it.

Example packaging command in DASH
---------------------------------

::

    $ packager \
      'in=udp://127.0.0.1:40000,stream=audio,init_segment=live_cam_audio.mp4,segment_template=live_cam_audio_$Number$.m4s' \
      'in=udp://127.0.0.1:40000,stream=video,init_segment=live_cam_video.mp4,segment_template=live_cam_video_$Number$.m4s' \
      --mpd_output live_cam.mpd

