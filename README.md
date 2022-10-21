[comment]: # (While not ideal, absolute URLs are used here as it is the        )
[comment]: # (simplest way to make the links work on GitHub and Docker Hub.    )
[comment]: # (These links in cloned repositories will point back to the main   )
[comment]: # (repository and if it is an issue, we suggest updating the links  )
[comment]: # (in the cloned repository.                                        )
[comment]: # (See https://github.com/shaka-project/shaka-packager/issues/408 for the  )
[comment]: # (full background.                                                 )

![Shaka Packager](https://raw.githubusercontent.com/shaka-project/shaka-packager/main/docs/shaka-packager.png)

Shaka Packager is a tool and a media packaging SDK for
[DASH](http://dashif.org/) and [HLS](https://developer.apple.com/streaming/)
packaging and encryption. It can prepare and package media content for online
streaming.

Shaka Packager supports:

- Both Video-On-Demand and Live.
- Streaming formats:
  - [DASH](http://dashif.org/)
  - [HLS](https://developer.apple.com/streaming/)
- Key systems:
  - [Widevine](http://www.widevine.com/)
  - [PlayReady](https://www.microsoft.com/playready/)¹
  - [FairPlay](https://developer.apple.com/streaming/fps/)¹
  - [Marlin](https://www.intertrust.com/marlin-drm/)¹
- Encryption standards:
  - [CENC](https://en.wikipedia.org/wiki/MPEG_Common_Encryption)
  - [SAMPLE-AES](https://developer.apple.com/library/content/documentation/AudioVideo/Conceptual/HLS_Sample_Encryption/Intro/Intro.html)
- Media Containers and codecs

  |      Codecs       |   ISO-BMFF   |     WebM     |   MPEG2-TS   |     WVM     | Packed Audio²|
  |:-----------------:|:------------:|:------------:|:------------:|:-----------:|:------------:|
  |    H264 (AVC)     |    I / O     |      -       |     I / O    |      I      |       -      |
  |    H265 (HEVC)    |    I / O     |      -       |       I      |      -      |       -      |
  |       VP8         |    I / O     |    I / O     |       -      |      -      |       -      |
  |       VP9         |    I / O     |    I / O     |       -      |      -      |       -      |
  |       AV1         |    I / O     |    I / O     |       -      |      -      |       -      |
  |       AAC         |    I / O     |      -       |     I / O    |      I      |       O      |
  |       MP3         |      O       |      -       |     I / O    |      -      |       O      |
  |    Dolby AC3      |    I / O     |      -       |     I / O    |      -      |       O      |
  |    Dolby EAC3     |    I / O     |      -       |       O      |      -      |       O      |
  |   MPEG-H Audio    |    I / O     |      -       |       -      |      -      |       -      |
  |    Dolby AC4      |    I / O     |      -       |       -      |      -      |       -      |
  |       DTS         |    I / O     |      -       |       -      |      -      |       -      |
  |       FLAC        |    I / O     |      -       |       -      |      -      |       -      |
  |       Opus        |    I / O³    |    I / O     |       -      |      -      |       -      |
  |      Vorbis       |      -       |    I / O     |       -      |      -      |       -      |

  NOTES:
  - I for input and O for output.
  - ²: https://tools.ietf.org/html/draft-pantos-http-live-streaming-23#section-3.4
  - ³: Opus support in ISO-BMFF is experimental.
- Subtitles

  |     Format    |  Input   | Output |
  |:-------------:|:--------:|:------:|
  |  Text WebVTT  |    Y     |    Y   |
  | WebVTT in MP4 | [#405][] |    Y   |
  |   Text TTML   |    ⁴     |    Y   |
  |  TTML in MP4  |    -     |    Y   |
  |    DVB-SUB    |    Y     |    -   |
  |   Teletext    | [#272][] |    -   |

  - ⁴: TTML input is only supported with TTML output (pass-through, DASH only),
    see also [#584][].

[#272]: https://github.com/shaka-project/shaka-packager/issues/272
[#405]: https://github.com/shaka-project/shaka-packager/issues/405
[#584]: https://github.com/shaka-project/shaka-packager/issues/584

- Platforms
  - Linux
  - Mac
  - Windows
  - Cross compiling for ARM is also supported.

<sup>1: Limited support</sup>

# Getting Shaka Packager

There are several ways you can get Shaka Packager.

- Using [Docker](https://www.docker.com/whatisdocker).
  Instructions are available
  [here](https://github.com/shaka-project/shaka-packager/blob/main/docs/source/docker_instructions.md).
- Get prebuilt binaries from
  [release](https://github.com/shaka-project/shaka-packager/releases).
- Built from source, see
  [Build Instructions](https://github.com/shaka-project/shaka-packager/blob/main/docs/source/build_instructions.md)
  for details.

# Useful Links

- [Announcement List](https://groups.google.com/forum/#!forum/shaka-packager-users)
  (join for infrequent announcements and surveys)
- Subscribe to releases by following
  [instructions from this blog](https://www.jessesquires.com/blog/2020/07/30/github-tip-watching-releases/)- [Documentation](https://shaka-project.github.io/shaka-packager/html/)
- [Tutorials](https://shaka-project.github.io/shaka-packager/html/tutorials/tutorials.html)
- Several open source players:
  - [DASH and HLS on Web: Shaka Player](https://github.com/shaka-project/shaka-player)
  - [DASH on Web: dash.js](https://github.com/Dash-Industry-Forum/dash.js)
  - [HLS on Web: hls.js](https://github.com/video-dev/hls.js)
  - [DASH and HLS on Android: ExoPlayer](https://github.com/google/ExoPlayer)

# Contributing

If you have improvements or fixes, we would love to have your contributions.
See https://github.com/shaka-project/shaka-packager/blob/main/CONTRIBUTING.md for
details.
