DASH options
^^^^^^^^^^^^

--generate_static_mpd

    If enabled, generates static mpd. If segment_template is specified in
    stream descriptors, shaka-packager generates dynamic mpd by default; if
    this flag is enabled, shaka-packager generates static mpd instead. Note
    that if segment_template is not specified, shaka-packager always generates
    static mpd regardless of the value of this flag.

--mpd_output <file_path>

    MPD output file name.

--base_urls <comma separated url>

    Comma separated BaseURLs for the MPD. The values will be added  as <BaseURL>
    element(s) immediately under the <MPD> element.

--min_buffer_time <seconds>

    Specifies, in seconds, a common duration used in the definition of the MPD
    Representation data rate.

--minimum_update_period <seconds>

    Indicates to the player how often to refresh the media presentation
    description in seconds. This value is used for dynamic MPD only.

--suggested_presentation_delay <seconds>

    Specifies a delay, in seconds, to be added to the media presentation time.
    This value is used for dynamic MPD only.

--time_shift_buffer_depth <seconds>

    Guaranteed duration of the time shifting buffer for dynamic media
    presentations, in seconds.

--default_language <language>

    Any audio/text tracks tagged with this language will have
    <Role ... value=\"main\" /> in the manifest.  This allows the player to
    choose the correct default language for the content.
