## [3.0.0] - 2025-03-18
- Sync with Google Shaka version 3.4.2
- Sync with PlutoTV ST version

## [2.6.1_pluto.v8.5.2] - 2025-01-27
### Added
 - Update schemeIdUri for ID3 PRIV tags to use scheme "https://aomedia.org/emsg/ID3". [TRANS-7290][https://plutotv.atlassian.net/browse/TRANS-7290]
 
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
### Added
 - Add CLI option to skip empty edits inside a editlist during a packaging
 - Add ability to package empty web vtt.