HLS options
^^^^^^^^^^^

--hls_master_playlist_output <file_path>

    Output path for the master playlist for HLS. This flag must be used to
    output HLS.

--hls_base_url <url>

    The base URL for the Media Playlists and media files listed in the
    playlists. This is the prefix for the files.

--hls_key_uri <uri>

    The key uri for 'identity' and 'com.apple.streamingkeydelivery' key formats.
    Ignored if the playlist is not encrypted or not using the above key formats.

--hls_playlist_type <type>

    VOD, EVENT, or LIVE. This defines the EXT-X-PLAYLIST-TYPE in the HLS
    specification. For hls_playlist_type of LIVE, EXT-X-PLAYLIST-TYPE tag is
    omitted.

--time_shift_buffer_depth <seconds>

    Guaranteed duration of the time shifting buffer for LIVE playlists, in
    seconds.

--default_language <language>

    The first audio/text rendition in a group tagged with this language will
    have 'DEFAULT' attribute set to 'YES'. This allows the player to choose the
    correct default language for the content.
