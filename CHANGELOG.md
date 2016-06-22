## [1.4.1] - 2016-06-23
### Fixed
- [VP9] VPCodecConfiguration box should inherit from FullBox instead of Box.
- [VP9] Fixed 'senc' box generation when encrypting mp4:vp9 with superframe.
- [WebM] Close file before trying to get file size, so the file size can be
  correctly calculated.

### Changed
- [MP4] Ignore unrecognized mp4 boxes instead of error out.

## [1.4.0] - 2016-04-08
### Added
- Added support for MacOSX (#65). Thanks to @nevil.
- Added support for Dolby AC3 and EAC3 Audio in ISO-BMFF (#64).
- Added support for language code with subtags, e.g. por-BR is now supported.
- Added a new optional flag (--include_common_pssh) to widevine encryption
  to include [common system pssh box](https://goo.gl/507mKp) in addition to
  widevine pssh box.
- Improved handling of unescaped NAL units in byte stream (#96).

### Changed
- Changed fixed key encryption to generate
  [common system pssh box](https://goo.gl/507mKp) by default; overridable by
  specifying pssh box(es) explicitly with --pssh flag, which is now optional.
  --pssh should be one or more PSSH boxes instead of just pssh data in hex
  string format if it is specified.
- Improved subsample encryption algorithm for H.264 and H.265. Now only video
  data in slice NALs are encrypted (#40).

### Fixed
- Split AdaptationSets by container and codec in addition to content_type,
  language. AVC/MP4 and VP9/WebM are now put in different AdaptationSets if
  they are packaged together.
- Fixed index range off-by-1 error in WebM DASH manifest (#99).
- Fixed WebM SeekHeader bug that the positions should be relative to the
  Segment payload instead of the start of the file.

## [1.3.1] - 2016-01-22
This release fixes and improves WebM parsing and packaging.
### Added
- Added 'cenc:default_KID' attribute in ContentProtection element for non-MP4,
  i.e. WebM mpd too #69.
- Added WebM content decryption support #72.

### Fixed
- Fixed decoding timestamp always being 0 when trasmuxing from WebM to MP4 #67.
- Improved sample duration computation for WebM content to get rid of possible
  gaps due to accumulated errors #68.
- Fixed possible audio sample loss in WebM parser, which could happen if there
  are audio blocks before the first video block #71.

## [1.3.0] - 2016-01-15
### Added
- Added support for new container format: WebM.
- Added support for new codecs:
  - H265 in ISO-BMFF (H265 in other containers will be added later).
  - VP8, VP9 in WebM and ISO-BMFF (experimental).
  - Opus and Vorbis in WebM.
  - DTS in ISO-BMFF.
- Added Verbose logging through --v or --vmodule command line flags.
- Added Subtitle support for On-Demand: allowing subtitle inputs in webvtt or
  ttml. Support for subtitle inputs in media files will be added later.
- Added version information in generated outputs.

### Changed
- Store Sample Auxiliary Information in Sample Encryption Information ('senc')
  box instead of inside Media Data ('mdat') box.
- Got rid of svn dependencies, now all dependencies are in git repo.
- Switched to boringssl, replacing openssl.

### Fixed (in addition to fix in 1.2.1)
- Fixed issue #55 DASH validation (conformance check) problems.
- Fixed AssetId overflow in classic WVM decryption when AssetId exceeds
  0x8000000.
- Fixed a memory leak due to thread object tracking #61.

## [1.2.1] - 2015-11-18
### Fixed
- Fixed a deadlock in MpdBuilder which could lead to program hang #45
- Fixed a race condition in MpdNotifier which could lead to corrupted mpd #49
- Improved support for WVM files:
  - Support files with no PES stream ID metadata.
  - Support files with multiple audio or video configurations.
- Fixed a race condition when flushing ThreadedIoFile which may cause flush
  to be called before file being written; fixed another race condition in
  ThreadedIoFile if there is an error in reading or writing files.
- Relaxed requirement on reserved bits when parsing AVCC #44
- Fixed stropts.h not found issue in CentOS 7.

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

[1.4.0]: https://github.com/google/edash-packager/compare/v1.3.1...v1.4.0
[1.3.1]: https://github.com/google/edash-packager/compare/v1.3.0...v1.3.1
[1.3.0]: https://github.com/google/edash-packager/compare/v1.2.0...v1.3.0
[1.2.1]: https://github.com/google/edash-packager/compare/v1.2.0...v1.2.1
[1.2.0]: https://github.com/google/edash-packager/compare/v1.1...v1.2.0
[1.1.0]: https://github.com/google/edash-packager/compare/v1.0...v1.1
