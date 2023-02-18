## [2.5.1] - 2021-06-21
### Added
 - Add support for MSVS 2017 and 2019 (#867, #955)

### Fixed
 - Fix position of LA_URL in PlayReady headers (#961)
 - Fix broken Dockerfile due to depot_tools update
 - Fix shared_library builds on Windows (#318, #956, #957, #958)

### Changed
 - CI overhaul based on GitHub Actions (#336, #959)
   - Migrated Appveyor and Travis integrations to GitHub Actions
   - Added significant new release automation to build, test, and release on
     GitHub, NPM, and Docker Hub

### Doc
 - Fix doc formatting for dash_only and hls_only (#954)


## [2.5.0] - 2021-06-09
### Added
- Support HTTP PUT to upload packaging outputs to cloud (#149).
- Support Dolby Vision backward compatible profiles (#341).
- Support different IVs for each track (#543).
- Support dash_only and hls_only parameters
  (`dash_only=0|1`, `hls_only=0|1`) (#651).
- [HLS] Allow custom EXT-X-MEDIA-SEQUENCE number (`--hls_media_sequence_number`)
  (#691).
- [MP4] Allow specifying protection pattern for pattern encryption
  (`--crypt_byte_block`, `--skip_byte_block`) (#710).
- [MP4] Allow write |mvex| before |trak| (`--mvex_before_trak`) (#711).
- [DASH] Support signalling of last segment number
  (`dash_add_last_segment_number_when_needed`) (#713).
- [DASH] Allow adaptive switching between different codecs
  (`--allow_codec_switching`) (#726).
- [DASH] Include <mspr:pro> alongside to <cenc:pssh> for PlayReady (#743).
- Support Dolby DD+JOC in DASH and HLS (#753).
- Support AC-4 codec (#754).
- Support inclusion of extra PlayReady header data
  (`--playready_extra_header_data`) (#756).
- Support MPEG-1 Audio in mpeg2ts I/O and packed-audio / mp4 output (#779).
- Support more text input and output formats, including DVB-SUB input (#832) and
  TTML in MP4 output (#87).
- Support segment_list for DASH on-demand profile (`--dash_force_segment_list`).

### Fixed
- DASH / HLS spec compliance issues
  - [HLS] Add support for independent segments tag (#564).
  - [TS] Improve frame rate calculation for TS streams (#751).
  - [MP4] Change major brand from isom to mp41 (#755).
  - [MP4] Always set ES_ID to 0 when writing ES Descriptor (#755).
  - Properly handle AVC profiles with SPS extension (#755).
  - [HLS] Don't include FRAME-RATE in EXT-X-STREAM-INF (#816).
  - [HLS] Fix missing FRAME-RATE in playlists with TS streams (#816).
- [DASH] TrickPlay using separate trick play specific streams (#732).
- Don't fail if input contents contain SampleGroupDescriptionBox with 0 entries
  (#812).
- [HLS] Fixes attributes for DVS tracks (#857).
- Fix trick-mode property values (space instead of comma).
- Properly handle SkipBytes with num_bytes as 0 (#875).
- [MPEG-TS] Fix PCR reserved bits not being set correctly (#893).
- [HLS] Explicitly signal the lack of CEA captions (#922).

### Changed
- Change AV1 cbcs to protect all bytes of decode_tile structure (#698).
- [MP4] Allow not to generate 'sidx' box for single-segment too (#862).
- [WebM] Ignore matroska projection metadata instead of fail parsing (#932).
- Changed default HTTP UserAgent to ShakaPackager/<version> (#939).

## [2.4.3] - 2020-08-04
### Fixed
- Fix playback issue of HEVC content with cbcs encryption in AVplayer (#717).
- Fix possible incorrect resolutions with avc3 (#750).

## [2.4.2] - 2020-03-30
### Fixed
- Fix truncation of timestamp to 32bits in segment file names for MPEG2-TS
  output (#701).
- [DASH] Fix "roles" stream descriptor support for WebVTT text streams (#708).
- Fix potential deadlock when reading WebVTT from a pipe.

## [2.4.1] - 2020-01-17
### Fixed
- Fixed Windows buildbot (Appveyor builds).

## [2.4.0] - 2019-12-26
### Added
- Support hardware accelerated AES (#198).
- Support various HDR formats for HEVC (#341, #632).
- Add more loggings for GAPs (#474).
- Retry file deletion if it fails (#533). Only applicable to live packaging.
- Simplify the dependency for pssh-box utility (#538).
- Add crypto_period_duration to Widevine key requests (#545).
- Include pssh-box.py in docker image and release (#550).
- Support encryption using IV from Widevine key server (#555).
- [DASH] Support custom Accessibillity and Role elements (#565). This is needed
  to support DVS Accessibillity audio in DASH.
- Support CMAF file extensions (#574).
- Support PlayReady PSSH generation with CBCS protection scheme (#602).
- [HLS] Generate FRAME-RATE attribute in EXT-X-STREAM-INF tag (#634).
- Add --quiet to suppress LOG(INFO) outputs (#661).

### Fixed
- Handle large descriptor header size in 'esds' box (#536).
- Improve the handling of corrupted timestamp in live streams (#563).
- Fix problems that target duration is not set in mpd/hls params, which then
  results incorrect bandwidth estimates (#498, #581).
- Allow absolute path in playlist name (#585).
- [HLS] Fix possible zero bandwidth for EXT-I-FRAME-STREAM-INF (#610).
- Supports encryption of streams with parameter sets in frames, i.e. avc3, hvc1
  etc (#621, #627).
- [HLS] Segments not deleted with $Time$ in segment_template when output HLS
  segments only (#625).
- [HLS] ID3 payload for transportStreamTimestamp not truncated to 33 bits
  (#629).
- Fix UDP sockets support in Windows (#643).
- Fix possible packager hangs when reading mp4 files from FIFO (#664).

### Changed
- [HLS] Replace hev1 in codec with hvc1 and avc3 with avc1 (#587).
- Rename `--generate_static_mpd` to `--generate_static_live_mpd` (#672).

### Doc
- Added documentation for pssh-box utility (#500).

## [2.3.0] - 2018-12-20
### Added
- Alpine Linux support (#164).
- WebVTT style and region support (#344).
- Marlin DRM support (#381).
- HLS CHARACTERISTICS attribute on #EXT-X-MEDIA (#404).
- Default text language support ('--default_text_language') (#430).
- AV1 support (#453).
- HLS audio only master playlist support (#461).

### Fixed
- MPEG-TS demuxing with AC-3 / E-AC-3 (#487). kFrameSizeCodeTable were reversed
  results in wrong frame size being detected except for 44.1kHz.
- HLS peak bandwidth calculation with very short segments (#498). The short
  segments should be excluded from peak bandwidth calculation per HLS
  specification.
- Output directory permission (#499). The new directory permission was fixed to
  0700.
- Workaround access units with extra AUD (#526). VLC inserts an extra AUD in the
  key frames, which caused packager to delay emitting the frame. The delays
  accumulated and became noticeable after running the live packaging for some
  time.
- Problem when using Trick Play with Ad Cues (#528).

### Changed
- Disable bundled binutil and gold on Linux by default. There may be a slight
  increase in binary link time.
- Reduced official Docker image size from ~1GB to ~15MB (#535). The new image
  is based on Alpine and contains only result binaries (`packager` and
  `mpd_generator`).

## [2.2.1] - 2018-09-20
### Added
- Added support for seek preroll in AAC and other audio codecs (#452). This also
  addressed 'Unexpected seek preroll for codecs ...' warnings.
- Computes and sets VP9 Level in the codec config if it is not already set
  (#469). This fixed VP9 in ISO-BMFF files generated by FFmpeg v4.0.2 or earlier
  which does not have level set in the codec config.

### Fixed
- Added a workaround for TS contents with dts moving backwards (#451). So
  instead of generating a sample with negative duration, which ExoPlayer does
  not handle, use an arbitrarily short duration instead.
- Fixed pattern signaling in seig for key rotation with cbcs (#460).
- Fixed incorrect segment name with $Time$ in segment_template (#472). This
  resulted in the first segment being overwritten and led to playback problems.
- Fixed TTML text input passthrough in DASH (#478). This is a regression
  introduced in v2.2.0.

## [2.2.0] - 2018-08-16
### Added
- EditList support in ISO-BMFF in both input and output (#112).
- Multi-DRM support with --protection_systems flag (#245).
- HLS AVERAGE-BANDWIDTH support (#361).
- Dynamic Ad Insertion preconditioning support with Google Ad Manager (#362, #382, #384).
- Configurable UDP receiver buffer size (#411). This can help mitigate or
  eliminate packet loss due to receiver buffer overrun.
- Allow non-zero text start time (#416). Needed for live text packaging.

### Changed
- Deprecated --mp4_use_decoding_timestamp_in_timeline.
- Deprecated --num_subsegments_per_sidx.
- Generate DASH IF IOP compliant MPD with mpd_generator by default.
- Adjust timestamps in ISO-BMFF if there is an initial composition offset
  as we believe that an EditList is missing in this case (Related to #112).
- Add an adjustable offset to transport streams (MPEG2-TS, HLS Packed Audio)
  (Related to #112). The offset is configurable with
  --transport_stream_offset_ms. The default is 0.1 seconds.
- Set default --segment_duration to 6 seconds.
- Set default --clear_lead to 5 seconds. Shaka Packager does not support partial encrypted segments,
  so if segment_duration is 6 seconds, then only the first segment is in clear, with all the
  following segments encrypted.
- Set default --io_block_size to 64K.
- Disable Legacy Widevine HLS signaling for HLS with Widevine protection system by default. Use flag
  --enable_legacy_widevine_hls_signaling to enable it if needed.

### Fixed
- Build failures in Windows with CJK environment (#419).
- Segmentation fault when processing WebVTT with out of order cues (#425).
- Support WebVTT cues without payload (#433).
- segmentAlignment is not set correctly in static live profile for multi-period
  content (#435). Theoretically it could happen for single period content as
  well, but with very low possibility of occuring.
- Segmentation fault when packaging with an empty VTT file (#446).
- Possible file name collision when --temp_dir is used (#448).

### Doc
- Added documentation for PlayReady and FairPlay (#306).
- Added examples for TrickPlay.
- Fixed live HLS example (#403).
- Fixed DockerHub instructions link (#408).
- Added documentation for Dynamic Ad Insertion preconditioning.
- Added instructions for missing curl CA bundle on mac.

## [2.1.1] - 2018-07-03
### Changed
- Warn if HLS type is not set set to LIVE for UDP inputs (#347).
- Use new vp09 codec string for WebM by default (#406). Set command line flag:
  `--use_legacy_vp9_codec_string` if the old behavior is needed.
- Allow trailing null bytes in NAL units, to allow contents with the H264 spec
  violation to be processed instead of erroring out (#418).

### Fixed
- Fix MPD@duration not set with MPDGenerator (#401). This is a regression
  introduced in v2.0.1.
- Remove 'wvtt' in HLS master playlist codec string as it breaks some old Apple
  products, e.g. AppleTV3 (#402).
- Fix potential text Segment Timeline not being grouped together in DASH mpd
  (#417), which happens when `--allow_approximate_segment_timeline` is set.

## [2.1.0] - 2018-05-22
### Added
- Support Widevine and PlayReady PSSH generation internally in packager (#245).
  Documentation will be updated later.
- Support removing segments outside of live window in DASH and HLS (#223).
- Support UTCTiming for DASH (#311).
- Support approximate SegmentTimeline (#330) under flag
  --allow_approximate_segment_timeline. The flag is disabled by default and it
  will be enabled in a later release.
- Support UDP Source Specific Multicast (SSM) (#332).
- Support elementary audio (Packed Audio) for HLS (#342).
- Support FLAC codec (#345).
- Support AAC with program_config_element (#387).
- Support Widevine entitlement license with dual PSSH.
- Add license notice in --licenses.

### Changed
- Ignore unsupported audio codec in the source content (#395). This allows other
  supported streams to be processed and packaged.

### Fixed
- Fix bitrate for DASH on-demand profile too (#376).
- Fix Ad Cues and EXT-X-KEY tag handling in HLS iFrames only playlist
  (#378, #396).
- Skip Style and Region Blocks in the source instead of failing (#380).
- Fix potential slice header size off by one byte in H265 (#383).
- Fix potential partial DASH segments during live packaging (#386).
- Fix incorrect BOM used in WEBVTT header (#397).
- Fix TS mimetype in DASH.

## [2.0.3] - 2018-04-23
### Changed
- Removed --pto_adjustment flag (related to #368).

### Fixed
- Use max bitrate in Representation@bandwidth instead of average bitrate (#376).
- Set Widevine key request content-type to JSON instead of xml (#372).
- Fix default_language not working if 2-char code is used (#371).
- Do not force earliest_presentation_time to 0 for VOD (#303).
- Generate more precise time in Period@duration (#368). This avoids possible
  rounding error in MSE causing frames to be dropped.

## [2.0.2] - 2018-03-27
### Added
- Support cue alignment from multiple demuxed streams (#355).

## [2.0.1] - 2018-03-05
### Added
- Recognize m4s as a valid extension for init segment (#331). It is used to be
  allowed as the extension for media segments only.
- Improve DASH multi-period support: calculate presentationTimeOffset and
  Period@duration from video segment presentation timestamps. This avoids
  video playback jitters due to gaps.

### Fixed
- Handle invalid WebVTT with start_time == end_time gracefully (#335).
- Ignore invalid `meta` box in mp4 files, which Android's camera app generates
  (#319).
- Set stream duration in init segment for mp4 with static live profile (#340).

## [2.0.0] - 2018-02-10
### Added
- Enhanced HLS support.
  - Support for attributes RESOLUTION, CHANNELS, AUTOSELECT and DEFAULT.
  - Live and Event playlists.
  - fMP4 in HLS (including byte range support).
  - DRM: Widevine and FairPlay.
  - I-Frame playlist.
- Enhanced subtitle support.
  - Segmented WebVTT in fMP4.
  - Segmented WebVTT in text, for HLS.
- Support generating DASH + HLS manifests simultaneously (#262).
- AC3 / E-AC3 support.
- Experimental multi-period support.
- Raw key multi-key support.
- DASH Trickplay.
- Make fMP4 output CMAF compatible.
- Support for WebM colour element.
- Support skip_encryption stream descriptor fields (#219).
- Improved documentation and tutorials.

### Changed
- Refactored packager code and media pipeline.
- Exposed top level packaging interface.
- Renamed --webm_subsample_encryption flag to --vp9_subsample_encryption flag.
- Deprecated --availability_time_offset flag.

### Fixed
- Write manifests atomically to fix possible truncated manifests seen on clients
  (#186).
- [WebM] Fix live segmenter overflow if longer than two hours (#233).
- Fix a possible interferenace problem when re-using UDP multicast streams in
  different processes (#241).
- Create directories in the output path if not exist (#276).
- Fix order of H265 VPS, SPS, PPS in hvcC box (#297).
- Handle additional unused mdat properly (#298).
- Fix possible incorrect HEVC decoder configuration data (#312).
- Handle varying parameter sets in sample when converting from NAL unit stream
  to byte stream (#327).

## [1.6.2] - 2017-04-18
### Added
- Added an option to keep parameter set NAL units (SPS/PPS for H264,
  SPS/PPS/VPS for H265), which is necessary if the parameter set NAL units
  are varying from frame to frame. The flag is --strip_parameter_set_nalus,
  which is true by default. This addresses #206 (the flag needs to be set to
  false).

### Fixed
- Fixed the problem that sliding window logic is still active with DASH static
  live profile (#218).
- Fixed AAC-HE not correctly signaled in codec string (#225).
- [WebM] Fixed output truncated if using the same file for both input and
  output (#210).
- [WebM] Fixed possible integer overflow in stream duration in MPD manifest
  (#214).

## [1.6.1] - 2017-02-10
### Changed
- Enable --generate_dash_if_iop_compliant_mpd by default. This moves
  ContentProtection element from Representation to AdaptationSet. The feature
  can still be disabled by setting the flag to false if needed.

### Fixed
- MPD duration not set for live profile with static mpd (#201).

## [1.6.0] - 2017-01-13
### Added
- Added support for Windows (both 32-bit and 64-bit are supported).
- Added support for live profile with static mpd by setting flag
  --generate_static_mpd (#142). This allows on demand content to use segment
  template.
- Added support for tagging a specific audio AdaptationSet as the default /
  main AdaptationSet with --default_language flag (#155).
- Added UDP options support: udp://ip:port[?options]. Currently three options
  are supported: reuse=1|0 whether reusing UDP sockets are allowed (#133),
  interface=a.b.c.d interface address, timeout=microseconds for socket timeout.
- Added 4K and 8K encryption support (#163).

### Changed
- [WebM][VP9] Use subsample encryption by default for VP9 per latest WebM spec.
  The feature can be disabled by setting --webm_subsample_encryption=false.
- [WebM] Mimic mp4 behavior: either all the samples in a segment are encrypted
  or all the samples are clear.
- [WebM] Move index segment forward to right after init segment (#159).

### Fixed
- Fixed AdaptationSet switching signalling when
  --generate_dash_if_iop_compliant_mpd is enabled (#156).
- [H.264] Fixed access unit detection problem if there are multiple video slice
  NAL units in the same frame (#134).
- [WebVTT] Detect .webvtt as WebVTT files.
- [WebM] Fixed keyframe detection in BlockGroup for encrypted frames.
- [HLS] Fixed HLS playlist problem when clear lead is set to zero (#169).
- Fixed --version command.

### Deprecated
- Deprecated flag --udp_interface_address. Use udp options instead.
- Deprecated flags --single_segment and --profile. They are now derived from
  the presence of 'segment_template' in stream descriptors.

## [1.5.1] - 2016-07-25
### Added
- Added a runtime flag to use dts in timeline for mp4:
  --mp4_use_decoding_timestamp_in_timeline

### Changed
- Remove restriction that sps:gaps_in_frame_num_value_allowed_flag should be
  0 in h264. Packager should not care about this flag (#126).
- Remove restriction that sample duration cannot be zero. A warning message
  is printed instead (#127).

### Fixed
- Fix text formats (webvtt, ttml) not recognized problem (#130).

## [1.5.0] - 2016-07-12
### Added
- Added TS (output) and HLS (output) with SAMPLE-AES encryption support.
  Note that only H.264 and AAC are supported right now.
- Added support for CENCv3, i.e. 'cbcs', 'cbc1', 'cens' protection schemes.
- Added H.265 support in TS (input) and iso-bmff (input / output).
- Added experimental Opus in iso-bmff support.

### Changed
- Change project name from edash-packager to shaka-packager. Also replaces
  various references of edash in the code accordingly.

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

[2.5.1]: https://github.com/google/shaka-packager/compare/v2.5.0...v2.5.1
[2.5.0]: https://github.com/google/shaka-packager/compare/v2.4.3...v2.5.0
[2.4.3]: https://github.com/google/shaka-packager/compare/v2.4.2...v2.4.3
[2.4.2]: https://github.com/google/shaka-packager/compare/v2.4.1...v2.4.2
[2.4.1]: https://github.com/google/shaka-packager/compare/v2.4.0...v2.4.1
[2.4.0]: https://github.com/google/shaka-packager/compare/v2.3.0...v2.4.0
[2.3.0]: https://github.com/google/shaka-packager/compare/v2.2.1...v2.3.0
[2.2.1]: https://github.com/google/shaka-packager/compare/v2.2.0...v2.2.1
[2.2.0]: https://github.com/google/shaka-packager/compare/v2.1.1...v2.2.0
[2.1.1]: https://github.com/google/shaka-packager/compare/v2.1.0...v2.1.1
[2.1.0]: https://github.com/google/shaka-packager/compare/v2.0.3...v2.1.0
[2.0.3]: https://github.com/google/shaka-packager/compare/v2.0.2...v2.0.3
[2.0.2]: https://github.com/google/shaka-packager/compare/v2.0.1...v2.0.2
[2.0.1]: https://github.com/google/shaka-packager/compare/v2.0.0...v2.0.1
[2.0.0]: https://github.com/google/shaka-packager/compare/v1.6.2...v2.0.0
[1.6.2]: https://github.com/google/shaka-packager/compare/v1.6.1...v1.6.2
[1.6.1]: https://github.com/google/shaka-packager/compare/v1.6.0...v1.6.1
[1.6.0]: https://github.com/google/shaka-packager/compare/v1.5.1...v1.6.0
[1.5.1]: https://github.com/google/shaka-packager/compare/v1.5.0...v1.5.1
[1.5.0]: https://github.com/google/shaka-packager/compare/v1.4.0...v1.5.0
[1.4.1]: https://github.com/google/shaka-packager/compare/v1.4.0...v1.4.1
[1.4.0]: https://github.com/google/shaka-packager/compare/v1.3.1...v1.4.0
[1.3.1]: https://github.com/google/shaka-packager/compare/v1.3.0...v1.3.1
[1.3.0]: https://github.com/google/shaka-packager/compare/v1.2.0...v1.3.0
[1.2.1]: https://github.com/google/shaka-packager/compare/v1.2.0...v1.2.1
[1.2.0]: https://github.com/google/shaka-packager/compare/v1.1...v1.2.0
[1.1.0]: https://github.com/google/shaka-packager/compare/v1.0...v1.1
