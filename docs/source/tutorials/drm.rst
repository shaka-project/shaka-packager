DRM
===

Shaka Packager supports fetching encryption keys from Widevine Key Server,
PlayReady Key Server and CPIX (DASH-IF Content Protection Information
Exchange) documents. Shaka Packager also supports Raw Keys, for which keys
are provided to Shaka Packager directly.

.. toctree::
    :maxdepth: 2

    /tutorials/widevine.rst
    /tutorials/playready.rst
    /tutorials/raw_key.rst
    /tutorials/cpix.rst

Regardless of which key server you are using, you can instruct Shaka Packager to
generate other protection systems in additional to the native protection system
from the key server. This allows generating multi-DRM contents easily.

Configuration options
---------------------

.. include:: /options/drm_stream_descriptors.rst
.. include:: /options/general_encryption_options.rst
.. include:: /options/widevine_encryption_options.rst
.. include:: /options/raw_key_encryption_options.rst
.. include:: /options/cpix_encryption_options.rst
