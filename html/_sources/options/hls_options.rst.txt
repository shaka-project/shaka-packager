HLS options
^^^^^^^^^^^

--hls_master_playlist_output <file_path>

    Output path for the master playlist for HLS. This flag must be used to
    output HLS.

--hls_base_url <url>

    The base URL for the Media Playlists and media files listed in the
    playlists. This is the prefix for the files.

--hls_playlist_type <type>

    VOD, EVENT, or LIVE. This defines the EXT-X-PLAYLIST-TYPE in the HLS
    specification. For hls_playlist_type of LIVE, EXT-X-PLAYLIST-TYPE tag is
    omitted.
