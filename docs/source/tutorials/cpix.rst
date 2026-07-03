Using CPIX
==========

Shaka Packager supports fetching encryption keys from CPIX (`DASH-IF Content
Protection Information Exchange
<https://dashif.org/docs/CPIX2.3/HTML/Index.html>`_) documents, as served by
multi-DRM providers such as DRMtoday, or produced by any other CPIX conformant
key server. Content keys, DRM signaling (PSSH) and the key to stream mapping
are all read from the document.

Synopsis
--------

::

    $ packager <stream_descriptor> ... \
      --enable_cpix_encryption \
      --cpix <path or url> \
      [--cpix_headers <headers>] \
      [--cpix_request_file <path>] \
      [--cpix_private_key <path>] \
      [Other options, e.g. DASH options, HLS options]

*--cpix* accepts a local path or an HTTP(S) URL. URLs are fetched with GET,
unless *--cpix_request_file* provides a CPIX request document, in which case
the request document is POSTed to the URL and the response is used as the
CPIX document (SPEKE style request/response exchange). *--cpix_headers* adds
HTTP headers, e.g. for authentication.

Key to stream mapping
---------------------

Keys are mapped to streams through the document's *ContentKeyUsageRuleList*:

* A rule with an *intendedTrackType* attribute maps its key to streams whose
  DRM label matches the attribute value (e.g. AUDIO, SD, HD, UHD1, UHD2).
* A rule with an *AudioFilter* maps its key to audio streams.
* A rule with *VideoFilter* pixel ranges maps its key to the SD/HD/UHD1/UHD2
  video labels covered by the range, as determined by the *--max_sd_pixels*,
  *--max_hd_pixels* and *--max_uhd1_pixels* thresholds. The filter boundaries
  must align with these thresholds.
* A rule with no filters, or a document with a single key and no usage rules,
  maps the key to all streams.

DRM signaling comes from the document's *DRMSystemList* and is authoritative:
no default protection system is generated. *--protection_systems* may still
be used to generate signaling for additional protection systems.

Encrypted documents
-------------------

CPIX documents with encrypted content keys are supported. Provide the
recipient RSA private key with *--cpix_private_key*; the document key and MAC
key are unwrapped from the document's *DeliveryData* and each content key
value's MAC is verified before use.

Examples
--------

The examples below use the H264 streams created in :doc:`encoding`.

* Example with a local, clear-text CPIX document::

    $ packager \
      in=h264_baseline_360p_600.mp4,stream=audio,output=audio.mp4 \
      in=h264_baseline_360p_600.mp4,stream=video,output=h264_360p.mp4 \
      in=h264_main_480p_1000.mp4,stream=video,output=h264_480p.mp4 \
      in=h264_main_720p_3000.mp4,stream=video,output=h264_720p.mp4 \
      --enable_cpix_encryption \
      --cpix keys.cpix \
      --mpd_output h264.mpd

* Example fetching an encrypted document from DRMtoday's CPIX endpoint::

    $ packager \
      in=h264_baseline_360p_600.mp4,stream=audio,output=audio.mp4 \
      in=h264_main_720p_3000.mp4,stream=video,output=h264_720p.mp4 \
      --enable_cpix_encryption \
      --cpix "https://fe.drmtoday.com/frontend/cpix/v1/<merchant>/ingest/<config>/<asset_id>?ticket=<ticket>&x-signaling-alg=cenc" \
      --cpix_private_key recipient_key.pem \
      --mpd_output h264.mpd

* Example decrypting content with keys from a CPIX document::

    $ packager \
      input=encrypted_content.mp4,stream=video,output=decrypted_video.mp4 \
      --enable_cpix_decryption \
      --cpix keys.cpix

Refer to the `CPIX specification <https://dashif.org/docs/CPIX2.3/HTML/Index.html>`_
for the document format, and to :doc:`/options/cpix_encryption_options` for
the complete list of options.
