General encryption options
^^^^^^^^^^^^^^^^^^^^^^^^^^

--protection_scheme <scheme>

    Specify a protection scheme, 'cenc' or 'cbc1' or pattern-based protection
    schemes 'cens' or 'cbcs'.

--vp9_subsample_encryption, --novp9_subsample_encryption

    Enable / disable VP9 subsample encryption. Enabled by default.

--clear_lead <seconds>

    Clear lead in seconds if encryption is enabled.

--generate_widevine_pssh

   Generate a Widevine PSSH box. This option is always true
   when using :doc:`/tutorials/widevine` key source.

--generate_common_pssh

    Generate a v1 PSSH box for the common system ID that includes
    the key IDs. See https://goo.gl/s8RIhr. This option is default to be
    true when using :doc:`/tutorials/raw_key` source and no other pssh
    flag is specified.

--generate_playready_pssh

    Generate a PlayReady PSSH box. This option is always
    true when using :doc:`/tutorials/playready` key source.
