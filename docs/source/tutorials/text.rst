Text output formats
===================

Shaka Packager supports several text/subtitle formats for both input and output.
We only support certain formats for output, other formats are converted to the
specified output format.  With the exception of TTML pass-through, there are no
restrictions of input vs output formats.


Examples
--------

* TTML pass-through::

    $ packager in=input.ttml,stream=text,output=output.ttml

* Convert WebVTT to TTML::

    $ packager in=input.vtt,stream=text,output=output.ttml

* Embed WebVTT in MP4 (single-file)::

    $ packager in=input.vtt,stream=text,output=output.mp4

* Embed WebVTT in MP4 (segmented)::

    $ packager 'in=input.vtt,stream=text,init_segment=init.mp4,segment_template=text_$Number$.mp4'

* Convert WebVTT to TTML in MP4::

    $ packager in=input.vtt,stream=text,format=ttml+mp4,output=output.mp4

* Convert DVB-SUB to TTML in MP4::

    $ packager in=input.ts,stream=text,format=ttml+mp4,output=output.mp4
    $ packager 'in=input.ts,stream=text,format=ttml+mp4,init_segment=init.mp4,segment_template=text_$Number$.mp4'

* Get a single page from DVB-SUB and set language::

    $ packager in=input.ts,stream=text,cc_index=3,lang=en,format=ttml+mp4,output=output.mp4

* Multiple languages::

    $ packager \
      in=in_en.vtt,stream=text,language=en,output=out_en.mp4 \
      in=in_sp.vtt,stream=text,language=sp,output=out_sp.mp4 \
      in=in_fr.vtt,stream=text,language=fr,output=out_fr.mp4

* Get a single 3-digit page from DVB-teletext and set language for output formats stpp (TTML in mp4), wvtt (WebVTT in mp4) and HLS WebVTT::

    $ packager in=input.ts,stream=text,cc_index=888,lang=en,format=ttml+mp4,output=output.mp4
    $ packager in=input.ts,stream=text,cc_index=888,lang=en,output=output.mp4
    $ packager in=input.ts,stream=text,cc_index=888,segment_template=text/$Number$.vtt,playlist_name=text/main.m3u8,hls_group_id=text,hls_name=ENGLISH


DVB-Teletext
------------

DVB-Teletext subtitles are commonly used in European broadcast systems. They are
embedded in MPEG-2 Transport Streams and identified by a 3-digit page number
(e.g., 888 for subtitles in many countries).

Page numbering
^^^^^^^^^^^^^^

Teletext pages are identified by a magazine number (1-8) and a two-digit page
number (00-99). The ``cc_index`` parameter uses a 3-digit format where the first
digit is the magazine and the last two digits are the page number:

* ``cc_index=888`` - Magazine 8, page 88 (common for subtitles)
* ``cc_index=100`` - Magazine 1, page 00
* ``cc_index=777`` - Magazine 7, page 77

Heartbeat mechanism
^^^^^^^^^^^^^^^^^^^

Teletext subtitles are "sparse" - they only contain data when subtitles are
displayed. This creates a problem for segmented output (DASH/HLS): if no
subtitle appears during a segment's time window, that segment might be missing
or have incorrect timing.

To solve this, Shaka Packager uses a "heartbeat" mechanism when processing
teletext from MPEG-TS input. The video stream's PTS timestamps are used to
generate periodic timing signals that ensure:

1. Text segments are generated continuously, even during gaps in subtitles
2. Text segment boundaries align with video segment boundaries
3. Ongoing subtitles that span multiple segments are properly handled

This mechanism is automatic when processing MPEG-TS files with both video and
teletext streams.

Heartbeat shift parameter
^^^^^^^^^^^^^^^^^^^^^^^^^

The ``--ts_ttx_heartbeat_shift`` parameter controls the timing offset between
when video timestamps arrive and when they trigger text segment generation.
This is needed because video is typically processed slightly ahead of teletext
in the pipeline.

The default value (90000 at 90kHz timescale = 1 second) works for most cases.
You may need to adjust this if:

* Text segments are generated later than video segments (value too large)
* Some text cues are missing from the output (value too small)

Example with custom heartbeat shift (3 seconds)::

    $ packager \
      --ts_ttx_heartbeat_shift 270000 \
      'in=input.ts,stream=video,init_segment=v/init.mp4,segment_template=v/$Number$.m4s' \
      'in=input.ts,stream=audio,init_segment=a/init.mp4,segment_template=a/$Number$.m4s' \
      'in=input.ts,stream=text,cc_index=888,lang=en,init_segment=t/init.mp4,segment_template=t/$Number$.m4s'

Segment alignment
^^^^^^^^^^^^^^^^^

When generating DASH or HLS output with teletext, the text segments are
automatically aligned with video segment boundaries. This ensures that:

* All adaptation sets have the same segment timeline
* Seeking works correctly across all media types
* There are no gaps or overlaps between segments

For best results, always include video and teletext streams from the same
MPEG-TS source in the same packager invocation::

    $ packager \
      --segment_duration 6 \
      --mpd_output manifest.mpd \
      'in=input.ts,stream=video,init_segment=video/init.mp4,segment_template=video/$Number$.m4s' \
      'in=input.ts,stream=audio,init_segment=audio/init.mp4,segment_template=audio/$Number$.m4s' \
      'in=input.ts,stream=text,cc_index=888,lang=en,init_segment=text/init.mp4,segment_template=text/$Number$.m4s,dash_only=1'

VoD output with non-zero start times
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When the input MPEG-TS has high PTS values (e.g., from a live recording that
started hours into a broadcast), use the ``--generate_static_live_mpd`` flag
to ensure proper ``presentationTimeOffset`` values in the DASH manifest::

    $ packager \
      --generate_static_live_mpd \
      --segment_duration 4 \
      --mpd_output manifest.mpd \
      'in=recording.ts,stream=video,init_segment=video/init.mp4,segment_template=video/$Number$.m4s' \
      'in=recording.ts,stream=text,cc_index=888,lang=en,init_segment=text/init.mp4,segment_template=text/$Number$.m4s'

Troubleshooting
^^^^^^^^^^^^^^^

**No subtitles in output**

* Verify the correct ``cc_index`` value. Use a tool like ``ccextractor`` or
  ``dvbsnoop`` to identify available teletext pages in the input.
* Ensure the teletext stream contains actual subtitle data, not just page
  structure information.

**Subtitles cues are missing**

* Check that video and teletext come from the same MPEG-TS source
* Try adjusting ``--ts_ttx_heartbeat_shift`` if subtitle cues are missing, or 
  text segments are generated too late

**Missing segments or gaps**

* Ensure video stream is included in the same packager run - the heartbeat
  mechanism requires video PTS to drive text segmentation
* Check that segment duration is appropriate for the subtitle density
