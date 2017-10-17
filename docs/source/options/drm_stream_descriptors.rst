DRM related Stream descriptor fields
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:skip_encryption=0|1:

    Optional. Defaults to 0 if not specified. If it is set to 1, no encryption
    of the stream will be made.

:drm_label:

    Optional value for custom DRM label, which defines the encryption key
    applied to the stream. Typically values include AUDIO, SD, HD, UHD1, UHD2.
    For raw key, it should be a label defined in --keys. If not provided, the
    DRM label is derived from stream type (video, audio), resolutions, etc.
    Note that it is case sensitive.
