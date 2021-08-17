####################################
Low Latency DASH (LL-DASH) Streaming
####################################

************
Introduction
************

If ``--low_latency_dash_mode`` is enabled, low latency DASH (LL-DASH) packaging will be used.

This will reduce overall latency by ensuring that the media segments are chunk encoded and delivered via an aggregating response.
The combination of these features will ensure that overall latency can be decoupled from the segment duration.
For low latency to be achieved, the output of Shaka Packager must be combined with a delivery system which can chain together a set of aggregating responses, such as chunked transfer encoding under HTTP/1.1 or a HTTP/2 or HTTP/3 connection.
The output of Shaka Packager must be played with a DASH client that understands the availabilityTimeOffset MPD value.
Furthermore, the player should also understand the throughput estimation and ABR challenges that arise when operating in the low latency regime.

This tutorial covers LL-DASH packaging and uses features from the DASH, HTTP upload, and FFmpeg piping tutorials.
For more information on DASH, see :doc:`dash`; for HTTP upload, see :doc:`http_upload`;
for FFmpeg piping, see :doc:`ffmpeg_piping`;
for full documentation, see :doc:`/documentation`.

*************
Documentation
*************

Getting started
===============

To enable LL-DASH mode, set the ``--low_latency_dash_mode`` flag to ``true``. 

All HTTP requests will use chunked transfer encoding:
``Transfer-Encoding: chunked``.

.. note::

    Only LL-DASH is supported. LL-HLS support is yet to come.

Synopsis
========

Here is a basic example of the LL-DASH support. 
The LL-DASH setup borrows features from "FFmpeg piping" and "HTTP upload",
see :doc:`ffmpeg_piping` and :doc:`http_upload`.

Define UNIX pipe to connect ffmpeg with packager::

    export PIPE=/tmp/bigbuckbunny.fifo
    mkfifo ${PIPE}

Acquire and transcode RTMP stream::

    ffmpeg -fflags nobuffer -threads 0 -y \
        -i rtmp://184.72.239.149/vod/mp4:bigbuckbunny_450.mp4 \
        -pix_fmt yuv420p -vcodec libx264 -preset:v superfast -acodec aac \
        -f mpegts pipe: > ${PIPE}

Configure and run packager::

    # Define upload URL
    export UPLOAD_URL=http://localhost:6767/ll-dash

    # Go
    packager \
        "input=${PIPE},stream=audio,init_segment=${UPLOAD_URL}_init.m4s,segment_template=${UPLOAD_URL}/bigbuckbunny-audio-aac-\$Number%04d\$.m4s" \
        "input=${PIPE},stream=video,init_segment=${UPLOAD_URL}_init.m4s,segment_template=${UPLOAD_URL}/bigbuckbunny-video-h264-450-\$Number%04d\$.m4s" \
        --io_block_size 65536 \
        --segment_duration 2 \
        --low_latency_dash_mode=true \
        --utc_timings "urn:mpeg:dash:utc:http-xsdate:2014"="https://time.akamai.com/?iso" \
        --mpd_output "${UPLOAD_URL}/bigbuckbunny.mpd" \


*************************
Low Latency Compatibility
*************************

For low latency to be achieved, the processes handling Shaka Packager's output, such as the server and player,
must support LL-DASH streaming.

Delivery Pipeline
=================
Shaka Packager will upload the LL-DASH content to the specified output via HTTP chunked transfer encoding.
The server must have the ability to handle this type of request. If using a proxy or shim for cloud authentication,
these services must also support HTTP chunked transfer encoding.

Examples of supporting content delivery systems:

* `AWS MediaStore <https://aws.amazon.com/mediastore/>`_

Player
======
The player must support LL-DASH playout.
LL-DASH requires the player to be able to interpret ``availabilityTimeOffset`` values from the DASH MPD.
The player should also recognize the the throughput estimation and ABR challenges that arise with low latency streaming.

Examples of supporting players:

* `Shaka Player <https://github.com/google/shaka-player>`_
* `dash.js <https://github.com/Dash-Industry-Forum/dash.js>`_