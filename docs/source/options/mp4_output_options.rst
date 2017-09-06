MP4 output options
^^^^^^^^^^^^^^^^^^

--mp4_include_pssh_in_stream

    MP4 only: include pssh in the encrypted stream. Default enabled.

--mp4_use_decoding_timestamp_in_timeline

    If set, decoding timestamp instead of presentation timestamp will be used
    when generating media timeline, e.g. timestamps in sidx and mpd. This is
    to workaround a Chromium bug that decoding timestamp is used in buffered
    range, https://crbug.com/398130. Default false.

--num_subsegments_per_sidx <number>

    Set the number of subsegments in each SIDX box. If 0, a single SIDX box is
    used per segment; if -1, no SIDX box is used; Otherwise, the muxer packs N
    subsegments in the root SIDX of the segment, with
    segment_duration/N/fragment_duration fragments per subsegment.
