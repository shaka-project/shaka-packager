    🛠 Status: Work in progress

    The Shaka Packager HTTP upload feature is currently in development.
    It's on the fast track to a working alpha, so we encourage you to use
    it and give us some feedback. However, there are things that haven't
    been finalized yet so you can expect some changes.

    This document describes the current state of the implementation,
    contributions are always welcome.

    The discussion about this feature currently happens at
    `Add HTTP PUT output #149 <https://github.com/google/shaka-packager/issues/149>`_.

###########
HTTP upload
###########


************
Introduction
************
Shaka Packager can upload produced artefacts to a HTTP server using
HTTP PUT requests with chunked transfer encoding to improve live
publishing performance when content is not served directly from
the packaging output location. For talking HTTP, libcurl_ is used.

The produced artefacts are:

- HLS_ playlist files in M3U_ Format encoded with UTF-8 (.m3u8)
- Chunked audio segments encoded with AAC (.aac)
- Chunked video segments encapsulated into the
  `MPEG transport stream`_ container format (.ts)

References
==========
- `RFC 2616 about HTTP PUT`_
- `RFC 2616 about Chunked Transfer Coding`_


*************
Documentation
*************

Getting started
===============
To enable the HTTP upload transfer mode, use ``https:`` file paths for any
output files (e.g. ``segment_template``).

All HTTP requests will be declared as
``Content-Type: application/octet-stream``.

Synopsis
========
Here is a basic example. It is similar to the "live" example and also
borrows features from "FFmpeg piping", see :doc:`live` and :doc:`ffmpeg_piping`.

Define UNIX pipe to connect ffmpeg with packager::

    export PIPE=/tmp/bigbuckbunny.fifo
    mkfifo ${PIPE}

Acquire and transcode RTMP stream::

    # Steady
    ffmpeg -fflags nobuffer -threads 0 -y \
        -i rtmp://184.72.239.149/vod/mp4:bigbuckbunny_450.mp4 \
        -pix_fmt yuv420p -vcodec libx264 -preset:v superfast -acodec aac \
        -f mpegts pipe: > ${PIPE}

Configure and run packager::

    # Define upload URL
    export UPLOAD_URL=http://localhost:6767/hls-live

    # Go
    packager \
        "input=${PIPE},stream=audio,segment_template=${UPLOAD_URL}/bigbuckbunny-audio-aac-\$Number%04d\$.aac,playlist_name=bigbuckbunny-audio.m3u8,hls_group_id=audio" \
        "input=${PIPE},stream=video,segment_template=${UPLOAD_URL}/bigbuckbunny-video-h264-450-\$Number%04d\$.ts,playlist_name=bigbuckbunny-video-450.m3u8" \
        --io_block_size 65536 --fragment_duration 2 --segment_duration 2 \
        --time_shift_buffer_depth 3600 --preserved_segments_outside_live_window 7200 \
        --hls_master_playlist_output "${UPLOAD_URL}/bigbuckbunny.m3u8" \
        --hls_playlist_type LIVE \
        --vmodule=http_file=1

*********************
Client Authentication
*********************
If your server requires client authentication, you can add the following
arguments to enable it:

- ``--ca_file``: (optional) Absolute path to the Certificate Authority file for
  the server cert. PEM format.
- ``--client_cert_file``: Absolute path to client certificate file.
- ``--client_cert_private_key_file``: Absolute path to the private Key file.
- ``--client_cert_private_key_password``: (optional) Password to the private
  key file.

*******
Backlog
*******
Please note the HTTP upload feature still lacks some features
probably important for production. Contributions are welcome!

HTTP DELETE
===========
Nothing has be done to support this yet:

    Packager supports removing old segments automatically.
    See ``preserved_segments_outside_live_window`` option in
    DASH_ options or HLS_ options for details.

Software tests
==============
We should do some minimal QA, check whether the test
suite breaks and maybe add some tests covering new code.

Network timeouts
================
libcurl_ can apply network timeout settings. However,
we haven't addressed this yet.

Miscellaneous
=============
- Address all things TODO and FIXME
- Make ``io_cache_size`` configurable?


***************
Example Backend
***************

HTTP PUT file uploads to Nginx
==============================
The receiver is based on the native Nginx_ module "`ngx_http_dav_module`_",
it handles HTTP PUT requests with chunked transfer encoding
like emitted by Shaka Packager.

The configuration is very simple::

    server {
        listen 6767 default_server;

        access_log  /dev/stdout combined;
        error_log   /dev/stdout info;

        root /var/spool;
        location ~ ^/hls-live/(.+)$ {

            dav_methods PUT;
            create_full_put_path on;

            proxy_buffering off;
            client_max_body_size 20m;

        }

    }

Run Nginx::

    nginx -p `pwd` -c nginx.conf -g "daemon off;"


HTTP PUT file uploads to Caddy
==============================
The receiver is based on the Caddy_ webserver, it handles HTTP PUT
requests with chunked transfer encoding like emitted by Shaka Packager.

Put this configuration into a `Caddyfile`::

    # Bind address
    :6767

    # Enable logging
    log stdout

    # Web server root with autoindex
    root /var/spool
    redir /hls-live {
        if {path} is "/"
    }
    browse

    # Enable upload with HTTP PUT
    upload /hls-live {
        to "/var/spool/hls-live"
    }

Run Caddy::

    caddy -conf Caddyfile


*************************
Development and debugging
*************************

Watch the network::

    ngrep -Wbyline -dlo port 6767

Grab and run `httpd-reflector.py`_ to use it as a dummy HTTP sink::

    # Ready
    wget https://gist.githubusercontent.com/amotl/3ed38e461af743aeeade5a5a106c1296/raw/httpd-reflector.py
    chmod +x httpd-reflector.py
    ./httpd-reflector.py --port 6767


----

.. _HLS: https://en.wikipedia.org/wiki/HTTP_Live_Streaming
.. _DASH: https://en.wikipedia.org/wiki/Dynamic_Adaptive_Streaming_over_HTTP
.. _M3U: https://en.wikipedia.org/wiki/M3U
.. _MPEG transport stream: https://en.wikipedia.org/wiki/MPEG_transport_stream
.. _libcurl: https://curl.haxx.se/libcurl/
.. _RFC 1867: https://tools.ietf.org/html/rfc1867
.. _RFC 2616 about HTTP PUT: https://www.w3.org/Protocols/rfc2616/rfc2616-sec9.html#sec9.6
.. _RFC 2616 about Chunked Transfer Coding: https://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.6.1
.. _RFC 5789: https://tools.ietf.org/html/rfc5789
.. _Nginx: http://nginx.org/
.. _ngx_http_dav_module: http://nginx.org/en/docs/http/ngx_http_dav_module.html
.. _Caddy: https://caddyserver.com/
.. _httpd-reflector.py: https://gist.github.com/amotl/3ed38e461af743aeeade5a5a106c1296

.. _@colleenkhenry: https://github.com/colleenkhenry
.. _@kqyang: https://github.com/kqyang
