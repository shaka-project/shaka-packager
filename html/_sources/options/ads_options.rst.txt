Ads options
^^^^^^^^^^^

--ad_cues <start_time[;start_time]...>

    List of cuepoint markers separated by semicolon. The start_time represents
    the start of the cue marker in seconds (double precision) relative to the
    start of the program. This flag preconditions content for
    `Dynamic Ad Insertion <http://bit.ly/2KK10DD>`_ with Google Ad Manager.
    For DASH, multiple periods will be generated with period boundaries at the
    next key frame to the designated start times; For HLS, segments will be
    terminated at the next key frame to the designated start times and
    '#EXT-X-PLACEMENT-OPPORTUNITY' tag will be inserted after the segment in
    media playlist.
