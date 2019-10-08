Packager Documentation
======================

Shaka Packager is a tool and a media packaging SDK for DASH and HLS packaging
and encryption. It can transmux input media files from one container to another
container.

.. note::

    Shaka Packager does not do transcoding. Content must be pre-encoded before
    passing to packager.

Packager operates in *streams*, described by *stream_descriptor*. The streams
can be read from the same "file" or different "files", which can be regular
files, pipes, udp streams, etc.

This page is the documentation on using the *packager* tool. If you are
interested in integrating *packager* library into your own tool, please see
:doc:`library`.

Getting Shaka Packager
----------------------

There are several ways you can get Shaka Packager.

- Using `Docker <https://www.docker.com/whatisdocker>`_.
  Instructions are available at :doc:`docker_instructions`.
- Get prebuilt binaries from
  `release <https://github.com/google/shaka-packager/releases>`_.
- Built from source, see :doc:`build_instructions` for details.

Synopsis
--------

::

    $ packager <stream_descriptor> ... \
               [--dump_stream_info] \
               [--quiet] \
               [Chunking Options] \
               [MP4 Output Options] \
               [encryption / decryption options] \
               [DASH options] \
               [HLS options] \
               [Ads options]

.. include:: /options/stream_descriptors.rst

.. include:: /options/chunking_options.rst

.. include:: /options/mp4_output_options.rst

.. include:: /options/transport_stream_output_options.rst

.. include:: /options/dash_options.rst

.. include:: /options/hls_options.rst

.. include:: /options/ads_options.rst

Encryption / decryption options
-------------------------------

Shaka Packager supports three different types of key providers:

- Raw key: keys are provided in command line
- Widevine: fetches keys from Widevine key server
- PlayReady: fetches keys from PlayReady key server

Different key providers cannot be specified at the same time.

::

    [--enable_widevine_encryption <Widevine Encryption Options>] \
    [--enable_widevine_decryption <Widevine Decryption Options>] \
    [--enable_raw_key_encryption <Raw Key Encryption Options>] \
    [--enable_raw_key_decryption <Raw Key Decryption Options>] \
    [--enable_playready_encryption <PlayReady Encryption Options>]

.. include:: /options/general_encryption_options.rst

.. include:: /options/raw_key_encryption_options.rst

.. include:: /options/widevine_encryption_options.rst

.. include:: /options/playready_encryption_options.rst
