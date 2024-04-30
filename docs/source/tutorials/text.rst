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
