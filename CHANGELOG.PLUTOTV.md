## [3.4.2-pluto.v9.1.3] - 2025-08-15
- additional artifact for Linux arm64

## [3.4.2-pluto.v9.1.1] - 2025-06-26
- Fix for AdaptationSet switching

## [3.4.2-pluto.v9.1.0] - 2025-04-28
- ClearKey encryption for MPEG TS streams. Single key

## [3.4.2_pluto.v9.0.0] - 2025-04-03
 - code synced with Google Shaka Release 3.4.2
 - code synced with shaka-packager version, used in Service Transcoder

## [2.6.1_pluto.v8.5.2] - 2025-01-27
### Added
 - Update schemeIdUri for ID3 PRIV tags to use scheme "https://aomedia.org/emsg/ID3". [TRANS-7290][https://plutotv.atlassian.net/browse/TRANS-7290]
## [2.6.1_pluto.v8.4.0] - 2024-02-20
### Added
 - Update shaka packager to include styles in the vtt to ttml conversion and manifest changes to use in dvb dash. [TRANS-4844][https://plutotv.atlassian.net/browse/TRANS-4844]
 - Update shaka packager to enable ttml to ttml+mp4 and vtt+mp4 conversion. [TRANS-4844][https://plutotv.atlassian.net/browse/TRANS-4844]
 - Added optional duration flag for packaging subtitles by themselves. [TRANS-5423][https://plutotv.atlassian.net/browse/TRANS-5423]
## [2.6.1_pluto.v8.3.0] - 2023-11-23
### Added
 - Update shaka packager to enable ttml to ttml+mp4 and vtt+mp4 conversion. [TRANS-4845][https://plutotv.atlassian.net/browse/TRANS-4845]
## [2.6.1_pluto.v8.1] - 2022-12-12
### Added
 - Creates a psuedo random starting EMSG ID based on the content ID. [TRANS-3036][https://plutotv.atlassian.net/browse/TRANS-3036]
## [2.6.1_pluto.v4] - 2022-06-02
### Added
 - Add label attribute in AdaptationSet for DASH manifest. [TRANS-2319][https://plutotv.atlassian.net/browse/TRANS-2319]

## [2.6.1_pluto.v3] - 2022-04-06
### Added
 - Add InbandEventStream element to MPD. [PLAYOUT-4528][https://plutotv.atlassian.net/browse/PLAYOUT-4528]

## [2.6.1_pluto.v2] - 2022-03-25
### Added
 - Add ability to dynamically create and insert ID3 EMSG For AD Events. [TRANS-1686][https://plutotv.atlassian.net/browse/TRANS-1686]

## [2.6.1_pluto.v1] - 2022-01-27
### Fixed
 - Fix WebVTT: END_OF_STREAM error when there is no or only one cue (#1018)
### Changed
 - Limited amount of platforms and docker images for CI/CD
### Added
 - Add CLI option to skip empty edits inside a editlist during a packaging
 - Add ability to package empty web vtt.