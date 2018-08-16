MP4 output options
^^^^^^^^^^^^^^^^^^

--mp4_include_pssh_in_stream

    MP4 only: include pssh in the encrypted stream. Default enabled.

--mp4_use_decoding_timestamp_in_timeline

    Deprecated. Do not use.

--generate_sidx_in_media_segments
--nogenerate_sidx_in_media_segments

    For MP4 with DASH live profile only: Indicates whether to generate 'sidx'
    box in media segments. Note that it is reuqired by spec if segment template
    contains $Time$ specifier.
