## [1.2.0] - 2015-10-01
### Added
- Added [docker](https://www.docker.com/) support. Thanks @leandromoreira.

### Changed
- Improved performance with threaded I/O.
- Disabled gold linker by default, which does not work on Ubuntu 64bit server.
- Delete temperary files created by packager when done.
- Updated MediaInfo file formats.

### Fixed
- Support ISO-BMFF files with trailing 'moov' boxes.
- DASH-IF IOP 3.0 Compliance. Some changes are controlled by flag
  `--generate_dash_if_iop_compliant_mpd`. It is defaulted to false, due to lack
  of player support. Will change the default to true in future releases.
  - Added @contentType to AdaptationSet;
  - For video adaptation sets, added `@maxWidth/@width, @maxHeight/@height,
    @maxFrameRate/@frameRate and @par` attributes;
  - For video representations, added `@frameRate and @sar` attributes;
  - For audio adaptation sets, added `@lang` attribute;
  - For representations with aligned segments/subsegments, added attribute
    `@subSegmentAlignment/@segmentAlignment`;
  - Added cenc:default_KID and cenc:pssh to ContentProtection elements;
  - Moved ContentProtection elements up to AdaptationSet element, controlled by
    `--generate_dash_if_iop_compliant_mpd`;
  - Moved representations encrypted with different keys to different adaptation
    sets, grouped by `@group` attribute, controlled by
    `--generate_dash_if_iop_compliant_mpd`.
- Fixed SSL CA cert issue on CentOS.
- Fixed a couple of packager crashes on invalid inputs.
- Read enough bytes before detecting container type. This fixed MPEG-TS not
  recognized issue on some systems.
- Generate proper tkhd.width and tkhd.height with non-square pixels.
- Support composition offset greater than (1<<31).
- Fixed one-sample fragment issue with generated audio streams.
- Fixed and correct width/height in VisualSampleEntry for streams with cropping.
  This fixes encrypted live playback issue for some resolutions.

## [1.1.0] - 2014-10-14
### Added
- Added timeout support for encryption key request.
- Support mpd generation in packager driver program.
- Support segment template identifier $Time$.
- Support configurable policy in Widevine encryption key request.
- Support key rotation, with configurable crypto_period_duration.
- Support UDP unicast/multicast capture.
- Support auto-determination of SD/HD track based on a configurable flag
  `--max_sd_pixels`.
- Support new input formats:
  - WVM (legacy Widevine format), both encrypted and clear;
  - CENC encrypted ISO-BMFF.

### Changed
- Replaced HappyHttp with curl for http request. Added https support.
- Changed packager driver program to be able to package multiple streams.
- Move source code into packager directory, to make it easier to third_party
  integration.

### Fixed
- Support 64 bit mdat box size.
- Support on 32-bit OS.

## 1.0.0 - 2014-04-21
First public release.

### Added
- Repo management with gclient from Chromium.
- Support input formats: fragmented and non-fragmented ISO-BMFF.
- Support encryption with Widevine license server.
- Support encryption with user supplied encryption keys.
- Added packager driver program.
- Added mpd_generator driver program to generate mpd file from packager generated
  intermediate files.

[1.2.0]: https://github.com/google/edash-packager/compare/v1.1...v1.2.0
[1.1.0]: https://github.com/google/edash-packager/compare/v1.0...v1.1
