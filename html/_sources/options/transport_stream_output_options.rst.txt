Transport stream output options
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

--transport_stream_timestamp_offset_ms

    Transport stream only (MPEG2-TS, HLS Packed Audio): A positive value, in
    milliseconds, by which output timestamps are offset to compensate for
    possible negative timestamps in the input. For example, timestamps from
    ISO-BMFF after adjusted by EditList could be negative. In transport streams,
    timestamps are not allowed to be less than zero. Default: 100ms.
