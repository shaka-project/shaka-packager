Widevine
========

There are two options to package a Widevine DRM encrypted content:

1. If you know the encryption keys and have the associated Widevine PSSH at
   hand, you can provide them in clear text to *packager* directly. Refer to
   :doc:`/tutorials/raw_key` for details.

2. Provide *key_server_url* and associated credentials to *packager*.
   *Packager* will fetch encryption keys from Widevine key server.

Synopsis
--------

AES signing::

    $ packager <stream_descriptor> ... \
      --enable_widevine_encryption \
      --key_server_url <key_server_url> \
      --content_id <content_id> \
      --signer <signer> --aes_signing_key <aes_signing_key> \
      --aes_signing_iv <aes_signing_iv> \
      [Other options, e.g. DASH options, HLS options]

RSA signing::

    $ packager <stream_descriptor> ... \
      --enable_widevine_encryption \
      --key_server_url <key_server_url> \
      --content_id <content_id> \
      --signer <signer> --rsa_signing_key_path <rsa_signing_key_path> \
      [Other options, e.g. DASH options, HLS options]

Examples
--------

The examples below uses the H264 streams created in :doc:`encoding`.

* Here is an example with both DASH and HLS output::

    $ packager \
      in=h264_baseline_360p_600.mp4,stream=audio,output=audio.mp4 \
      in=h264_baseline_360p_600.mp4,stream=video,output=h264_360p.mp4 \
      in=h264_main_480p_1000.mp4,stream=video,output=h264_480p.mp4 \
      in=h264_main_720p_3000.mp4,stream=video,output=h264_720p.mp4 \
      in=h264_high_1080p_6000.mp4,stream=video,output=h264_1080p.mp4 \
      --mpd_output h264.mpd \
      --hls_master_playlist_output h264_master.m3u8 \
      --enable_widevine_encryption \
      --key_server_url https://license.uat.widevine.com/cenc/getcontentkey/widevine_test \
      --content_id 7465737420636f6e74656e74206964 \
      --signer widevine_test \
      --aes_signing_key 1ae8ccd0e7985cc0b6203a55855a1034afc252980e970ca90e5202689f947ab9 \
      --aes_signing_iv d58ce954203b7c9a9a9d467f59839249

* Another example using 'cbcs' protection scheme::

    $ packager \
      in=h264_baseline_360p_600.mp4,stream=audio,output=audio.mp4 \
      in=h264_baseline_360p_600.mp4,stream=video,output=h264_360p.mp4 \
      in=h264_main_480p_1000.mp4,stream=video,output=h264_480p.mp4 \
      in=h264_main_720p_3000.mp4,stream=video,output=h264_720p.mp4 \
      in=h264_high_1080p_6000.mp4,stream=video,output=h264_1080p.mp4 \
      --mpd_output h264.mpd \
      --hls_master_playlist_output h264_master.m3u8 \
      --protection_scheme cbcs \
      --enable_widevine_encryption \
      --key_server_url https://license.uat.widevine.com/cenc/getcontentkey/widevine_test \
      --content_id 7465737420636f6e74656e74206964 \
      --signer widevine_test \
      --aes_signing_key 1ae8ccd0e7985cc0b6203a55855a1034afc252980e970ca90e5202689f947ab9 \
      --aes_signing_iv d58ce954203b7c9a9a9d467f59839249

* One example with multi-DRM support (PlayReady in addition to Widevine)::

    $ packager \
      in=h264_baseline_360p_600.mp4,stream=audio,output=audio.mp4 \
      in=h264_baseline_360p_600.mp4,stream=video,output=h264_360p.mp4 \
      in=h264_main_480p_1000.mp4,stream=video,output=h264_480p.mp4 \
      in=h264_main_720p_3000.mp4,stream=video,output=h264_720p.mp4 \
      in=h264_high_1080p_6000.mp4,stream=video,output=h264_1080p.mp4 \
      --mpd_output h264.mpd \
      --hls_master_playlist_output h264_master.m3u8 \
      --enable_widevine_encryption \
      --key_server_url https://license.uat.widevine.com/cenc/getcontentkey/widevine_test \
      --content_id 7465737420636f6e74656e74206964 \
      --signer widevine_test \
      --aes_signing_key 1ae8ccd0e7985cc0b6203a55855a1034afc252980e970ca90e5202689f947ab9 \
      --aes_signing_iv d58ce954203b7c9a9a9d467f59839249 \
      --additional_protection_systems PlayReady

.. note::

    User is responsible for setting up the PlayReady server and managing keys
    there.

Refer to
`player setup <https://shaka-player-demo.appspot.com/docs/api/tutorial-drm-config.html>`_
on how to config the DRM in Shaka Player.

Widevine test credential
------------------------

Here is the test crendential used in this tutorial.

:key_server_url:

    https://license.uat.widevine.com/cenc/getcontentkey/widevine_test

:signer:

    widevine_test

:aes_signing_key:

    1ae8ccd0e7985cc0b6203a55855a1034afc252980e970ca90e5202689f947ab9

:aes_signing_iv:

    d58ce954203b7c9a9a9d467f59839249

.. note::

    The test credential is only meant for development. Please reach out to
    `Widevine <https://support.google.com/widevine/troubleshooter/6027072>`_ if
    you need something for production use.

.. include:: /options/drm_stream_descriptors.rst
.. include:: /options/general_encryption_options.rst
.. include:: /options/widevine_encryption_options.rst
