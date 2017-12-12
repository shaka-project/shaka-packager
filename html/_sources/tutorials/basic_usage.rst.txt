Basic Usage
===========

Getting help
------------

::

    $ packager --help

Media file analysis
-------------------

Shaka Packager can be used to inspect the content of a media file and dump basic
stream information::

    $ packager input=some_content.mp4 --dump_stream_info

The output looks like::

    File "some_content.mp4":
    Found 2 stream(s).
    Stream [0] type: Video
     codec_string: avc1.4d001e
     time_scale: 24000
     duration: 3002000 (125.1 seconds)
     is_encrypted: false
     codec: H264
     width: 720
     height: 360
     pixel_aspect_ratio: 8:9
     trick_play_factor: 0
     nalu_length_size: 4

    Stream [1] type: Audio
     codec_string: mp4a.40.2
     time_scale: 44100
     duration: 5517311 (125.1 seconds)
     is_encrypted: false
     codec: AAC
     sample_bits: 16
     num_channels: 2
     sampling_frequency: 44100
     language: eng

Basic transmuxing
-----------------

Shaka Packager can be used to extract streams, optionally transmuxes the streams
from one container format to another container format.

Here is a simple command that extracts video and audio from the input file::

    $ packager in=some_content.mp4,stream=video,out=video.mp4 \
               in=some_content.mp4,stream=audio,out=audio.mp4

Shaka Packager is also capable of more complex tasks, such as applying
encryption, packaging contents to DASH or HLS formats, etc. Refer to
:doc:`tutorials`.
