## Sync with Google code

- sync with Google main branch and make PR
- fix all merge conflicts

# Linux Dockers
we disable OpenSUSE_Dockerfile for now, since it is still broken on Google's side

# Submodules
 The Shaka uses curl as submodule. Please make sure you are pointed to the correct branch (the same as original Shaka branch). Don't update it, otherwise it will break the build.

# Sync with Transcoder's version of Shaka
 - added ID3 support
 
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
