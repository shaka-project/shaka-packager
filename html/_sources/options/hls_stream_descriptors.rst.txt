HLS specific stream descriptor fields
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:hls_name:

    Required for audio when outputting HLS. name of the output stream. This is
    not (necessarily) the same as output. This is used as the NAME attribute for
    EXT-X-MEDIA.

:hls_group_id:

    Required for audio when outputting HLS. The group ID for the output stream.
    This is used as the GROUP-ID attribute for EXT-X-MEDIA.

:playlist_name:

    Required for HLS output. Name of the playlist for the stream. Usually ends
    with '.m3u8'.
