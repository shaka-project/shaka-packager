Using PlayReady Key Server
==========================

Shaka Packager can talk to PlayReady Key Server that implements
`AcquirePackagingData Web Method specification <http://bit.ly/2M9NuOt>`_ to
fetch encryption keys.

Refer to :doc:`/tutorials/drm` if you are interested in generating multi-DRM
contents.

Synopsis
--------

::

    $ packager <stream_descriptor> ... \
      --enable_playready_encryption \
      --playready_server_url <playready_server_url> \
      --program_identifier <program_identifier> \
      --client_cert_file <client_cert_file> \
      --client_cert_private_key_file <client_cert_private_key_file> \
      --client_cert_private_key_password <client_cert_private_key_password> \
      --ca_file <ca_file> \
      [Other options, e.g. DASH options, HLS options]

The --client_cert_xx and --ca_file parameters can be omitted if not required by
the key server.

Configuration options
---------------------

.. include:: /options/drm_stream_descriptors.rst
.. include:: /options/general_encryption_options.rst
.. include:: /options/playready_encryption_options.rst
