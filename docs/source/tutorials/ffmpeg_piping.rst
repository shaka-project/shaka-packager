ffmpeg piping
=============

We can use *ffmpeg* to redirect / pipe input not supported by *packager*
to *packager*, for example, input from webcam devices. The example below uses
webcam input device. The concept depicted here can be applied to
other *ffmpeg* supported capture device.

ffmpeg camera capture
---------------------

Refer to `ffmpeg Capture/Webcam <https://trac.ffmpeg.org/wiki/Capture/Webcam>`_
on how to use *ffmpeg* to capture webmcam inputs.

The examples below assumes Mac OS X 10.7 (Lion) or later. It is similar on
other platforms. Refer to the above link for details.

Create pipe
-----------

We use pipe to connect *ffmpeg* and *packager*::

    $ mkfifo pipe1

Encoding / capture command
--------------------------

The below command captures from the default audio / video devices on the
machine::

    $ ffmpeg -f avfoundation -i "default" -f mpegts pipe: > pipe1

The command starts only after packager starts.

.. note::

    After encoding starts, monitor encoding speed carefully. It should always be
    1x and above. If not, adjust the encoding parameters to recude it.

Packaging command (DASH)
------------------------

::

    $ packager \
      'in=pipe1,stream=audio,init_segment=live_cam_audio.mp4,segment_template=live_cam_audio_$Number$.m4s' \
      'in=pipe1,stream=video,init_segment=live_cam_video.mp4,segment_template=live_cam_video_$Number$.m4s' \
      --mpd_output live_cam.mpd \
      --io_block_size 65536

.. note::

    Option -io_block_size 65536 tells packager to use an io_block_size of 65K
    for threaded io file. This is necessary as reading from pipe blocks until
    the specified number of bytes, which is specified in io_block_size for
    threaded io file, thus the value of io_block_size cannot be too large.
