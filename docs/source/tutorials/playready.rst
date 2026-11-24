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

Generating PlayReady with other key sources
-------------------------------------------

A PlayReady key server is not required to generate PlayReady content. Add
*PlayReady* to *--protection_systems* with any key source, and Shaka Packager
generates the PlayReady header and PSSH from the content key and key ID that
key source provides. This works with :doc:`/tutorials/raw_key`,
:doc:`/tutorials/widevine` and :doc:`/tutorials/cpix`, and is the usual way to
produce multi-DRM content without having to provide a second set of keys.

PlayReady header
----------------

When Shaka Packager generates the PlayReady header itself, the protection
scheme selects the header version:

* 'cenc' and 'cens' produce a version 4.0.0.0 header using AESCTR.
* 'cbcs' and 'cbc1' produce a version 4.3.0.0 header using AESCBC, which
  requires PlayReady 4.0 or later clients.

Extra elements, such as a <LAURL> license acquisition URL, can be added to the
header with *--playready_extra_header_data*. This is optional, and is only
needed for clients that cannot be told the license server URL by the
application.

When keys are fetched from a PlayReady key server, the header comes from the
key server response instead, and *--playready_extra_header_data* does not
apply.

In DASH, the PlayReady ContentProtection element carries the header both as a
<cenc:pssh> and as an <mspr:pro> element. See
*--include_mspr_pro_for_playready* in the :doc:`/tutorials/dash` options.

Limitations
-----------

The PlayReady key server integration fetches a single content key and uses it
for every stream, so *drm_label* in the stream descriptors has no effect.

Key rotation (*--crypto_period_duration*) is not implemented for the PlayReady
key server; the same key is returned for every crypto period. Use another key
source if you need key rotation or different keys per stream.

Configuration options
---------------------

.. include:: /options/drm_stream_descriptors.rst
.. include:: /options/general_encryption_options.rst
.. include:: /options/playready_encryption_options.rst
