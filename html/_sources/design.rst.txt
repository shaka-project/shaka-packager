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
