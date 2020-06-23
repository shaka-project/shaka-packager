General encryption options
^^^^^^^^^^^^^^^^^^^^^^^^^^

--protection_scheme <scheme>

    Specify a protection scheme, 'cenc' or 'cbc1' or pattern-based protection
    schemes 'cens' or 'cbcs'.

--crypt_byte_block

    Specify the count of the encrypted blocks in the protection pattern, where
    block is of size 16-bytes.

    There are three common patterns (crypt_byte_block:skip_byte_block):
    1:9 (default), 5:5, 10:0.

    Apply to video streams with 'cbcs' and 'cens' protection schemes only;
    ignored otherwise.

--skip_byte_block

    Specify the count of the unencrypted blocks in the protection pattern.

    Apply to video streams with 'cbcs' and 'cens' protection schemes only;
    ignored otherwise.

--vp9_subsample_encryption, --novp9_subsample_encryption

    Enable / disable VP9 subsample encryption. Enabled by default.

--clear_lead <seconds>

    Clear lead in seconds if encryption is enabled.
    Shaka Packager does not support partial encrypted segments, all the
    segments including the partial segment overlapping with the initial
    'clear_lead' seconds are not encrypted, with all the following segments
    encrypted. If segment_duration is greater than 'clear_lead', then only the
    first segment is not encrypted.
    Default: 5

--protection_systems

    Protection systems to be generated. Supported protection systems include
    Widevine, PlayReady, FairPlay, Marlin, and
    `CommonSystem <https://goo.gl/s8RIhr>`_.

--playready_extra_header_data <string>

    Extra XML data to add to PlayReady PSSH data.  Can be specified even if
    using another key source.
