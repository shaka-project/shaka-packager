HLS specific stream descriptor fields
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:hls_name:

    Used for HLS audio to set the NAME attribute for EXT-X-MEDIA.
    Defaults to the base of the playlist name.

:hls_group_id:

    Used for HLS audio to set the GROUP-ID attribute for EXT-X-MEDIA.
    Defaults to 'audio' if not specified.

:playlist_name:

    The HLS playlist file to create. Usually ends with '.m3u8', and is
    relative to hls_master_playlist_output (see below). If unspecified,
    defaults to something of the form 'stream_0.m3u8', 'stream_1.m3u8',
    'stream_2.m3u8', etc.

:iframe_playlist_name:

    The optional HLS I-Frames only playlist file to create. Usually ends with
    '.m3u8', and is relative to hls_master_playlist_output (see below). Should
    only be set for video streams. If unspecified, no I-Frames only playlist is
    created.

:hls_characteristics (charcs):

    Optional colon or semi-colon separated list of values for the
    CHARACTERISTICS attribute for EXT-X-MEDIA. See CHARACTERISTICS attribute in
    http://bit.ly/2OOUkdB for details.
