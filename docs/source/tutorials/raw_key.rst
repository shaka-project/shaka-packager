Using Raw Key
=============

Shaka Packager supports raw keys, for which keys and key_ids are provided to
Shaka Packager directly.

This is often used if you are managing the encryption keys yourself. It also
allows you to support multi-DRM by providing custom PSSHs.

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

Custom PSSH(s) can be provided in *--pssh*. If neither --pssh nor
--protection_systems is specified, `v1 common PSSH box <https://goo.gl/s8RIhr>`_
is generated.

Examples
--------

The examples below use the H264 streams created in :doc:`encoding`.

* Example with pre-generated PSSH::

    $ packager \
      in=h264_baseline_360p_600.mp4,stream=audio,output=audio.mp4,drm_label=AUDIO \
      in=h264_baseline_360p_600.mp4,stream=video,output=h264_360p.mp4,drm_label=SD \
      in=h264_main_480p_1000.mp4,stream=video,output=h264_480p.mp4,drm_label=SD \
      in=h264_main_720p_3000.mp4,stream=video,output=h264_720p.mp4,drm_label=HD \
      in=h264_high_1080p_6000.mp4,stream=video,output=h264_1080p.mp4,drm_label=HD \
      --enable_raw_key_encryption \
      --keys label=AUDIO:key_id=f3c5e0361e6654b28f8049c778b23946:key=a4631a153a443df9eed0593043db7519,label=SD:key_id=abba271e8bcf552bbd2e86a434a9a5d9:key=69eaa802a6763af979e8d1940fb88392,label=HD:key_id=6d76f25cb17f5e16b8eaef6bbf582d8e:key=cb541084c99731aef4fff74500c12ead \
      --pssh 000000317073736800000000EDEF8BA979D64ACEA3C827DCD51D21ED00000011220F7465737420636F6E74656E74206964 \
      --mpd_output h264.mpd \
      --hls_master_playlist_output h264_master.m3u8

* Common PSSH is generated if no PSSH or protection system flag is specified::

    $ packager \
      in=h264_baseline_360p_600.mp4,stream=audio,output=audio.mp4,drm_label=AUDIO \
      in=h264_baseline_360p_600.mp4,stream=video,output=h264_360p.mp4,drm_label=SD \
      in=h264_main_480p_1000.mp4,stream=video,output=h264_480p.mp4,drm_label=SD \
      in=h264_main_720p_3000.mp4,stream=video,output=h264_720p.mp4,drm_label=HD \
      in=h264_high_1080p_6000.mp4,stream=video,output=h264_1080p.mp4,drm_label=HD \
      --enable_raw_key_encryption \
      --keys label=AUDIO:key_id=f3c5e0361e6654b28f8049c778b23946:key=a4631a153a443df9eed0593043db7519,label=SD:key_id=abba271e8bcf552bbd2e86a434a9a5d9:key=69eaa802a6763af979e8d1940fb88392,label=HD:key_id=6d76f25cb17f5e16b8eaef6bbf582d8e:key=cb541084c99731aef4fff74500c12ead \
      --mpd_output h264.mpd

* Example with FairPlay using 'cbcs' protection scheme::

    $ packager \
      in=h264_baseline_360p_600.mp4,stream=audio,output=audio.mp4,drm_label=AUDIO \
      in=h264_baseline_360p_600.mp4,stream=video,output=h264_360p.mp4,drm_label=SD \
      in=h264_main_480p_1000.mp4,stream=video,output=h264_480p.mp4,drm_label=SD \
      in=h264_main_720p_3000.mp4,stream=video,output=h264_720p.mp4,drm_label=HD \
      in=h264_high_1080p_6000.mp4,stream=video,output=h264_1080p.mp4,drm_label=HD \
      --protection_scheme cbcs \
      --enable_raw_key_encryption \
      --keys label=AUDIO:key_id=f3c5e0361e6654b28f8049c778b23946:key=a4631a153a443df9eed0593043db7519,label=SD:key_id=abba271e8bcf552bbd2e86a434a9a5d9:key=69eaa802a6763af979e8d1940fb88392,label=HD:key_id=6d76f25cb17f5e16b8eaef6bbf582d8e:key=cb541084c99731aef4fff74500c12ead \
      --protection_systems FairPlay \
      --iv 11223344556677889900112233445566
      --hls_master_playlist_output h264_master.m3u8 \
      --hls_key_uri skd://testAssetID

* Example with multi-drm (Widevine and PlayReady)::

    $ packager \
      in=h264_baseline_360p_600.mp4,stream=audio,output=audio.mp4,drm_label=AUDIO \
      in=h264_baseline_360p_600.mp4,stream=video,output=h264_360p.mp4,drm_label=SD \
      in=h264_main_480p_1000.mp4,stream=video,output=h264_480p.mp4,drm_label=SD \
      in=h264_main_720p_3000.mp4,stream=video,output=h264_720p.mp4,drm_label=HD \
      in=h264_high_1080p_6000.mp4,stream=video,output=h264_1080p.mp4,drm_label=HD \
      --enable_raw_key_encryption \
      --keys label=AUDIO:key_id=f3c5e0361e6654b28f8049c778b23946:key=a4631a153a443df9eed0593043db7519,label=SD:key_id=abba271e8bcf552bbd2e86a434a9a5d9:key=69eaa802a6763af979e8d1940fb88392,label=HD:key_id=6d76f25cb17f5e16b8eaef6bbf582d8e:key=cb541084c99731aef4fff74500c12ead \
      --protection_systems Widevine,PlayReady \
      --mpd_output h264.mpd

.. note::

    Users are responsible for setting up the license servers and managing keys
    there unless they are using a cloud service provided by the DRM provider or
    third_parties.

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

    The PSSH 00000031707373... is generated using
    `pssh-box script <https://github.com/google/shaka-packager/tree/master/packager/tools/pssh>`_::

        $ pssh-box.py --widevine-system-id \
          --content-id 7465737420636f6e74656e74206964 --hex

Configuration options
---------------------

.. include:: /options/drm_stream_descriptors.rst
.. include:: /options/general_encryption_options.rst
.. include:: /options/raw_key_encryption_options.rst

pssh-box (Utility to generate PSSH boxes)
-----------------------------------------

https://github.com/google/shaka-packager/tree/master/packager/tools/pssh
