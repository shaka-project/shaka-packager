Raw key
=======

*Packager* allows encrypting contents with raw key.

Synopsis
--------

::

    $ packager <stream_descriptor> ... \
      --enable_raw_key_encryption \
      --keys <key_info_string>[,<key_info_string>]... \
      [--pssh <concatenated_PSSHs>] \
      [Other options, e.g. DASH options, HLS options]

**key_info_string** is of the form::

    label=<label>:key_id=<key_id>:key=<key>

Custom PSSH(s) can be provided in *--pssh*. If absent,
`v1 common PSSH box <https://goo.gl/s8RIhr>`_ is generated.

Examples
--------

The examples below uses the H264 streams created in :doc:`encoding`. Here are
examples with DASH. It can be applied to HLS in a similar way.

Common PSSH (one key for all streams)::

    $ packager \
      in=h264_baseline_360p_600.mp4,stream=audio,output=audio.mp4 \
      in=h264_baseline_360p_600.mp4,stream=video,output=h264_360p.mp4 \
      in=h264_main_480p_1000.mp4,stream=video,output=h264_480p.mp4 \
      in=h264_main_720p_3000.mp4,stream=video,output=h264_720p.mp4 \
      in=h264_high_1080p_6000.mp4,stream=video,output=h264_1080p.mp4 \
      --enable_raw_key_encryption \
      --keys label=:key_id=abba271e8bcf552bbd2e86a434a9a5d9:key=69eaa802a6763af979e8d1940fb88392 \
      --mpd_output h264.mpd

Common PSSH (different keys for different streams)::

    $ packager \
      in=h264_baseline_360p_600.mp4,stream=audio,output=audio.mp4,drm_label=AUDIO \
      in=h264_baseline_360p_600.mp4,stream=video,output=h264_360p.mp4,drm_label=SD \
      in=h264_main_480p_1000.mp4,stream=video,output=h264_480p.mp4,drm_label=SD \
      in=h264_main_720p_3000.mp4,stream=video,output=h264_720p.mp4,drm_label=HD \
      in=h264_high_1080p_6000.mp4,stream=video,output=h264_1080p.mp4,drm_label=HD \
      --enable_raw_key_encryption \
      --keys label=AUDIO:key_id=f3c5e0361e6654b28f8049c778b23946:key=a4631a153a443df9eed0593043db7519,label=SD:key_id=abba271e8bcf552bbd2e86a434a9a5d9:key=69eaa802a6763af979e8d1940fb88392,label=HD:key_id=6d76f25cb17f5e16b8eaef6bbf582d8e:key=cb541084c99731aef4fff74500c12ead \
      --mpd_output h264.mpd

Widevine PSSH (with pre-generated Widevine PSSH)::

    $ packager \
      in=h264_baseline_360p_600.mp4,stream=audio,output=audio.mp4,drm_label=AUDIO \
      in=h264_baseline_360p_600.mp4,stream=video,output=h264_360p.mp4,drm_label=SD \
      in=h264_main_480p_1000.mp4,stream=video,output=h264_480p.mp4,drm_label=SD \
      in=h264_main_720p_3000.mp4,stream=video,output=h264_720p.mp4,drm_label=HD \
      in=h264_high_1080p_6000.mp4,stream=video,output=h264_1080p.mp4,drm_label=HD \
      --enable_raw_key_encryption \
      --keys label=AUDIO:key_id=f3c5e0361e6654b28f8049c778b23946:key=a4631a153a443df9eed0593043db7519,label=SD:key_id=abba271e8bcf552bbd2e86a434a9a5d9:key=69eaa802a6763af979e8d1940fb88392,label=HD:key_id=6d76f25cb17f5e16b8eaef6bbf582d8e:key=cb541084c99731aef4fff74500c12ead \
      --pssh 000000407073736800000000edef8ba979d64acea3c827dcd51d21ed000000201a0d7769646576696e655f74657374220f7465737420636f6e74656e74206964 \
      --mpd_output h264.mpd

Widevine PSSH and PlayReady PSSH::

    $ packager \
      in=h264_baseline_360p_600.mp4,stream=audio,output=audio.mp4,drm_label=AUDIO \
      in=h264_baseline_360p_600.mp4,stream=video,output=h264_360p.mp4,drm_label=SD \
      in=h264_main_480p_1000.mp4,stream=video,output=h264_480p.mp4,drm_label=SD \
      in=h264_main_720p_3000.mp4,stream=video,output=h264_720p.mp4,drm_label=HD \
      in=h264_high_1080p_6000.mp4,stream=video,output=h264_1080p.mp4,drm_label=HD \
      --enable_raw_key_encryption \
      --keys label=AUDIO:key_id=f3c5e0361e6654b28f8049c778b23946:key=a4631a153a443df9eed0593043db7519,label=SD:key_id=abba271e8bcf552bbd2e86a434a9a5d9:key=69eaa802a6763af979e8d1940fb88392,label=HD:key_id=6d76f25cb17f5e16b8eaef6bbf582d8e:key=cb541084c99731aef4fff74500c12ead \
      --generate_widevine_pssh \
      --generate_playready_pssh \
      --mpd_output h264.mpd

Note that user is responsible for setting up Widevine and PlayReady license server
and managing keys there.

Refer to
`player setup <https://shaka-player-demo.appspot.com/docs/api/tutorial-drm-config.html>`_
on how to config the DRM in Shaka Player.

Test vectors used in this tutorial
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:Key ID:

    | SD:    abba271e8bcf552bbd2e86a434a9a5d9
    | HD:    6d76f25cb17f5e16b8eaef6bbf582d8e
    | AUDIO: f3c5e0361e6654b28f8049c778b23946

    Key ID must be 16 bytes or 32 digits in HEX.

:Key:

    | SD:    69eaa802a6763af979e8d1940fb88392
    | HD:    cb541084c99731aef4fff74500c12ead
    | AUDIO: a4631a153a443df9eed0593043db7519

    Key must be 16 bytes or 32 digits in HEX.

:Widevine PSSH:

    000000407073736800000000edef8ba979d64acea3c827dcd51d21ed000000201a0d7769646576696e655f74657374220f7465737420636f6e74656e74206964

    The PSSH is generated using
    `pssh-box script <https://github.com/google/shaka-packager/tree/master/packager/tools/pssh>`_::

        $ pssh-box.py --widevine-system-id \
          --content-id 7465737420636f6e74656e74206964 --provider widevine_test

.. include:: /options/drm_stream_descriptors.rst
.. include:: /options/general_encryption_options.rst
.. include:: /options/raw_key_encryption_options.rst

pssh-box (Utility to generate PSSH boxes)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

https://github.com/google/shaka-packager/tree/master/packager/tools/pssh
