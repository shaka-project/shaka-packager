Design
======

Architecture diagram
--------------------

.. graphviz::

    digraph shaka_packager {
      label=<<u>Shaka Packager Architecture</u>>
      labelloc=t

      subgraph cluster_media {
        label=<<u>Media Processing Pipeline</u>>

        Demuxer, ChunkingHandler, EncryptionHandler, Replicator,
            TrickplayHandler, Muxer [shape=rectangle]

        Demuxer -> ChunkingHandler [style=bold headlabel="many" taillabel="1"]
        ChunkingHandler -> EncryptionHandler -> Replicator -> TrickplayHandler
            -> Muxer [style=bold]
        ChunkingHandler -> Replicator -> Muxer [style=bold]
      }

      MuxerListener, MpdNotifyMuxerListener, HlsNotifyMuxerListener,
          MpdNotifier, HlsNotifier [shape=rectangle style=rounded]

      Muxer -> MuxerListener
      MuxerListener -> MpdNotifyMuxerListener, HlsNotifyMuxerListener
          [dir=back arrowtail=onormal]

      subgraph cluster_manifest {
        label=<<u>Manifest Generation</u>>

        HlsNotifyMuxerListener -> HlsNotifier [headlabel="1" taillabel="many"]
        MpdNotifyMuxerListener -> MpdNotifier [headlabel="1" taillabel="many"]

        MasterPlaylist, MediaPlaylist, MpdBuilder, AdaptationSet,
            Representation [shape=trapezium]

        HlsNotifier -> MasterPlaylist
        MasterPlaylist -> MediaPlaylist
            [dir=back arrowtail=diamond headlabel="many" taillabel="1"]
        MpdNotifier -> MpdBuilder
        MpdBuilder -> AdaptationSet -> Representation
            [dir=back arrowtail=diamond headlabel="many" taillabel="1"]

        {rank=same; MasterPlaylist, MpdBuilder}
        {rank=same; MediaPlaylist, Representation}
      }
    }

.. graphviz::

    digraph shaka_packager {
        subgraph cluster_demuxer {
          style=rounded
          label=<<u> </u>>

          Demuxer2 [label="Demuxer" shape=rectangle]
          Demuxer2 -> MediaParser
              [dir=back arrowtail=diamond headlabel="1" taillabel="1"]
          MediaParser -> Mp4MediaParser, WebMMediaParser, Mp2tMediaParser,
              WvmMediaParser [dir=back arrowtail=onormal]
        }

        subgraph cluster_muxer {
          style=rounded
          label=<<u> </u>>

          Muxer2 [label="Muxer" shape=rectangle]
          Muxer2 -> Mp4Muxer, WebMMuxer, Mp2tMuxer [dir=back arrowtail=onormal]
        }
    }

.. graphviz::

    digraph shaka_packager {
      subgraph cluster_legend {
        style=rounded
        label=<<u>Legend</u>>

        node [shape=plaintext]

        blank1 [label="" height=0]
        blank2 [label="" height=0]
        blank3 [label="" height=0]

        "Composition" -> blank1 [dir=back arrowtail=diamond]
        "Inheritance" -> blank2 [dir=back arrowtail=onormal]
        "MediaHandler data flow" -> blank3 [style=bold]

        "Bridge Class" [shape=rectangle style=rounded]
        "Manifest Class" [shape=trapezium]
        MediaHandler [shape=rectangle]
      }
    }

Media handler data flow
-----------------------

.. graphviz::

    digraph g {
      rankdir=LR

      StreamData [
        label="{... | SegmentInfo | MediaSample ... | SegmentInfo | MediaSample ... | StreamInfo}"
        shape=record
        style=rounded
      ];

      MediaHandler [shape=rectangle]
      MediaHandler2 [shape=rectangle, label=MediaHandler]
      MediaHandler -> StreamData -> MediaHandler2
    }

.. uml::

    MediaHandler -> MediaHandler2 : StreamInfo
    MediaHandler -> MediaHandler2 : MediaSample
    MediaHandler -> MediaHandler2 : MediaSample
    MediaHandler -> MediaHandler2 : ...
    MediaHandler -> MediaHandler2 : MediaSample
    MediaHandler -> MediaHandler2 : SegmentInfo
    MediaHandler -> MediaHandler2 : MediaSample
    MediaHandler -> MediaHandler2 : MediaSample
    MediaHandler -> MediaHandler2 : ...
    MediaHandler -> MediaHandler2 : MediaSample
    MediaHandler -> MediaHandler2 : SegmentInfo
    MediaHandler -> MediaHandler2 : ...

Teletext processing pipeline
----------------------------

When processing MPEG-TS input with DVB-Teletext subtitles, a specialized pipeline
ensures text segments align with video/audio segment boundaries:

.. graphviz::

    digraph teletext_pipeline {
      label=<<u>Teletext Processing Pipeline</u>>
      labelloc=t
      rankdir=TB
      compound=true

      subgraph cluster_demuxer {
        label=<<u>Demuxer</u>>
        style=rounded

        Mp2tMediaParser [shape=rectangle]
        EsParserTeletext [shape=rectangle]
        EsParserH264 [shape=rectangle label="EsParserH26x"]

        Mp2tMediaParser -> EsParserH264 [label="video PID"]
        Mp2tMediaParser -> EsParserTeletext [label="teletext PID"]
        Mp2tMediaParser -> EsParserTeletext [label="heartbeat\n(video PTS)" style=dashed]
      }

      subgraph cluster_coordinator {
        label=<<u>Segment Coordination</u>>
        style=rounded

        SegmentCoordinator [shape=rectangle]
      }

      subgraph cluster_chunking {
        label=<<u>Chunking</u>>
        style=rounded

        ChunkingHandler [shape=rectangle]
        TextChunker [shape=rectangle]
      }

      subgraph cluster_output {
        label=<<u>Output</u>>
        style=rounded

        VideoMuxer [shape=rectangle label="Muxer\n(video)"]
        TextMuxer [shape=rectangle label="Muxer\n(text)"]
      }

      EsParserH264 -> SegmentCoordinator [style=bold label="video stream"]
      EsParserTeletext -> SegmentCoordinator [style=bold label="text stream"]

      SegmentCoordinator -> ChunkingHandler [style=bold label="video/audio"]
      SegmentCoordinator -> TextChunker [style=bold label="teletext"]

      ChunkingHandler -> SegmentCoordinator [style=dashed label="SegmentInfo" constraint=false]
      SegmentCoordinator -> TextChunker [style=dashed label="replicate\nSegmentInfo" constraint=false]

      ChunkingHandler -> VideoMuxer [style=bold]
      TextChunker -> TextMuxer [style=bold]
    }

The key components are:

- **Mp2tMediaParser**: Sends periodic "heartbeat" signals (video PTS timestamps)
  to the teletext parser to drive segment generation even when no subtitle data
  is present.

- **EsParserTeletext**: Parses DVB-Teletext PES packets and emits text samples
  with special roles (CueStart, CueEnd, TextHeartBeat, MediaHeartBeat).

- **SegmentCoordinator**: An N-to-N handler that passes all streams through
  unchanged, but also replicates SegmentInfo from video/audio streams to
  registered teletext streams. This ensures text segment boundaries match
  video segment boundaries exactly.

- **TextChunker**: In "coordinator mode", receives SegmentInfo events from the
  SegmentCoordinator and uses them to determine segment boundaries instead of
  calculating boundaries from text sample timestamps.

.. uml::

    participant "Mp2tMediaParser" as Parser
    participant "EsParserTeletext" as TtxParser
    participant "SegmentCoordinator" as Coord
    participant "ChunkingHandler" as Chunking
    participant "TextChunker" as TextChunk

    Parser -> TtxParser : MediaHeartBeat(video_pts)
    Parser -> Coord : video MediaSample
    Parser -> Coord : text TextSample

    Coord -> Chunking : video MediaSample
    Coord -> TextChunk : text TextSample

    Chunking -> Coord : SegmentInfo(boundary)
    Coord -> Coord : pass through to video output
    Coord -> TextChunk : replicate SegmentInfo

    TextChunk -> TextChunk : dispatch text segment\n(aligned with video)
