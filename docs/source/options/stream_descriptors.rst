Stream descriptors
^^^^^^^^^^^^^^^^^^

There can be multiple *stream_descriptor* with input from the same "file" or
multiple different "files".

Stream descriptor is of the form::

    <field>=<value>[,<field>=<value>]...

These are the available fields:

:input (in):

    input/source media "file" path, which can be regular files, pipes, udp
    streams. See :doc:`/options/udp_file_options` on additional options for UDP
    files.

:stream_selector (stream):

    Required field with value 'audio', 'video', 'text' or stream number (zero
    based).

:output (out):

    Required output file path (single file).

:init_segment:

    initialization segment path (multiple file).

:segment_template (segment):

    Optional value which specifies the naming  pattern for the segment files,
    and that the stream should be split into multiple files. Its presence should
    be consistent across streams. See
    :doc:`/options/segment_template_formatting`.

:bandwidth (bw):

    Optional value which contains a user-specified maximum bit rate for the
    stream, in bits/sec. If specified, this value is propagated to (HLS)
    EXT-X-STREAM-INF:BANDWIDTH or (DASH) Representation@bandwidth and the
    $Bandwidth$ template parameter for segment names. If not specified, the
    bandwidth value is estimated from content bitrate. Note that it only affects
    the generated manifests/playlists; it has no effect on the media content
    itself.

:language (lang):

    Optional value which contains a user-specified language tag. If specified,
    this value overrides any language metadata in the input stream.

:output_format (format):

    Optional value which specifies the format of the output files (MP4 or WebM).
    If not specified, it will be derived from the file extension of the output
    file.

    For subtitles in MP4, you can specify 'vtt+mp4' or 'ttml+mp4' to control
    which text format is used.

:trick_play_factor (tpf):

    Optional value which specifies the trick play, a.k.a. trick mode, stream
    sampling rate among key frames. If specified, the output is a trick play
    stream.

:cc_index:

    Optional value which specifies the index/ID of the subtitle stream to use
    for formats where multiple exist within the same stream. For example,
    CEA allows specifying up to 4 streams within a single video stream. If not
    specified, all subtitles will be merged together.

.. include:: /options/drm_stream_descriptors.rst
.. include:: /options/dash_stream_descriptors.rst
.. include:: /options/hls_stream_descriptors.rst
