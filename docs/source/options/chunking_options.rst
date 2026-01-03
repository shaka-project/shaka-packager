Chunking options
^^^^^^^^^^^^^^^^

--segment_duration <seconds>

    Segment duration in seconds. If single_segment is specified, this parameter
    sets the duration of a subsegment; otherwise, this parameter sets the
    duration of a segment. Actual segment durations may not be exactly as
    requested.

--fragment_duration <seconds>

    Fragment duration in seconds. Should not be larger than the segment
    duration. Actual fragment durations may not be exactly as requested.

--segment_sap_aligned

    Force segments to begin with stream access points. Default enabled.

--fragment_sap_aligned

   Force fragments to begin with stream access points. This flag implies
   *segment_sap_aligned*. Default enabled.

--start_segment_number

   Indicates the startNumber in DASH SegmentTemplate and HLS segment name.

--ts_ttx_heartbeat_shift <90kHz ticks>

   For DVB-Teletext in MPEG-2 TS: timing offset (in 90kHz ticks) between
   video PTS timestamps and text segment generation. This compensates for
   the pipeline delay where video is processed ahead of teletext.

   Default is 90000 (1 second). If the value is too large, heartbeat-triggered
   text segments are generated later than the corresponding video segments.
   If the value is too small, some text cues may be absent in the output.

   Only applicable when processing MPEG-TS input with both video and teletext
   streams. See :doc:`/tutorials/text` for more details.
