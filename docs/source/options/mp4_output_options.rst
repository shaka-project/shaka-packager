MP4 output options
^^^^^^^^^^^^^^^^^^

--mp4_include_pssh_in_stream

    MP4 only: include pssh in the encrypted stream. Default enabled.

--mp4_use_decoding_timestamp_in_timeline

    Deprecated. Do not use.

--generate_sidx_in_media_segments
--nogenerate_sidx_in_media_segments

    Indicates whether to generate 'sidx' box in media segments. Note
    that it is required for DASH on-demand profile (not using segment
    template).

    Default enabled.

--mp4_initial_sequence_number

    MP4 only: Sets the initialize sequence number in the mp4 moof->mfhd.
              Otherwise defaults to -1.
