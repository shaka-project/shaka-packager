## Sync with Google code

- sync with Google main branch and make PR
- fix all merge conflicts

# Linux Dockers
we disable OpenSUSE_Dockerfile for now, since it is still broken on Google's side

# Submodules
 The Shaka uses curl as submodule. Please make sure you are pointed to the correct branch (the same as original Shaka branch). Don't update it, otherwise it will break the build.

# Sync with Transcoder's version of Shaka
 - added ID3 support

# Build with dubug and as shared lib

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON -S . -B build/
cmake --build build --parallel

ctest -C Debug -V --test-dir build

# Running individual test from Python code

1. change directory in the copy of Python script, located at /buil/packager/packager_test.py
2. find the test you want to run, for example:
```python
    def testAudioVideoWithTrickPlay(self):
    
    self.tmp_dir = './tmp'
    self.mpd_output = os.path.join(self.tmp_dir, 'output.mpd')

    streams = [
        self._GetStream('audio'),
        self._GetStream('video'),
        self._GetStream('video', trick_play_factor=1),
    ]

    self.assertPackageSuccess(streams, self._GetFlags(output_dash=True))
    self._CheckTestResults('audio-video-with-trick-play')

    self.tmp_dir = tempfile.mkdtemp()
    self.mpd_output = os.path.join(self.tmp_dir, 'output.mpd')

```
you are changing output for the test, so it will not conflict with other tests.

3. run the test with:

```bash
    python3 "packager_test.py"
```

# ClearKey support for HLS with MPEG TS streams.
- command line sample:
packager \
    in=media/video_480p.mp4,stream=video,drm_label=k_0,segment_template=result/480/video_480p_$Number%05d$.ts,playlist_name=video_480p.m3u8 \
    in=media/video_480p.mp4,stream=audio,drm_label=k_0,hls_group_id=audio,language=en,hls_name=audio_default,segment_template=result/audio/en/$Number%05d$.ts, playlist_name=audio_en.m3u8 \
    --hls_master_playlist_output=result/master.m3u8 \
    --segment_duration=5 \ 
    --clear_lead=5 \
    --hls_playlist_type=VOD \
    --enable_raw_key_encryption \
    --protection_scheme=aes128 \
    --hls_key_uri=aes128.bin \
    --keys=label=k_0:key_id=0000000067e1bf604279e91b925cc2ed:key=d59fab7cc84fef9df4c1e39ca9be395c:iv=00000000000000000000000000000001
