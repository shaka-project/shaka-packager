# Changelog


## [3.2.0](https://github.com/shaka-project/shaka-packager/compare/v3.1.0...v3.2.0) (2024-05-11)


### Features

* support Dolby Vision profile 8.x (HEVC) and 10.x (AV1) in HLS and DASH  ([#1396](https://github.com/shaka-project/shaka-packager/issues/1396)) ([a99cfe0](https://github.com/shaka-project/shaka-packager/commit/a99cfe036f09de51b488f87f4cb126a1bcd3a286))


### Bug Fixes

* adaptation set IDs were referenced by lowest representation ID  ([#1394](https://github.com/shaka-project/shaka-packager/issues/1394)) ([94db9c9](https://github.com/shaka-project/shaka-packager/commit/94db9c9db3e73073925205355dd61a6dc9785065)), closes [#1393](https://github.com/shaka-project/shaka-packager/issues/1393)
* escape media URLs in MPD ([#1395](https://github.com/shaka-project/shaka-packager/issues/1395)) ([98b44d0](https://github.com/shaka-project/shaka-packager/commit/98b44d01df6a952466b5a1667818da877502da97))
* set yuv full range flag to 1 for VP9 with sRGB ([#1398](https://github.com/shaka-project/shaka-packager/issues/1398)) ([f6f60e5](https://github.com/shaka-project/shaka-packager/commit/f6f60e5fff8d5c9b13fbf65f494eba651050ccb9))

## [3.1.0](https://github.com/shaka-project/shaka-packager/compare/v3.0.4...v3.1.0) (2024-05-03)


### Features

* add missing DASH roles from ISO/IEC 23009-1 section 5.8.5.5 ([#1390](https://github.com/shaka-project/shaka-packager/issues/1390)) ([fe885b3](https://github.com/shaka-project/shaka-packager/commit/fe885b3ade020b197a04fc63ee41fd90e7e11a14))
* get start number from muxer and specify initial sequence number ([#879](https://github.com/shaka-project/shaka-packager/issues/879)) ([bb104fe](https://github.com/shaka-project/shaka-packager/commit/bb104fef5d745ac3a0a8c1e6fb4f1b1a9b27d8ae))
* teletext formatting ([#1384](https://github.com/shaka-project/shaka-packager/issues/1384)) ([4b5e80d](https://github.com/shaka-project/shaka-packager/commit/4b5e80d02c10fd1ddb8f7e0f2f1a8608782d8442))

## [3.0.4](https://github.com/shaka-project/shaka-packager/compare/v3.0.3...v3.0.4) (2024-03-27)


### Bug Fixes

* BaseURL missing when MPD base path is empty ([#1380](https://github.com/shaka-project/shaka-packager/issues/1380)) ([90c3c3f](https://github.com/shaka-project/shaka-packager/commit/90c3c3f9b3e8e36706b9769e574aa316e8bbb351)), closes [#1378](https://github.com/shaka-project/shaka-packager/issues/1378)
* Fix NPM binary selection on ARM Macs ([#1376](https://github.com/shaka-project/shaka-packager/issues/1376)) ([733af91](https://github.com/shaka-project/shaka-packager/commit/733af9128dfa7c46dd7a9fe3f8361ab50a829afe)), closes [#1375](https://github.com/shaka-project/shaka-packager/issues/1375)

## [3.0.3](https://github.com/shaka-project/shaka-packager/compare/v3.0.2...v3.0.3) (2024-03-12)


### Bug Fixes

* Fix NPM binary publication ([#1371](https://github.com/shaka-project/shaka-packager/issues/1371)) ([4cb6536](https://github.com/shaka-project/shaka-packager/commit/4cb653606047b086affc111321187c7889fa238a)), closes [#1369](https://github.com/shaka-project/shaka-packager/issues/1369)
* Fix tags in official Docker images and binaries ([#1370](https://github.com/shaka-project/shaka-packager/issues/1370)) ([d83c7b1](https://github.com/shaka-project/shaka-packager/commit/d83c7b1d4505a08ef578390115cf28bea77404c2)), closes [#1366](https://github.com/shaka-project/shaka-packager/issues/1366)

## [3.0.2](https://github.com/shaka-project/shaka-packager/compare/v3.0.1...v3.0.2) (2024-03-07)


### Bug Fixes

* duplicate representation id for TTML when forced ordering is on ([#1364](https://github.com/shaka-project/shaka-packager/issues/1364)) ([0fd815a](https://github.com/shaka-project/shaka-packager/commit/0fd815a160cc4546ea0c13ac916727777f5cdd41)), closes [#1362](https://github.com/shaka-project/shaka-packager/issues/1362)

## [3.0.1](https://github.com/shaka-project/shaka-packager/compare/v3.0.0...v3.0.1) (2024-03-05)


### Bug Fixes

* **CI:** Add Mac-arm64 to build matrix ([#1359](https://github.com/shaka-project/shaka-packager/issues/1359)) ([c456ad6](https://github.com/shaka-project/shaka-packager/commit/c456ad64d1291bcf057c22a5c34479fcb4bbda55))
* **CI:** Add missing Linux arm64 builds to release ([9c033b9](https://github.com/shaka-project/shaka-packager/commit/9c033b9d40087a9a4eef6a013d17fd696c44459c))

## [3.0.0](https://github.com/shaka-project/shaka-packager/compare/v2.6.1...v3.0.0) (2024-02-28)


### ⚠ BREAKING CHANGES

* Update all dependencies
* Drop Python 2 support in all scripts
* Replace glog with absl::log, tweak log output and flags
* Replace gyp build system with CMake

### Features

* Add input support for EBU Teletext in MPEG-TS ([#1344](https://github.com/shaka-project/shaka-packager/issues/1344)) ([71c175d](https://github.com/shaka-project/shaka-packager/commit/71c175d4b8fd7dd1ebb2df8ce06c575f54666738))
* Add install target to build system ([3e71302](https://github.com/shaka-project/shaka-packager/commit/3e71302ba46e5164db91495c5da5ba07fc88cfca))
* Add PlayReady support in HLS. ([#1011](https://github.com/shaka-project/shaka-packager/issues/1011)) ([96efc5a](https://github.com/shaka-project/shaka-packager/commit/96efc5aa70c2152a242f36644100161bc562d3f7))
* add startwithSAP/subsegmentstartswithSAP for audio tracks ([#1346](https://github.com/shaka-project/shaka-packager/issues/1346)) ([d23cce8](https://github.com/shaka-project/shaka-packager/commit/d23cce85b93263a4c7541d9761145be54dd1348d))
* Add support for ALAC codec ([#1299](https://github.com/shaka-project/shaka-packager/issues/1299)) ([b68ec87](https://github.com/shaka-project/shaka-packager/commit/b68ec87f6a552c27882746a14415bb334041d86b))
* Add support for single file TS for HLS ([#934](https://github.com/shaka-project/shaka-packager/issues/934)) ([4aa4b4b](https://github.com/shaka-project/shaka-packager/commit/4aa4b4b9aac08fdcf10954de37b9982761fa42c1))
* Add support for the EXT-X-START tag ([#973](https://github.com/shaka-project/shaka-packager/issues/973)) ([76eb2c1](https://github.com/shaka-project/shaka-packager/commit/76eb2c1575d4c8b32782292dc5216c444a6f2b27))
* Add xHE-AAC support ([#1092](https://github.com/shaka-project/shaka-packager/issues/1092)) ([5d998fc](https://github.com/shaka-project/shaka-packager/commit/5d998fca7fb1d3d9c98f5b26561f842edcb2a925))
* Allow LIVE UDP WebVTT input ([#1349](https://github.com/shaka-project/shaka-packager/issues/1349)) ([89376d3](https://github.com/shaka-project/shaka-packager/commit/89376d3c4d3f3005de64dd4bc1668297024dfe46))
* **DASH:** Add Label element. ([#1175](https://github.com/shaka-project/shaka-packager/issues/1175)) ([b1c5a74](https://github.com/shaka-project/shaka-packager/commit/b1c5a7433e1c148fb6648aadd36c14840e1a4b50))
* **DASH:** Add video transfer characteristics. ([#1210](https://github.com/shaka-project/shaka-packager/issues/1210)) ([8465f5f](https://github.com/shaka-project/shaka-packager/commit/8465f5f020b5c6152d24107a6d164301e05c3176))
* default text zero bias ([#1330](https://github.com/shaka-project/shaka-packager/issues/1330)) ([2ba67bc](https://github.com/shaka-project/shaka-packager/commit/2ba67bc24cf6116349ad16d3bfa1a121c95a3173))
* Drop Python 2 support in all scripts ([3e71302](https://github.com/shaka-project/shaka-packager/commit/3e71302ba46e5164db91495c5da5ba07fc88cfca))
* Generate the entire AV1 codec string when the colr atom is present ([#1205](https://github.com/shaka-project/shaka-packager/issues/1205)) ([cc9a691](https://github.com/shaka-project/shaka-packager/commit/cc9a691aef946dfb4d68077d3a741ef1b88d2f21)), closes [#1007](https://github.com/shaka-project/shaka-packager/issues/1007)
* HLS / DASH support forced subtitle ([#1020](https://github.com/shaka-project/shaka-packager/issues/1020)) ([f73ad0d](https://github.com/shaka-project/shaka-packager/commit/f73ad0d9614ba5b1ba552385c0157b7017dba204))
* Move all third-party deps into git submodules ([#1083](https://github.com/shaka-project/shaka-packager/issues/1083)) ([3e71302](https://github.com/shaka-project/shaka-packager/commit/3e71302ba46e5164db91495c5da5ba07fc88cfca))
* order streams in manifest based on command-line order ([#1329](https://github.com/shaka-project/shaka-packager/issues/1329)) ([aad2a12](https://github.com/shaka-project/shaka-packager/commit/aad2a12a9d6b79abe511d66e9e8d936e0ba46cb4))
* Parse MPEG-TS PMT ES language and maximum bitrate descriptors ([#369](https://github.com/shaka-project/shaka-packager/issues/369)) ([#1311](https://github.com/shaka-project/shaka-packager/issues/1311)) ([c09eb83](https://github.com/shaka-project/shaka-packager/commit/c09eb831b85c93eb122373ce82b64c354d68c3c1))
* Portable, fully-static release executables on Linux ([#1351](https://github.com/shaka-project/shaka-packager/issues/1351)) ([9be7c2b](https://github.com/shaka-project/shaka-packager/commit/9be7c2b1ac63d2299ed8293c381ef5877003b5c4))
* Replace glog with absl::log, tweak log output and flags ([3e71302](https://github.com/shaka-project/shaka-packager/commit/3e71302ba46e5164db91495c5da5ba07fc88cfca))
* Replace gyp build system with CMake ([3e71302](https://github.com/shaka-project/shaka-packager/commit/3e71302ba46e5164db91495c5da5ba07fc88cfca)), closes [#1047](https://github.com/shaka-project/shaka-packager/issues/1047)
* Respect the file mode for HttpFiles ([#1081](https://github.com/shaka-project/shaka-packager/issues/1081)) ([3e71302](https://github.com/shaka-project/shaka-packager/commit/3e71302ba46e5164db91495c5da5ba07fc88cfca))
* This patch adds support for DTS:X Profile 2 audio in MP4 files. ([#1303](https://github.com/shaka-project/shaka-packager/issues/1303)) ([07f780d](https://github.com/shaka-project/shaka-packager/commit/07f780dae19d7953bd27b646d9410ed68b4b580e))
* Update all dependencies ([3e71302](https://github.com/shaka-project/shaka-packager/commit/3e71302ba46e5164db91495c5da5ba07fc88cfca))
* Write colr atom to muxed mp4 ([#1261](https://github.com/shaka-project/shaka-packager/issues/1261)) ([f264bef](https://github.com/shaka-project/shaka-packager/commit/f264befe868a9fbf054697a6d06218cc25de055d)), closes [#1202](https://github.com/shaka-project/shaka-packager/issues/1202)


### Bug Fixes

* Accept 100% when parsing WEBVTT regions ([#1006](https://github.com/shaka-project/shaka-packager/issues/1006)) ([e1b0c7c](https://github.com/shaka-project/shaka-packager/commit/e1b0c7c45431327fd3ce193514a5407d07b39b22)), closes [#1004](https://github.com/shaka-project/shaka-packager/issues/1004)
* Add missing &lt;cstdint&gt; includes ([#1306](https://github.com/shaka-project/shaka-packager/issues/1306)) ([ba5c771](https://github.com/shaka-project/shaka-packager/commit/ba5c77155a6b0263f48f24f93033c7a386bc83b6)), closes [#1305](https://github.com/shaka-project/shaka-packager/issues/1305)
* Always log to stderr by default ([#1350](https://github.com/shaka-project/shaka-packager/issues/1350)) ([35c2f46](https://github.com/shaka-project/shaka-packager/commit/35c2f4642830e1446701abe17ae6d3b96d6043ae)), closes [#1325](https://github.com/shaka-project/shaka-packager/issues/1325)
* AudioSampleEntry size caluations due to bad merge ([#1354](https://github.com/shaka-project/shaka-packager/issues/1354)) ([615720e](https://github.com/shaka-project/shaka-packager/commit/615720e7dd9699f2d90c63d4bb6ac45c0e804d32))
* dash_roles add role=description for DVS audio per DASH-IF-IOP-v4.3 ([#1054](https://github.com/shaka-project/shaka-packager/issues/1054)) ([dc03952](https://github.com/shaka-project/shaka-packager/commit/dc0395291a090b342beaab2e89c627dc33ee89b0))
* Don't close upstream on HttpFile::Flush ([#1201](https://github.com/shaka-project/shaka-packager/issues/1201)) ([53d91cd](https://github.com/shaka-project/shaka-packager/commit/53d91cd0f1295a0c3456cb1a34e5235a0316c523)), closes [#1196](https://github.com/shaka-project/shaka-packager/issues/1196)
* duration formatting and update mpd testdata to reflect new format ([#1320](https://github.com/shaka-project/shaka-packager/issues/1320)) ([56bd823](https://github.com/shaka-project/shaka-packager/commit/56bd823339bbb9ba94ed60a84c554864b42cf94a))
* Fix build errors related to std::numeric_limits ([#972](https://github.com/shaka-project/shaka-packager/issues/972)) ([9996c73](https://github.com/shaka-project/shaka-packager/commit/9996c736aea79e0cce22bee18dc7dcabfffff47b))
* Fix build on FreeBSD ([#1287](https://github.com/shaka-project/shaka-packager/issues/1287)) ([3e71302](https://github.com/shaka-project/shaka-packager/commit/3e71302ba46e5164db91495c5da5ba07fc88cfca))
* Fix clang build ([#1288](https://github.com/shaka-project/shaka-packager/issues/1288)) ([3e71302](https://github.com/shaka-project/shaka-packager/commit/3e71302ba46e5164db91495c5da5ba07fc88cfca))
* Fix failure on very short WebVTT files ([#1216](https://github.com/shaka-project/shaka-packager/issues/1216)) ([dab165d](https://github.com/shaka-project/shaka-packager/commit/dab165d3e5d979e2e5ff783d91d948357b932078)), closes [#1217](https://github.com/shaka-project/shaka-packager/issues/1217)
* Fix handling of non-interleaved multi track FMP4 files ([#1214](https://github.com/shaka-project/shaka-packager/issues/1214)) ([dcf3225](https://github.com/shaka-project/shaka-packager/commit/dcf32258ffd725bc3de06c9bceb86fc8a403ecba)), closes [#1213](https://github.com/shaka-project/shaka-packager/issues/1213)
* Fix issues with `collections.abc` in Python 3.10+ ([#1188](https://github.com/shaka-project/shaka-packager/issues/1188)) ([80e0240](https://github.com/shaka-project/shaka-packager/commit/80e024013df87a4bfeb265c8ea83cfa2a0c5db0f)), closes [#1192](https://github.com/shaka-project/shaka-packager/issues/1192)
* Fix local files with UTF8 names ([#1246](https://github.com/shaka-project/shaka-packager/issues/1246)) ([3e71302](https://github.com/shaka-project/shaka-packager/commit/3e71302ba46e5164db91495c5da5ba07fc88cfca))
* Fix missing newline at the end of usage ([#1352](https://github.com/shaka-project/shaka-packager/issues/1352)) ([6276584](https://github.com/shaka-project/shaka-packager/commit/6276584de784025380f46dca379795ed6ee42b8f))
* Fix Python 3.10+ compatibility in scripts ([3e71302](https://github.com/shaka-project/shaka-packager/commit/3e71302ba46e5164db91495c5da5ba07fc88cfca))
* Fix uninitialized value found by Valgrind ([#1336](https://github.com/shaka-project/shaka-packager/issues/1336)) ([7ef5167](https://github.com/shaka-project/shaka-packager/commit/7ef51671f1a221443bcd000ccb13189ee6ccf749))
* Fix various build issues on macOS ([3e71302](https://github.com/shaka-project/shaka-packager/commit/3e71302ba46e5164db91495c5da5ba07fc88cfca))
* Fix various build issues on Windows ([3e71302](https://github.com/shaka-project/shaka-packager/commit/3e71302ba46e5164db91495c5da5ba07fc88cfca))
* hls, set the DEFAULT explicitly to NO. Supports native HLS players. ([#1170](https://github.com/shaka-project/shaka-packager/issues/1170)) ([1ab6818](https://github.com/shaka-project/shaka-packager/commit/1ab68188326685ab4e427e7a6eab0694e0b0b60a)), closes [#1169](https://github.com/shaka-project/shaka-packager/issues/1169)
* http_file: Close upload cache on task exit ([#1348](https://github.com/shaka-project/shaka-packager/issues/1348)) ([6acdcc3](https://github.com/shaka-project/shaka-packager/commit/6acdcc394a1583b22c3c16cee3e419eea6c6e4f9)), closes [#1347](https://github.com/shaka-project/shaka-packager/issues/1347)
* Indexing `bytes` produces `int` on python3 for `pssh-box.py` ([#1228](https://github.com/shaka-project/shaka-packager/issues/1228)) ([d9d3c7f](https://github.com/shaka-project/shaka-packager/commit/d9d3c7f8be13e493a99b2ff4b72a402c441c1666)), closes [#1227](https://github.com/shaka-project/shaka-packager/issues/1227)
* Low Latency DASH: include the "availabilityTimeComplete=false" attribute ([#1198](https://github.com/shaka-project/shaka-packager/issues/1198)) ([d687ad1](https://github.com/shaka-project/shaka-packager/commit/d687ad1ed00da260c4f4e5169b042ef4291052a0))
* misleading log output when HLS target duration updates (fixes [#969](https://github.com/shaka-project/shaka-packager/issues/969)) ([#971](https://github.com/shaka-project/shaka-packager/issues/971)) ([f7b3986](https://github.com/shaka-project/shaka-packager/commit/f7b3986818915ab3d7d3dc31b8316fcef0384294))
* **MP4:** Add compatible brand dby1 for Dolby content. ([#1211](https://github.com/shaka-project/shaka-packager/issues/1211)) ([520926c](https://github.com/shaka-project/shaka-packager/commit/520926c27ad0d183127e4548c4564af33a2ad2f3))
* Parse one frame mpeg-ts video ([#1015](https://github.com/shaka-project/shaka-packager/issues/1015)) ([b221aa9](https://github.com/shaka-project/shaka-packager/commit/b221aa9caf4f8357a696f3265d1e2a5bf504dbd9)), closes [#1013](https://github.com/shaka-project/shaka-packager/issues/1013)
* preserve case for stream descriptors ([#1321](https://github.com/shaka-project/shaka-packager/issues/1321)) ([5d44368](https://github.com/shaka-project/shaka-packager/commit/5d44368478bbd0cd8bd06f0dfa739f8cfa032ddc))
* Prevent crash in GetEarliestTimestamp() if periods are empty ([#1173](https://github.com/shaka-project/shaka-packager/issues/1173)) ([d6f28d4](https://github.com/shaka-project/shaka-packager/commit/d6f28d456c6ec5ecf39c868447d85294a698166d)), closes [#1172](https://github.com/shaka-project/shaka-packager/issues/1172)
* PTS diverge DTS when DTS close to 2pow33 and PTS more than 0 ([#1050](https://github.com/shaka-project/shaka-packager/issues/1050)) ([ab8ab12](https://github.com/shaka-project/shaka-packager/commit/ab8ab12d098c352372014180bd2cb5407e018739)), closes [#1049](https://github.com/shaka-project/shaka-packager/issues/1049)
* remove extra block assumptions in mbedtls integration ([#1323](https://github.com/shaka-project/shaka-packager/issues/1323)) ([db59ad5](https://github.com/shaka-project/shaka-packager/commit/db59ad582a353fa1311563bdde93c49449159859)), closes [#1316](https://github.com/shaka-project/shaka-packager/issues/1316)
* Restore support for legacy FairPlay system ID ([#1357](https://github.com/shaka-project/shaka-packager/issues/1357)) ([4d22e99](https://github.com/shaka-project/shaka-packager/commit/4d22e99f8e7777557f5de3d5439f7e7b397e4323))
* Roll back depot_tools, bypass vpython ([#1045](https://github.com/shaka-project/shaka-packager/issues/1045)) ([3fd538a](https://github.com/shaka-project/shaka-packager/commit/3fd538a587184a87e2b41a526e089247007aa526)), closes [#1023](https://github.com/shaka-project/shaka-packager/issues/1023)
* set array_completeness in HEVCDecoderConfigurationRecord correctly ([#975](https://github.com/shaka-project/shaka-packager/issues/975)) ([270888a](https://github.com/shaka-project/shaka-packager/commit/270888abb12b2181ff84071f2c2685bd196de6fe))
* TTML generator timestamp millisecond formatting ([#1179](https://github.com/shaka-project/shaka-packager/issues/1179)) ([494769c](https://github.com/shaka-project/shaka-packager/commit/494769ca864e04d582f707934a6573cad78d2e8c)), closes [#1180](https://github.com/shaka-project/shaka-packager/issues/1180)
* Update golden files for ttml tests and failing hls unit tests. ([#1226](https://github.com/shaka-project/shaka-packager/issues/1226)) ([ac47e52](https://github.com/shaka-project/shaka-packager/commit/ac47e529ad7b69cc232f7f96e2a042990505776f))
* Update to use official FairPlay UUID. ([#1281](https://github.com/shaka-project/shaka-packager/issues/1281)) ([ac59b9e](https://github.com/shaka-project/shaka-packager/commit/ac59b9ebc94018b216c657854ca5163c9d2e7f31))
* use a better estimate of frame rate for cases with very short first sample durations ([#838](https://github.com/shaka-project/shaka-packager/issues/838)) ([5644041](https://github.com/shaka-project/shaka-packager/commit/56440413aa32a13cbfbb41a6d5ee611bae02ab2e))
* webvtt single cue do not fail on EOS ([#1061](https://github.com/shaka-project/shaka-packager/issues/1061)) ([b9d477b](https://github.com/shaka-project/shaka-packager/commit/b9d477b969f34124dfe184b4ac1d00ea8faf0a7d)), closes [#1018](https://github.com/shaka-project/shaka-packager/issues/1018)

## [2.6.1] - 2021-10-14
### Fixed
 - Fix crash in static-linked linux builds (#996)
 - Update outdated Dockerfiles

## [2.6.0] - 2021-09-07
### Added
 - Low latency DASH support (#979)
 - Added MPEG-H support (mha1, mhm1) (#930, #952)

### Fixed
 - Workaround warning spam using http_file (#948)
 - Fixed various python2/3 issues in the build
 - Fixed builds with CC=clang CXX=clang++

### Changed
 - Added arm64 to the build matrix
 - Make release binary names more consistent
 - Produce static release executables on Linux (#978, #965)
 - Stop using hermetic clang, libc++, etc
   - "gclient sync" now runs 20-30% faster
   - "ninja -C out/Release" now runs 5-13% faster
   - No longer required:
     - DEPOT_TOOLS_WIN_TOOLCHAIN environment variable
     - MACOSX_DEPLOYMENT_TARGET environment variable
     - clang=0 gyp variable
     - host_clang=0 gyp variable
     - clang_xcode=1 gyp variable
     - use_allocator=none gyp variable
     - use_experimental_allocator_shim=0 gyp variable


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

[2.6.1]: https://github.com/shaka-project/shaka-packager/compare/v2.6.0...v2.6.1
[2.6.0]: https://github.com/shaka-project/shaka-packager/compare/v2.5.1...v2.6.0
[2.5.1]: https://github.com/shaka-project/shaka-packager/compare/v2.5.0...v2.5.1
[2.5.0]: https://github.com/shaka-project/shaka-packager/compare/v2.4.3...v2.5.0
[2.4.3]: https://github.com/shaka-project/shaka-packager/compare/v2.4.2...v2.4.3
[2.4.2]: https://github.com/shaka-project/shaka-packager/compare/v2.4.1...v2.4.2
[2.4.1]: https://github.com/shaka-project/shaka-packager/compare/v2.4.0...v2.4.1
[2.4.0]: https://github.com/shaka-project/shaka-packager/compare/v2.3.0...v2.4.0
[2.3.0]: https://github.com/shaka-project/shaka-packager/compare/v2.2.1...v2.3.0
[2.2.1]: https://github.com/shaka-project/shaka-packager/compare/v2.2.0...v2.2.1
[2.2.0]: https://github.com/shaka-project/shaka-packager/compare/v2.1.1...v2.2.0
[2.1.1]: https://github.com/shaka-project/shaka-packager/compare/v2.1.0...v2.1.1
[2.1.0]: https://github.com/shaka-project/shaka-packager/compare/v2.0.3...v2.1.0
[2.0.3]: https://github.com/shaka-project/shaka-packager/compare/v2.0.2...v2.0.3
[2.0.2]: https://github.com/shaka-project/shaka-packager/compare/v2.0.1...v2.0.2
[2.0.1]: https://github.com/shaka-project/shaka-packager/compare/v2.0.0...v2.0.1
[2.0.0]: https://github.com/shaka-project/shaka-packager/compare/v1.6.2...v2.0.0
[1.6.2]: https://github.com/shaka-project/shaka-packager/compare/v1.6.1...v1.6.2
[1.6.1]: https://github.com/shaka-project/shaka-packager/compare/v1.6.0...v1.6.1
[1.6.0]: https://github.com/shaka-project/shaka-packager/compare/v1.5.1...v1.6.0
[1.5.1]: https://github.com/shaka-project/shaka-packager/compare/v1.5.0...v1.5.1
[1.5.0]: https://github.com/shaka-project/shaka-packager/compare/v1.4.0...v1.5.0
[1.4.1]: https://github.com/shaka-project/shaka-packager/compare/v1.4.0...v1.4.1
[1.4.0]: https://github.com/shaka-project/shaka-packager/compare/v1.3.1...v1.4.0
[1.3.1]: https://github.com/shaka-project/shaka-packager/compare/v1.3.0...v1.3.1
[1.3.0]: https://github.com/shaka-project/shaka-packager/compare/v1.2.0...v1.3.0
[1.2.1]: https://github.com/shaka-project/shaka-packager/compare/v1.2.0...v1.2.1
[1.2.0]: https://github.com/shaka-project/shaka-packager/compare/v1.1...v1.2.0
[1.1.0]: https://github.com/shaka-project/shaka-packager/compare/v1.0...v1.1
