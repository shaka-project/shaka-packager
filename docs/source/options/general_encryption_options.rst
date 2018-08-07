General encryption options
^^^^^^^^^^^^^^^^^^^^^^^^^^

--protection_scheme <scheme>

    Specify a protection scheme, 'cenc' or 'cbc1' or pattern-based protection
    schemes 'cens' or 'cbcs'.

--vp9_subsample_encryption, --novp9_subsample_encryption

    Enable / disable VP9 subsample encryption. Enabled by default.

--clear_lead <seconds>

    Clear lead in seconds if encryption is enabled.

--additional_protection_systems

    Generate additional protection systems in addition to the native protection
    system provided by the key source. Supported protection systems include
    Widevine, PlayReady, FairPlay, and CommonSystem (https://goo.gl/s8RIhr).
