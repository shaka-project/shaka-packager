## [2.6.1_pluto.v8] - 2022-12-12
### Added
 - Creates a psuedo random starting EMSG ID based on the content ID. [TRANS-3036][https://plutotv.atlassian.net/browse/TRANS-3036]
 
## [2.6.1_pluto.v4] - 2022-06-02 - it was in Google base
### Added
 - Add label attribute in AdaptationSet for DASH manifest. [TRANS-2319][https://plutotv.atlassian.net/browse/TRANS-2319]

## [2.6.1_pluto.v3] - 2022-04-06 synced
### Added
 - Add InbandEventStream element to MPD. [PLAYOUT-4528][https://plutotv.atlassian.net/browse/PLAYOUT-4528]

## [2.6.1_pluto.v2] - 2022-03-25 - synced
### Added
 - Add ability to dynamically create and insert ID3 EMSG For AD Events. [TRANS-1686][https://plutotv.atlassian.net/browse/TRANS-1686]

## [2.6.1_pluto.v1] - 2022-01-27 - synced
### Fixed
 - Fix WebVTT: END_OF_STREAM error when there is no or only one cue (#1018)
### Added
 - Add CLI option to skip empty edits inside a editlist during a packaging
 - Add ability to package empty web vtt.