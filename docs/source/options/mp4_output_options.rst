MP4 output options
^^^^^^^^^^^^^^^^^^

--mp4_include_pssh_in_stream

    MP4 only: include pssh in the encrypted stream. Defaults to enabled,
    except that when ``--generate_dash_if_iop_compliant_mpd`` is enabled (the
    default) it defaults to disabled, because the pssh is carried in the
    manifest instead. An explicit value always wins.

--mp4_use_decoding_timestamp_in_timeline

    Deprecated. Do not use.

--generate_sidx_in_media_segments
--nogenerate_sidx_in_media_segments

    Indicates whether to generate 'sidx' box in media segments. Note
    that it is required for DASH on-demand profile (not using segment
    template).

    Default enabled.
