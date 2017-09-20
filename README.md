![Shaka Packager](docs/shaka-packager.png)

[![Build Status](https://travis-ci.org/google/shaka-packager.svg?branch=master)](https://travis-ci.org/google/shaka-packager)
[![Build status](https://ci.appveyor.com/api/projects/status/3t8iu603rp25sa74?svg=true)](https://ci.appveyor.com/project/shaka/shaka-packager)

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
  - [Playready](https://www.microsoft.com/playready/)<sup>1</sup>
  - [Fairplay](https://developer.apple.com/streaming/fps/)<sup>1</sup>
- Encryption standards:
  - [CENC](https://en.wikipedia.org/wiki/MPEG_Common_Encryption)
  - [SAMPLE-AES](https://developer.apple.com/library/content/documentation/AudioVideo/Conceptual/HLS_Sample_Encryption/Intro/Intro.html)
- Media Containers and codecs

  |      Codecs       |   ISO-BMFF   |     WebM     |   MPEG2-TS   |     WVM     |
  |:-----------------:|:------------:|:------------:|:------------:|:-----------:|
  |    H264 (AVC)     |    I / O     |      -       |     I / O    |      I      |
  |    H265 (HEVC)    |    I / O     |      -       |       I      |      -      |
  |       VP8         |    I / O     |    I / O     |       -      |      -      |
  |       VP9         |    I / O     |    I / O     |       -      |      -      |
  |       AAC         |    I / O     |      -       |     I / O    |      I      |
  |  Dolby AC3/EAC3   |    I / O     |      -       |       -      |      -      |
  |       DTS         |    I / O     |      -       |       -      |      -      |
  |       Opus        |   *I / O*    |    I / O     |       -      |      -      |
  |      Vorbis       |      -       |    I / O     |       -      |      -      |

  ** I for input and O for output.
  ** Opus support in ISO-BMFF is experimental.
- Platforms
  - Linux
  - Mac
  - Windows
  - Cross compiling for ARM is also supported.

<sup>1: Limited support</sup>

# Getting Shaka Packager

There are several ways you can get Shaka Packager.

- Using [Docker](https://www.docker.com/whatisdocker).
  Instructions are available [here](docs/source/docker_instructions.md).
- Get prebuilt binaries from
  [release](https://github.com/google/shaka-packager/releases).
- Built from source, see [Build Instructions](docs/source/build_instructions.md)
  for details.

# Useful Links

- [Mailing List](https://groups.google.com/forum/#!forum/shaka-packager-users)
  (join for release announcements or problem discussions)
- [Documentation](https://google.github.io/shaka-packager/html/)
- [Tutorials](https://google.github.io/shaka-packager/html/tutorials/tutorials.html)
- Several open source players:
  - [Web: Shaka Player](https://github.com/google/shaka-player)
  - [Web: dash.js](https://github.com/Dash-Industry-Forum/dash.js)
  - [Android: ExoPlayer](https://github.com/google/ExoPlayer)

# Contributing

If you have improvements or fixes, we would love to have your contributions.
See https://github.com/google/shaka-packager/blob/master/CONTRIBUTING.md for
details.
