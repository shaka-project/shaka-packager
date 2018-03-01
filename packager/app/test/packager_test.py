#!/usr/bin/python
#
# Copyright 2014 Google Inc. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd
"""Tests utilizing the sample packager binary."""

import filecmp
import glob
import os
import platform
import re
import shutil
import subprocess
import tempfile
import unittest

import packager_app
import test_env

_TEST_FAILURE_COMMAND_LINE_MESSAGE = """
!!! To reproduce the failure, change the output files to an !!!
!!! existing directory, e.g. output artifacts to current    !!!
!!! directory by removing /tmp/something/ in the following  !!!
!!! command line.                                           !!!
The test executed the following command line:
""".strip()


class StreamDescriptor(object):
  """Basic class used to build stream descriptor commands."""

  def __init__(self, input_file):
    self.buffer = 'input=%s' % input_file

  def Append(self, key, value):
    self.buffer += ',%s=%s' % (key, value)
    return self

  def __str__(self):
    return self.buffer


def _UpdateMpdTimes(mpd_filepath):
  # Take a single pattern, and replace the first match with the
  # given new string.
  def _Replace(str_in, pattern, new):
    m = re.search(pattern, str_in)

    if m:
      old = m.group(0)
      out = str_in.replace(old, new)
      print 'Replacing "%s" with "%s"' % (old, new)
    else:
      out = str_in

    return out

  with open(mpd_filepath, 'rb') as f:
    content = f.read()

  content = _Replace(
      content,
      'availabilityStartTime="[^"]+"',
      'availabilityStartTime="some_time"')

  content = _Replace(
      content,
      'publishTime="[^"]+"',
      'publishTime="some_time"')

  with open(mpd_filepath, 'wb') as f:
    f.write(content)


def GetExtension(stream_descriptor, output_format):
  # TODO(rkuroiwa): Support ttml.
  if stream_descriptor == 'text':
    return 'vtt'
  if output_format:
    return output_format
  # Default to mp4.
  return 'mp4'


def GetSegmentedExtension(base_extension):
  if base_extension == 'mp4':
    return 'm4s'

  return base_extension


class PackagerAppTest(unittest.TestCase):

  def setUp(self):
    self.packager = packager_app.PackagerApp()
    self.tmp_dir = tempfile.mkdtemp()
    self.test_data_dir = os.path.join(test_env.SRC_DIR, 'packager', 'media',
                                      'test', 'data')
    self.golden_file_dir = os.path.join(test_env.SRC_DIR, 'packager', 'app',
                                        'test', 'testdata')
    self.mpd_output = os.path.join(self.tmp_dir, 'output.mpd')
    self.hls_master_playlist_output = os.path.join(self.tmp_dir, 'output.m3u8')
    self.output = []

    # Test variables.
    self.encryption_key_id = '31323334353637383930313233343536'
    if test_env.options.encryption_key:
      self.encryption_key = test_env.options.encryption_key
    else:
      self.encryption_key = '32333435363738393021323334353637'
    if test_env.options.encryption_iv:
      self.encryption_iv = test_env.options.encryption_iv
    else:
      self.encryption_iv = '3334353637383930'
    self.widevine_content_id = '3031323334353637'
    # TS files may have a non-zero start, which could result in the first
    # segment to be less than 1 second. Set clear_lead to be less than 1
    # so only the first segment is left in clear.
    self.clear_lead = 0.8

  def tearDown(self):
    if test_env.options.remove_temp_files_after_test:
      shutil.rmtree(self.tmp_dir)

  def _GetStream(self,
                 descriptor,
                 language=None,
                 output_format=None,
                 segmented=False,
                 hls=False,
                 trick_play_factor=None,
                 drm_label=None,
                 skip_encryption=None,
                 test_file=None,
                 test_file_index=None):
    """Get a stream descriptor as a string.


    Create the stream descriptor as a string for the given parameters so that
    it can be passed as an input parameter to the packager.


    Args:
      descriptor: The name of the stream in the container that should be used as
          input for the output.
      language: The language override for the input stream.
      output_format: Specify the format for the output.
      segmented: Should the output use a segmented formatted. This will affect
          the output extensions and manifests.
      hls: Should the output be for an HLS manifest.
      trick_play_factor: Signals the stream is to be used for a trick play
          stream and which key frames to use. A trick play factor of 0 is the
          same as not specifying a trick play factor.
      drm_label: Sets the drm label for the stream.
      skip_encryption: If set to true, the stream will not be encrypted.
      test_file: Specify the input file to use. If the input file is not
          specify, a default file will be used.
      test_file_index: Specify the index of the input out of a group of input
          files.


    Returns:
      A string that makes up a single stream descriptor for input to the
      packager.
    """

    input_file_name = test_file or 'bear-640x360.mp4'
    input_file_path = os.path.join(self.test_data_dir, input_file_name)

    stream = StreamDescriptor(input_file_path)
    stream.Append('stream', descriptor)

    if output_format:
      stream.Append('format', output_format)

    if language:
      stream.Append('lang', language)

    if test_file_index is None:
      output_file_name = 'output_%s' % descriptor
    else:
      output_file_name = 'output_%d_%s' % (test_file_index, descriptor)

    if hls:
      stream.Append('playlist_name', descriptor + '.m3u8')

    if trick_play_factor:
      stream.Append('trick_play_factor', trick_play_factor)
      output_file_name += '-trick_play_factor_%d' % trick_play_factor

    if drm_label:
      stream.Append('drm_label', drm_label)

    if skip_encryption:
      stream.Append('skip_encryption', 1)
      output_file_name += '-skip_encryption'

    base_ext = GetExtension(descriptor, output_format)

    requires_init_segment = segmented and base_ext not in ['ts', 'vtt']

    output_file_path = os.path.join(self.tmp_dir, output_file_name)

    if requires_init_segment:
      init_seg = '%s-init.%s' % (output_file_path, base_ext)
      stream.Append('init_segment', init_seg)

    if segmented:
      segment_ext = GetSegmentedExtension(base_ext)
      seg_template = '%s-$Number$.%s' % (output_file_path, segment_ext)
      stream.Append('segment_template', seg_template)
    else:
      output_file_path = '%s.%s' % (output_file_path, base_ext)
      stream.Append('output', output_file_path)

    self.output.append(output_file_path)

    return str(stream)

  def _GetStreams(self, streams, test_files=None, **kwargs):
    # Make sure there is a valid list that we can get the length from.
    test_files = test_files or []
    test_files_count = len(test_files)

    out = []

    if test_files_count == 0:
      for stream in streams:
        out.append(self._GetStream(stream, **kwargs))
    elif test_files_count == 1:
      for stream in streams:
        out.append(self._GetStream(stream, test_file=test_files[0], **kwargs))
    else:
      for index, file_name in enumerate(test_files):
        for stream in streams:
          out.append(self._GetStream(
              stream, test_file_index=index, test_file=file_name, **kwargs))

    return out

  def _GetFlags(self,
                strip_parameter_set_nalus=True,
                encryption=False,
                fairplay=False,
                protection_scheme=None,
                vp9_subsample_encryption=True,
                decryption=False,
                random_iv=False,
                widevine_encryption=False,
                key_rotation=False,
                include_pssh_in_stream=True,
                dash_if_iop=True,
                output_media_info=False,
                output_hls=False,
                hls_playlist_type=None,
                time_shift_buffer_depth=0.0,
                generate_static_mpd=False,
                ad_cues=None,
                use_fake_clock=True):
    flags = []

    if not strip_parameter_set_nalus:
      flags += ['--strip_parameter_set_nalus=false']

    if widevine_encryption:
      widevine_server_url = ('https://license.uat.widevine.com/cenc'
                             '/getcontentkey/widevine_test')
      flags += [
          '--enable_widevine_encryption',
          '--key_server_url=' + widevine_server_url,
          '--content_id=' + self.widevine_content_id,
      ]
    elif encryption:
      flags += [
          '--enable_raw_key_encryption',
          '--keys=label=:key_id={0}:key={1}'.format(self.encryption_key_id,
                                                    self.encryption_key),
          '--clear_lead={0}'.format(self.clear_lead)
      ]

      if not random_iv:
        flags.append('--iv=' + self.encryption_iv)

      if fairplay:
        fairplay_pssh = ('000000207073736800000000'
                         '29701FE43CC74A348C5BAE90C7439A4700000000')
        fairplay_key_uri = ('skd://www.license.com/'
                            'getkey?KeyId=31323334-3536-3738-3930-313233343536')
        flags += [
            '--pssh=' + fairplay_pssh, '--hls_key_uri=' + fairplay_key_uri
        ]
    if protection_scheme:
      flags += ['--protection_scheme', protection_scheme]
    if not vp9_subsample_encryption:
      flags += ['--vp9_subsample_encryption=false']

    if decryption:
      flags += [
          '--enable_raw_key_decryption',
          '--keys=label=:key_id={0}:key={1}'.format(self.encryption_key_id,
                                                    self.encryption_key)
      ]

    if key_rotation:
      flags.append('--crypto_period_duration=1')

    if not include_pssh_in_stream:
      flags.append('--mp4_include_pssh_in_stream=false')

    if not dash_if_iop:
      flags.append('--generate_dash_if_iop_compliant_mpd=false')
    if output_media_info:
      flags.append('--output_media_info')
    elif output_hls:
      flags += ['--hls_master_playlist_output', self.hls_master_playlist_output]
      if hls_playlist_type:
        flags += ['--hls_playlist_type', hls_playlist_type]
      if time_shift_buffer_depth != 0.0:
        flags += [
            '--time_shift_buffer_depth={0}'.format(time_shift_buffer_depth)
        ]
    else:
      flags += ['--mpd_output', self.mpd_output]

    if generate_static_mpd:
      flags += ['--generate_static_mpd']

    if ad_cues:
      flags += ['--ad_cues', ad_cues]

    flags.append('--segment_duration=1')
    # Use fake clock, so output can be compared.
    if use_fake_clock:
      flags.append('--use_fake_clock_for_muxer')

    # Override packager version string for testing.
    flags += ['--override_version', '--test_version', '<tag>-<hash>-<test>']
    return flags

  def _CompareWithGold(self, test_output, golden_file_name):
    golden_file = os.path.join(self.golden_file_dir, golden_file_name)
    return filecmp.cmp(test_output, golden_file)

  def _GitDiff(self, file_a, file_b):
    cmd = [
        'git',
        '--no-pager',
        'diff',
        '--color=auto',
        '--no-ext-diff',
        '--no-index',
        file_a,
        file_b
    ]
    p = subprocess.Popen(cmd, stderr=subprocess.PIPE, stdout=subprocess.PIPE)
    return p.communicate()

  def _DiffGold(self, test_output, golden_file_name):
    golden_file = os.path.join(self.golden_file_dir, golden_file_name)
    if test_env.options.test_update_golden_files:
      if not os.path.exists(golden_file) or not filecmp.cmp(test_output,
                                                            golden_file):
        print 'Updating golden file: ', golden_file_name
        shutil.copyfile(test_output, golden_file)
    else:
      match = filecmp.cmp(test_output, golden_file)
      if not match:
        output, error = self._GitDiff(test_output, golden_file)
        command_line = self.packager.GetCommandLine()
        failure_message = '\n'.join([
            output,
            error,
            _TEST_FAILURE_COMMAND_LINE_MESSAGE,
            command_line
        ])
        self.fail(failure_message)

  # '*.media_info' outputs contain media file names, which is changing for
  # every test run. These needs to be replaced for comparison.
  def _DiffMediaInfoGold(self, test_output, golden_file_name):
    if platform.system() == 'Windows':
      test_output = test_output.replace('\\', '\\\\')
    media_info_output = test_output + '.media_info'
    # Replaces file name, which is changing for every test run.
    with open(media_info_output, 'rb') as f:
      content = f.read()
    with open(media_info_output, 'wb') as f:
      f.write(content.replace(test_output, 'place_holder'))
    self._DiffGold(media_info_output, golden_file_name + '.media_info')

  # TODO(vaage): Replace all used of this with |_CheckTestResults|.
  def _DiffLiveGold(self,
                    test_output_prefix,
                    golden_file_name_prefix,
                    output_format='mp4'):
    # Compare init and the first three segments.
    if output_format == 'ts':
      for i in range(1, 4):
        self._DiffGold('%s-%d.ts' % (test_output_prefix, i),
                       '%s-%d.ts' % (golden_file_name_prefix, i))
    else:
      self._DiffGold(test_output_prefix + '-init.mp4',
                     golden_file_name_prefix + '-init.mp4')
      for i in range(1, 4):
        self._DiffGold('%s-%d.m4s' % (test_output_prefix, i),
                       '%s-%d.m4s' % (golden_file_name_prefix, i))

  # |test_dir| is expected to be relative to |self.golden_file_dir|.
  def _CheckTestResults(self, test_dir):
    # Live mpd contains current availabilityStartTime and publishTime, which
    # needs to be replaced before comparison. If this is not a live test, then
    # this will be a no-op.
    mpds = glob.glob(os.path.join(self.tmp_dir, '*.mpd'))
    for manifest in mpds:
      _UpdateMpdTimes(manifest)

    if test_env.options.test_update_golden_files:
      self._UpdateGold(test_dir)
    else:
      self._DiffDir(test_dir)

  # |test_dir| is expected to be relative to |self.golden_file_dir|.
  def _UpdateGold(self, test_dir):
    out_dir = self.tmp_dir
    gold_dir = os.path.join(self.golden_file_dir, test_dir)

    if os.path.exists(gold_dir):
      shutil.rmtree(gold_dir)

    shutil.copytree(out_dir, gold_dir)

  # |test_dir| is expected to be relative to |self.golden_file_dir|.
  def _DiffDir(self, test_dir):
    out_dir = self.tmp_dir
    gold_dir = os.path.join(self.golden_file_dir, test_dir)

    # Get a list of the files and dirs that are different between the two top
    # level directories.
    diff = filecmp.dircmp(out_dir, gold_dir)

    # Create a list of all the details about the failure. The list will be
    # joined together when sent out.
    failure_messages = []

    missing = diff.left_only
    if missing:
      failure_messages += [
          'Missing %d files: %s' % (len(missing), str(missing))
      ]

    extra = diff.right_only
    if extra:
      failure_messages += [
          'Found %d unexpected files: %s' % (len(extra), str(extra))
      ]

    # Produce nice diffs for each file that differs.
    for diff_file in diff.diff_files:
      actual_file = os.path.join(out_dir, diff_file)
      expected_file = os.path.join(gold_dir, diff_file)

      output, error = self._GitDiff(actual_file, expected_file)

      if output:
        failure_messages += [output]

      if error:
        failure_messages += [error]

    if failure_messages:
      # Prepend the failure messages with the header.
      failure_messages = [
          _TEST_FAILURE_COMMAND_LINE_MESSAGE,
          self.packager.GetCommandLine()
      ] + failure_messages

      self.fail('\n'.join(failure_messages))


class PackagerFunctionalTest(PackagerAppTest):

  def assertPackageSuccess(self, streams, flags=None):
    self.assertEqual(self.packager.Package(streams, flags), 0)

  def testVersion(self):
    self.assertRegexpMatches(
        self.packager.Version(), '^packager(.exe)? version '
        r'((?P<tag>[\w\.]+)-)?(?P<hash>[a-f\d]+)-(debug|release)[\r\n]+.*$')

  def testDumpStreamInfo(self):
    test_file = os.path.join(self.test_data_dir, 'bear-640x360.mp4')
    stream_info = self.packager.DumpStreamInfo(test_file)
    expected_stream_info = ('Found 2 stream(s).\n'
                            'Stream [0] type: Video\n'
                            ' codec_string: avc1.64001e\n'
                            ' time_scale: 30000\n'
                            ' duration: 82082 (2.7 seconds)\n'
                            ' is_encrypted: false\n'
                            ' codec: H264\n'
                            ' width: 640\n'
                            ' height: 360\n'
                            ' pixel_aspect_ratio: 1:1\n'
                            ' trick_play_factor: 0\n'
                            ' nalu_length_size: 4\n\n'
                            'Stream [1] type: Audio\n'
                            ' codec_string: mp4a.40.2\n'
                            ' time_scale: 44100\n'
                            ' duration: 121856 (2.8 seconds)\n'
                            ' is_encrypted: false\n'
                            ' codec: AAC\n'
                            ' sample_bits: 16\n'
                            ' num_channels: 2\n'
                            ' sampling_frequency: 44100\n'
                            ' language: und\n')
    stream_info = stream_info.replace('\r\n', '\n')
    self.assertIn(expected_stream_info, stream_info,
                  '\nExpecting: \n %s\n\nBut seeing: \n%s' %
                  (expected_stream_info, stream_info))

  def testPackageFirstStream(self):
    self.assertPackageSuccess(self._GetStreams(['0']), self._GetFlags())
    self._CheckTestResults('first-stream')

  def testPackageText(self):
    self.assertPackageSuccess(
        self._GetStreams(['text'], test_files=['subtitle-english.vtt']),
        self._GetFlags())
    self._CheckTestResults('text')

  # Probably one of the most common scenarios is to package audio and video.
  def testPackageAudioVideo(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video']), self._GetFlags())
    self._CheckTestResults('audio-video')

  def testPackageAudioVideoWithTrickPlay(self):
    streams = [
        self._GetStream('audio'),
        self._GetStream('video'),
        self._GetStream('video', trick_play_factor=1),
    ]

    self.assertPackageSuccess(streams, self._GetFlags())
    self._CheckTestResults('audio-video-with-trick-play')

  def testPackageAudioVideoWithTwoTrickPlay(self):
    streams = [
        self._GetStream('audio'),
        self._GetStream('video'),
        self._GetStream('video', trick_play_factor=1),
        self._GetStream('video', trick_play_factor=2),
    ]

    self.assertPackageSuccess(streams, self._GetFlags())
    self._CheckTestResults('audio-video-with-two-trick-play')

  def testPackageAudioVideoWithTwoTrickPlayDecreasingRate(self):
    streams = [
        self._GetStream('audio'),
        self._GetStream('video'),
        self._GetStream('video', trick_play_factor=2),
        self._GetStream('video', trick_play_factor=1),
    ]

    self.assertPackageSuccess(streams, self._GetFlags())
    # Since the stream descriptors are sorted in packager app, a different
    # order of trick play factors gets the same mpd.
    self._CheckTestResults('audio-video-with-two-trick-play')

  def testPackageAudioVideoWithLanguageOverride(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], language='por-BR'),
        self._GetFlags())
    self._CheckTestResults('audio-video-with-language-override')

  def testPackageAudioVideoWithLanguageOverrideWithSubtag(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], language='por-BR'),
        self._GetFlags())
    self._CheckTestResults('audio-video-with-language-override-with-subtag')

  def testPackageAacHe(self):
    self.assertPackageSuccess(
        self._GetStreams(
            ['audio'], test_files=['bear-640x360-aac_he-silent_right.mp4']),
        self._GetFlags())
    self._CheckTestResults('acc-he')

  # Package all video, audio, and text.
  def testPackageVideoAudioText(self):
    audio_video_streams = self._GetStreams(['audio', 'video'])
    text_stream = self._GetStreams(['text'],
                                   test_files=['subtitle-english.vtt'])
    self.assertPackageSuccess(audio_video_streams + text_stream,
                              self._GetFlags())
    self._CheckTestResults('video-audio-text')

  def testPackageAvcAacTs(self):
    # Currently we only support live packaging for ts.
    self.assertPackageSuccess(
        self._GetStreams(
            ['audio', 'video'],
            output_format='ts',
            segmented=True,
            hls=True,
            test_files=['bear-640x360.ts']),
        self._GetFlags(output_hls=True))
    self._CheckTestResults('avc-aac-ts')

  def testPackageAvcAc3Ts(self):
    # Currently we only support live packaging for ts.
    self.assertPackageSuccess(
        self._GetStreams(
            ['audio', 'video'],
            output_format='ts',
            segmented=True,
            hls=True,
            test_files=['bear-640x360-ac3.ts']),
        self._GetFlags(output_hls=True))
    self._CheckTestResults('avc-ac3-ts')

  def testPackageAvcAc3TsToMp4(self):
    self.assertPackageSuccess(
        self._GetStreams(
            ['audio', 'video'], hls=True, test_files=['bear-640x360-ac3.ts']),
        self._GetFlags(output_hls=True))
    self._CheckTestResults('avc-ac3-ts-to-mp4')

  def testPackageAvcTsLivePlaylist(self):
    self.assertPackageSuccess(
        self._GetStreams(
            ['audio', 'video'],
            output_format='ts',
            segmented=True,
            hls=True,
            test_files=['bear-640x360.ts']),
        self._GetFlags(
            output_hls=True,
            hls_playlist_type='LIVE',
            time_shift_buffer_depth=0.5))
    self._CheckTestResults('avc-ts-live-playlist')

  def testPackageAvcTsLivePlaylistWithKeyRotation(self):
    self.packager.Package(
        self._GetStreams(
            ['audio', 'video'],
            output_format='ts',
            segmented=True,
            hls=True,
            test_files=['bear-640x360.ts']),
        self._GetFlags(
            encryption=True,
            key_rotation=True,
            output_hls=True,
            hls_playlist_type='LIVE',
            time_shift_buffer_depth=0.5))
    self._CheckTestResults('avc-ts-live-playlist-with-key-rotation')

  def testPackageAvcTsEventPlaylist(self):
    self.assertPackageSuccess(
        self._GetStreams(
            ['audio', 'video'],
            output_format='ts',
            segmented=True,
            hls=True,
            test_files=['bear-640x360.ts']),
        self._GetFlags(
            output_hls=True,
            hls_playlist_type='EVENT',
            time_shift_buffer_depth=0.5))
    self._CheckTestResults('avc-ts-event-playlist')

  def testPackageVp8Webm(self):
    self.assertPackageSuccess(
        self._GetStreams(['video'],
                         output_format='webm',
                         test_files=['bear-640x360.webm']),
        self._GetFlags())
    self._CheckTestResults('vp8-webm')

  def testPackageVp9Webm(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'],
                         output_format='webm',
                         test_files=['bear-320x240-vp9-opus.webm']),
        self._GetFlags())
    self._CheckTestResults('vp9-webm')

  def testPackageVp9WebmWithBlockgroup(self):
    self.assertPackageSuccess(
        self._GetStreams(['video'],
                         output_format='webm',
                         test_files=['bear-vp9-blockgroup.webm']),
        self._GetFlags())
    self._CheckTestResults('vp9-webm-with-blockgroup')

  def testPackageVorbisWebm(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio'],
                         output_format='webm',
                         test_files=['bear-320x240-audio-only.webm']),
        self._GetFlags())
    self._CheckTestResults('vorbis-webm')

  def testPackageEncryption(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True))
    self._DiffGold(self.output[0], 'bear-640x360-a-cenc-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-cenc-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')

  # Test deprecated flag --enable_fixed_key_encryption, which is still
  # supported currently.
  def testPackageEncryptionUsingFixedKey(self):
    flags = self._GetFlags() + [
        '--enable_fixed_key_encryption', '--key_id={0}'.format(
            self.encryption_key_id), '--key={0}'.format(self.encryption_key),
        '--clear_lead={0}'.format(self.clear_lead), '--iv={0}'.format(
            self.encryption_iv)
    ]
    self.assertPackageSuccess(self._GetStreams(['audio', 'video']), flags)
    self._DiffGold(self.output[0], 'bear-640x360-a-cenc-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-cenc-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')

  def testPackageEncryptionMultiKeys(self):
    audio_key_id = '10111213141516171819202122232425'
    audio_key = '11121314151617181920212223242526'
    video_key_id = '20212223242526272829303132333435'
    video_key = '21222324252627282930313233343536'
    flags = self._GetFlags() + [
        '--enable_raw_key_encryption',
        '--keys=label=AUDIO:key_id={0}:key={1},label=SD:key_id={2}:key={3}'.
        format(audio_key_id, audio_key,
               video_key_id, video_key), '--clear_lead={0}'.format(
                   self.clear_lead), '--iv={0}'.format(self.encryption_iv)
    ]
    self.assertPackageSuccess(self._GetStreams(['audio', 'video']), flags)

    self.encryption_key_id = audio_key_id
    self.encryption_key = audio_key
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self.encryption_key_id = video_key_id
    self.encryption_key = video_key
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')

  def testPackageEncryptionMultiKeysWithStreamLabel(self):
    audio_key_id = '20212223242526272829303132333435'
    audio_key = '21222324252627282930313233343536'
    video_key_id = '10111213141516171819202122232425'
    video_key = '11121314151617181920212223242526'
    flags = self._GetFlags() + [
        '--enable_raw_key_encryption',
        '--keys=label=MyAudio:key_id={0}:key={1},label=:key_id={2}:key={3}'.
        format(audio_key_id, audio_key,
               video_key_id, video_key), '--clear_lead={0}'.format(
                   self.clear_lead), '--iv={0}'.format(self.encryption_iv)
    ]
    # DRM label 'MyVideo' is not defined, will fall back to the key for the
    # empty default label.
    streams = [
        self._GetStream('audio', drm_label='MyAudio'),
        self._GetStream('video', drm_label='MyVideo')
    ]

    self.assertPackageSuccess(streams, flags)

    self.encryption_key_id = audio_key_id
    self.encryption_key = audio_key
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self.encryption_key_id = video_key_id
    self.encryption_key = video_key
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')

  def testPackageEncryptionOfOnlyVideoStream(self):
    streams = [
        self._GetStream('audio', skip_encryption=True),
        self._GetStream('video')
    ]
    flags = self._GetFlags(encryption=True)

    self.assertPackageSuccess(streams, flags)

    self._DiffGold(self.output[0], 'bear-640x360-a-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-a-clear-v-cenc-golden.mpd')
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')

  def testPackageEncryptionAndTrickPlay(self):
    streams = [
        self._GetStream('audio'),
        self._GetStream('video'),
        self._GetStream('video', trick_play_factor=1),
    ]

    self.assertPackageSuccess(streams, self._GetFlags(encryption=True))
    self._DiffGold(self.output[0], 'bear-640x360-a-cenc-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')
    self._DiffGold(self.output[2], 'bear-640x360-v-trick-1-cenc-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-trick-1-cenc-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')
    self._VerifyDecryption(self.output[2], 'bear-640x360-v-trick-1-golden.mp4')

  # TODO(hmchen): Add a test case that SD and HD AdapatationSet share one trick
  # play stream.
  def testPackageEncryptionAndTwoTrickPlays(self):
    streams = [
        self._GetStream('audio'),
        self._GetStream('video'),
        self._GetStream('video', trick_play_factor=1),
        self._GetStream('video', trick_play_factor=2),
    ]

    self.assertPackageSuccess(streams, self._GetFlags(encryption=True))
    self._DiffGold(self.output[0], 'bear-640x360-a-cenc-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')
    self._DiffGold(self.output[2], 'bear-640x360-v-trick-1-cenc-golden.mp4')
    self._DiffGold(self.output[3], 'bear-640x360-v-trick-2-cenc-golden.mp4')
    self._DiffGold(self.mpd_output,
                   'bear-640x360-av-trick-1-trick-2-cenc-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')
    self._VerifyDecryption(self.output[2], 'bear-640x360-v-trick-1-golden.mp4')
    self._VerifyDecryption(self.output[3], 'bear-640x360-v-trick-2-golden.mp4')

  def testPackageEncryptionAndNoClearLead(self):
    self.clear_lead = 0
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video']), self._GetFlags(encryption=True))
    self._DiffGold(self.output[0],
                   'bear-640x360-a-cenc-no-clear-lead-golden.mp4')
    self._DiffGold(self.output[1],
                   'bear-640x360-v-cenc-no-clear-lead-golden.mp4')
    self._DiffGold(self.mpd_output,
                   'bear-640x360-av-cenc-no-clear-lead-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')

  def testPackageEncryptionAndNoPsshInStream(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True, include_pssh_in_stream=False))
    self._DiffGold(self.output[0], 'bear-640x360-a-cenc-no-pssh-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cenc-no-pssh-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-cenc-no-pssh-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')

  def testPackageEncryptionCbc1(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True,
                       protection_scheme='cbc1'))
    self._DiffGold(self.output[0], 'bear-640x360-a-cbc1-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cbc1-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-cbc1-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')

  def testPackageEncryptionCens(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True,
                       protection_scheme='cens'))
    self._DiffGold(self.output[0], 'bear-640x360-a-cens-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cens-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-cens-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')

  def testPackageEncryptionCbcs(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True,
                       protection_scheme='cbcs'))
    self._DiffGold(self.output[0], 'bear-640x360-a-cbcs-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cbcs-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-cbcs-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')

  def testPackageEncryptionAndAdCues(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True, ad_cues='1.5'))
    self._DiffGold(self.output[0], 'bear-640x360-a-cenc-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-cenc-ad_cues-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')

  def testPackageWebmSubsampleEncryption(self):
    self.assertPackageSuccess(
        self._GetStreams(['video'],
                         output_format='webm',
                         test_files=['bear-320x180-vp9-altref.webm']),
        self._GetFlags(encryption=True))
    self._DiffGold(self.output[0], 'bear-320x180-vp9-altref-enc-golden.webm')
    self._VerifyDecryption(self.output[0],
                           'bear-320x180-vp9-altref-dec-golden.webm')

  def testPackageWebmVp9FullSampleEncryption(self):
    self.assertPackageSuccess(
        self._GetStreams(['video'],
                         output_format='webm',
                         test_files=['bear-320x180-vp9-altref.webm']),
        self._GetFlags(encryption=True, vp9_subsample_encryption=False))
    self._DiffGold(self.output[0],
                   'bear-320x180-vp9-fullsample-enc-golden.webm')
    self._VerifyDecryption(self.output[0],
                           'bear-320x180-vp9-altref-dec-golden.webm')

  def testPackageAvcTsWithEncryption(self):
    # Currently we only support live packaging for ts.
    self.assertPackageSuccess(
        self._GetStreams(
            ['audio', 'video'],
            output_format='ts',
            segmented=True,
            hls=True,
            test_files=['bear-640x360.ts']),
        self._GetFlags(encryption=True, output_hls=True))
    self._CheckTestResults('avc-ts-with-encryption')

  def testPackageAvcTsWithEncryptionAndFairplay(self):
    # Currently we only support live packaging for ts.
    self.assertPackageSuccess(
        self._GetStreams(
            ['audio', 'video'],
            output_format='ts',
            segmented=True,
            hls=True,
            test_files=['bear-640x360.ts']),
        self._GetFlags(encryption=True, output_hls=True, fairplay=True))
    self._CheckTestResults('avc-ts-with-encryption-and-fairplay')

  def testPackageAvcAc3TsWithEncryption(self):
    # Currently we only support live packaging for ts.
    self.assertPackageSuccess(
        self._GetStreams(
            ['audio', 'video'],
            output_format='ts',
            segmented=True,
            hls=True,
            test_files=['bear-640x360-ac3.ts']),
        self._GetFlags(encryption=True, output_hls=True))
    self._CheckTestResults('avc-ac3-ts-with-encryption')

  def testPackageAvcTsWithEncryptionExerciseEmulationPrevention(self):
    self.encryption_key = 'ad7e9786def9159db6724be06dfcde7a'
    # Currently we only support live packaging for ts.
    self.assertPackageSuccess(
        self._GetStreams(
            ['video'],
            output_format='ts',
            segmented=True,
            hls=True,
            test_files=['sintel-1024x436.mp4']),
        self._GetFlags(
            encryption=True,
            output_hls=True))
    self._CheckTestResults(
        'avc-ts-with-encryption-exercise-emulation-prevention')

  def testPackageWebmWithEncryption(self):
    self.assertPackageSuccess(
        self._GetStreams(['video'],
                         output_format='webm',
                         test_files=['bear-640x360.webm']),
        self._GetFlags(encryption=True))
    self._DiffGold(self.output[0], 'bear-640x360-vp8-cenc-golden.webm')
    self._DiffGold(self.mpd_output, 'bear-640x360-vp8-cenc-webm-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-vp8-golden.webm')

  def testPackageHevcWithEncryption(self):
    self.assertPackageSuccess(
        self._GetStreams(['video'],
                         test_files=['bear-640x360-hevc.mp4']),
        self._GetFlags(encryption=True))
    self._DiffGold(self.output[0], 'bear-640x360-hevc-cenc-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-hevc-cenc-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-hevc-golden.mp4')

  def testPackageVp8Mp4WithEncryption(self):
    self.assertPackageSuccess(
        self._GetStreams(['video'],
                         output_format='mp4',
                         test_files=['bear-640x360.webm']),
        self._GetFlags(encryption=True))
    self._DiffGold(self.output[0], 'bear-640x360-vp8-cenc-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-vp8-cenc-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-vp8-golden.mp4')

  def testPackageOpusVp9Mp4WithEncryption(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'],
                         output_format='mp4',
                         test_files=['bear-320x240-vp9-opus.webm']),
        self._GetFlags(encryption=True))
    self._DiffGold(self.output[0], 'bear-320x240-opus-cenc-golden.mp4')
    self._DiffGold(self.output[1], 'bear-320x240-vp9-cenc-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-320x240-opus-vp9-cenc-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-320x240-opus-golden.mp4')
    self._VerifyDecryption(self.output[1], 'bear-320x240-vp9-golden.mp4')

  def testPackageWvmInput(self):
    self.encryption_key = '9248d245390e0a49d483ba9b43fc69c3'
    self.assertPackageSuccess(
        self._GetStreams(
            ['0', '1', '2', '3'], test_files=['bear-multi-configs.wvm']),
        self._GetFlags(decryption=True))
    # Output timescale is 90000.
    self._CheckTestResults('wvm-input')

  # TODO(kqyang): Fix shared_library not supporting strip_parameter_set_nalus
  # problem.
  @unittest.skipUnless(
      test_env.options.libpackager_type == 'static_library',
      'libpackager shared_library does not support '
      '--strip_parameter_set_nalus flag.'
  )
  def testPackageWvmInputWithoutStrippingParameterSetNalus(self):
    self.encryption_key = '9248d245390e0a49d483ba9b43fc69c3'
    self.assertPackageSuccess(
        self._GetStreams(
            ['0', '1', '2', '3'], test_files=['bear-multi-configs.wvm']),
        self._GetFlags(strip_parameter_set_nalus=False, decryption=True))
    # Output timescale is 90000.
    self._CheckTestResults('wvm-input-without-stripping-parameters-set-nalus')

  def testPackageEncryptionAndRandomIv(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True, random_iv=True))
    self._AssertStreamInfo(self.output[0], 'is_encrypted: true')
    self._AssertStreamInfo(self.output[1], 'is_encrypted: true')
    # The outputs are encrypted with random iv, so they are not the same as
    # golden files.
    self.assertFalse(self._CompareWithGold(self.output[0],
                                           'bear-640x360-a-cenc-golden.mp4'))
    self.assertFalse(self._CompareWithGold(self.output[1],
                                           'bear-640x360-v-cenc-golden.mp4'))
    self._DiffGold(self.mpd_output, 'bear-640x360-av-cenc-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')

  def testPackageEncryptionAndRealClock(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True, use_fake_clock=False))
    self._AssertStreamInfo(self.output[0], 'is_encrypted: true')
    self._AssertStreamInfo(self.output[1], 'is_encrypted: true')
    # The outputs are generated with real clock, so they are not the same as
    # golden files.
    self.assertFalse(self._CompareWithGold(self.output[0],
                                           'bear-640x360-a-cenc-golden.mp4'))
    self.assertFalse(self._CompareWithGold(self.output[1],
                                           'bear-640x360-v-cenc-golden.mp4'))
    self._DiffGold(self.mpd_output, 'bear-640x360-av-cenc-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')

  def testPackageEncryptionAndNonDashIfIop(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True, dash_if_iop=False))
    self._DiffGold(self.output[0], 'bear-640x360-a-cenc-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-cenc-non-iop-golden.mpd')

  def testPackageEncryptionAndOutputMediaInfo(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True, output_media_info=True))
    self._DiffGold(self.output[0], 'bear-640x360-a-cenc-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')
    self._DiffMediaInfoGold(self.output[0], 'bear-640x360-a-cenc-golden.mp4')
    self._DiffMediaInfoGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')

  def testPackageHlsSingleSegmentMp4Encrypted(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], hls=True),
        self._GetFlags(encryption=True, output_hls=True))
    self._CheckTestResults('hls-single-segment-mp4-encrypted')

  def testPackageEc3AndHlsSingleSegmentMp4Encrypted(self):
    self.assertPackageSuccess(
        self._GetStreams(
            ['audio', 'video'], hls=True, test_files=['bear-640x360-ec3.mp4']),
        self._GetFlags(encryption=True, output_hls=True))
    self._CheckTestResults('ec3-and-hls-single-segment-mp4-encrypted')

  def testPackageHlsSingleSegmentMp4EncryptedAndAdCues(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], hls=True),
        self._GetFlags(encryption=True, output_hls=True, ad_cues='1.5'))
    self._CheckTestResults('hls-single-segment-mp4-encrypted-and-ad-cues')

  # Test HLS with multi-segment mp4 and content in subdirectories.
  def testPackageHlsMultiSegmentMp4WithCustomPath(self):
    test_file = os.path.join(self.test_data_dir, 'bear-640x360.mp4')
    # {tmp}/audio/audio-init.mp4, {tmp}/audio/audio-1.m4s etc.
    audio_output_prefix = os.path.join(self.tmp_dir, 'audio', 'audio')
    # {tmp}/video/video-init.mp4, {tmp}/video/video-1.m4s etc.
    video_output_prefix = os.path.join(self.tmp_dir, 'video', 'video')

    self.assertPackageSuccess(
        [
            'input=%s,stream=audio,init_segment=%s-init.mp4,'
            'segment_template=%s-$Number$.m4s,playlist_name=audio/audio.m3u8' %
            (test_file, audio_output_prefix, audio_output_prefix),
            'input=%s,stream=video,init_segment=%s-init.mp4,'
            'segment_template=%s-$Number$.m4s,playlist_name=video/video.m3u8' %
            (test_file, video_output_prefix, video_output_prefix),
        ],
        self._GetFlags(output_hls=True))

    self._CheckTestResults('hls-multi-segment-mp4-with-custom-path')

  def testPackageLiveProfile(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], segmented=True), self._GetFlags())
    self._CheckTestResults('live-profile')

  def testPackageLiveStaticProfile(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], segmented=True),
        self._GetFlags(generate_static_mpd=True))
    self._CheckTestResults('live-static-profile')

  def testPackageLiveStaticProfileAndAdCues(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], segmented=True),
        self._GetFlags(generate_static_mpd=True, ad_cues='1.5'))
    self._CheckTestResults('live-static-profile-and-ad-cues')

  def testPackageLiveProfileAndEncryption(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], segmented=True),
        self._GetFlags(encryption=True))
    self._CheckTestResults('live-profile-and-encryption')

  def testPackageLiveProfileAndEncryptionAndNonDashIfIop(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], segmented=True),
        self._GetFlags(encryption=True, dash_if_iop=False))
    self._CheckTestResults(
        'live-profile-and-encryption-and-non-dash-if-iop')

  def testPackageLiveProfileAndEncryptionAndMultFiles(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'],
                         segmented=True,
                         test_files=['bear-1280x720.mp4', 'bear-640x360.mp4',
                                     'bear-320x180.mp4']),
        self._GetFlags(encryption=True))
    self._DiffLiveGold(self.output[2], 'bear-640x360-a-live-cenc-golden')
    self._DiffLiveGold(self.output[3], 'bear-640x360-v-live-cenc-golden')
    # Mpd cannot be validated right now since we don't generate determinstic
    # mpd with multiple inputs due to thread racing.
    # TODO(b/73349711): Generate determinstic mpd or at least validate mpd
    #                   schema.

  def testPackageLiveProfileAndKeyRotation(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], segmented=True),
        self._GetFlags(encryption=True, key_rotation=True))
    self._CheckTestResults('live-profile-and-key-rotation')

  def testPackageLiveProfileAndKeyRotationAndNoPsshInStream(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], segmented=True),
        self._GetFlags(
            encryption=True, key_rotation=True, include_pssh_in_stream=False))
    self._CheckTestResults(
        'live-profile-and-key-rotation-and-no-pssh-in-stream')

  def testPackageLiveProfileAndKeyRotationAndNonDashIfIop(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], segmented=True),
        self._GetFlags(encryption=True,
                       key_rotation=True,
                       dash_if_iop=False))
    self._CheckTestResults(
        'live-profile-and-key-rotation-and-non-dash-if-iop')

  @unittest.skipUnless(test_env.has_aes_flags, 'Requires AES credentials.')
  def testWidevineEncryptionWithAes(self):
    flags = self._GetFlags(widevine_encryption=True)
    flags += [
        '--signer=widevine_test',
        '--aes_signing_key=' + test_env.options.aes_signing_key,
        '--aes_signing_iv=' + test_env.options.aes_signing_iv
    ]
    self.assertPackageSuccess(self._GetStreams(['audio', 'video']), flags)
    self._AssertStreamInfo(self.output[0], 'is_encrypted: true')
    self._AssertStreamInfo(self.output[1], 'is_encrypted: true')

  @unittest.skipUnless(test_env.has_aes_flags, 'Requires AES credentials.')
  def testWidevineEncryptionWithAesAndMultFiles(self):
    flags = self._GetFlags(widevine_encryption=True)
    flags += [
        '--signer=widevine_test',
        '--aes_signing_key=' + test_env.options.aes_signing_key,
        '--aes_signing_iv=' + test_env.options.aes_signing_iv
    ]
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'],
                         test_files=['bear-1280x720.mp4', 'bear-640x360.mp4',
                                     'bear-320x180.mp4']), flags)
    with open(self.mpd_output, 'rb') as f:
      print f.read()
      # TODO(kqyang): Add some validations.

  @unittest.skipUnless(test_env.has_aes_flags, 'Requires AES credentials.')
  def testKeyRotationWithAes(self):
    flags = self._GetFlags(widevine_encryption=True, key_rotation=True)
    flags += [
        '--signer=widevine_test',
        '--aes_signing_key=' + test_env.options.aes_signing_key,
        '--aes_signing_iv=' + test_env.options.aes_signing_iv
    ]
    self.assertPackageSuccess(self._GetStreams(['audio', 'video']), flags)
    self._AssertStreamInfo(self.output[0], 'is_encrypted: true')
    self._AssertStreamInfo(self.output[1], 'is_encrypted: true')

  @unittest.skipUnless(test_env.has_rsa_flags, 'Requires RSA credentials.')
  def testWidevineEncryptionWithRsa(self):
    flags = self._GetFlags(widevine_encryption=True)
    flags += [
        '--signer=widevine_test',
        '--rsa_signing_key_path=' + test_env.options.rsa_signing_key_path
    ]
    self.assertPackageSuccess(self._GetStreams(['audio', 'video']), flags)
    self._AssertStreamInfo(self.output[0], 'is_encrypted: true')
    self._AssertStreamInfo(self.output[1], 'is_encrypted: true')

  def testHlsSegmentedWebVtt(self):
    streams = self._GetStreams(['audio', 'video'], segmented=True)
    streams += self._GetStreams(
        ['text'], test_files=['bear-subtitle-english.vtt'], segmented=True)

    flags = self._GetFlags(output_hls=True)

    self.assertPackageSuccess(streams, flags)
    self._CheckTestResults('hls-segmented-webvtt')

  def _AssertStreamInfo(self, stream, info):
    stream_info = self.packager.DumpStreamInfo(stream)
    self.assertIn('Found 1 stream(s).', stream_info)
    self.assertIn(info, stream_info)

  def _VerifyDecryption(self, test_encrypted_file, golden_clear_file):
    output_extension = os.path.splitext(golden_clear_file)[1][1:]
    self.assertPackageSuccess(
        self._GetStreams(['0'],
                         output_format=output_extension,
                         test_files=[test_encrypted_file]),
        self._GetFlags(decryption=True))
    self._DiffGold(self.output[-1], golden_clear_file)


class PackagerCommandParsingTest(PackagerAppTest):

  def testPackageEncryptionWithIncorrectKeyIdLength1(self):
    self.encryption_key_id = self.encryption_key_id[0:-2]
    packaging_result = self.packager.Package(
        self._GetStreams(['video']), self._GetFlags(encryption=True))
    self.assertEqual(packaging_result, 1)

  def testPackageEncryptionWithIncorrectKeyIdLength2(self):
    self.encryption_key_id += '12'
    packaging_result = self.packager.Package(
        self._GetStreams(['video']), self._GetFlags(encryption=True))
    self.assertEqual(packaging_result, 1)

  def testPackageEncryptionWithInvalidKeyIdValue(self):
    self.encryption_key_id = self.encryption_key_id[0:-1] + 'g'
    packaging_result = self.packager.Package(
        self._GetStreams(['video']), self._GetFlags(encryption=True))
    self.assertEqual(packaging_result, 1)

  def testPackageEncryptionWithIncorrectKeyLength1(self):
    self.encryption_key = self.encryption_key[0:-2]
    packaging_result = self.packager.Package(
        self._GetStreams(['video']), self._GetFlags(encryption=True))
    self.assertEqual(packaging_result, 1)

  def testPackageEncryptionWithIncorrectKeyLength2(self):
    self.encryption_key += '12'
    packaging_result = self.packager.Package(
        self._GetStreams(['video']), self._GetFlags(encryption=True))
    self.assertEqual(packaging_result, 1)

  def testPackageEncryptionWithInvalidKeyValue(self):
    self.encryption_key = self.encryption_key[0:-1] + 'g'
    packaging_result = self.packager.Package(
        self._GetStreams(['video']), self._GetFlags(encryption=True))
    self.assertEqual(packaging_result, 1)

  def testPackageEncryptionWithIncorrectIvLength1(self):
    self.encryption_iv = self.encryption_iv[0:-2]
    packaging_result = self.packager.Package(
        self._GetStreams(['video']), self._GetFlags(encryption=True))
    self.assertEqual(packaging_result, 1)

  def testPackageEncryptionWithIncorrectIvLength2(self):
    self.encryption_iv += '12'
    packaging_result = self.packager.Package(
        self._GetStreams(['video']), self._GetFlags(encryption=True))
    self.assertEqual(packaging_result, 1)

  def testPackageEncryptionWithInvalidIvValue(self):
    self.encryption_iv = self.encryption_iv[0:-1] + 'g'
    packaging_result = self.packager.Package(
        self._GetStreams(['video']), self._GetFlags(encryption=True))
    self.assertEqual(packaging_result, 1)

  def testPackageEncryptionWithInvalidPsshValue1(self):
    packaging_result = self.packager.Package(
        self._GetStreams(['video']),
        self._GetFlags(encryption=True) + ['--pssh=ag'])
    self.assertEqual(packaging_result, 1)

  def testPackageEncryptionWithInvalidPsshValue2(self):
    packaging_result = self.packager.Package(
        self._GetStreams(['video']),
        self._GetFlags(encryption=True) + ['--pssh=1122'])
    self.assertEqual(packaging_result, 1)

  def testWidevineEncryptionInvalidContentId(self):
    self.widevine_content_id += 'ag'
    flags = self._GetFlags(widevine_encryption=True)
    flags += [
        '--signer=widevine_test', '--aes_signing_key=1122',
        '--aes_signing_iv=3344'
    ]
    packaging_result = self.packager.Package(
        self._GetStreams(['audio', 'video']), flags)
    self.assertEqual(packaging_result, 1)

  def testWidevineEncryptionInvalidAesSigningKey(self):
    flags = self._GetFlags(widevine_encryption=True)
    flags += [
        '--signer=widevine_test', '--aes_signing_key=11ag',
        '--aes_signing_iv=3344'
    ]
    packaging_result = self.packager.Package(
        self._GetStreams(['audio', 'video']), flags)
    self.assertEqual(packaging_result, 1)

  def testWidevineEncryptionInvalidAesSigningIv(self):
    flags = self._GetFlags(widevine_encryption=True)
    flags += [
        '--signer=widevine_test', '--aes_signing_key=1122',
        '--aes_signing_iv=33ag'
    ]
    packaging_result = self.packager.Package(
        self._GetStreams(['audio', 'video']), flags)
    self.assertEqual(packaging_result, 1)

  def testWidevineEncryptionMissingAesSigningKey(self):
    flags = self._GetFlags(widevine_encryption=True)
    flags += ['--signer=widevine_test', '--aes_signing_iv=3344']
    packaging_result = self.packager.Package(
        self._GetStreams(['audio', 'video']), flags)
    self.assertEqual(packaging_result, 1)

  def testWidevineEncryptionMissingAesSigningIv(self):
    flags = self._GetFlags(widevine_encryption=True)
    flags += ['--signer=widevine_test', '--aes_signing_key=1122']
    packaging_result = self.packager.Package(
        self._GetStreams(['audio', 'video']), flags)
    self.assertEqual(packaging_result, 1)

  def testWidevineEncryptionMissingSigner1(self):
    flags = self._GetFlags(widevine_encryption=True)
    flags += ['--aes_signing_key=1122', '--aes_signing_iv=3344']
    packaging_result = self.packager.Package(
        self._GetStreams(['audio', 'video']), flags)
    self.assertEqual(packaging_result, 1)

  def testWidevineEncryptionMissingSigner2(self):
    flags = self._GetFlags(widevine_encryption=True)
    flags += ['--rsa_signing_key_path=/tmp/test']
    packaging_result = self.packager.Package(
        self._GetStreams(['audio', 'video']), flags)
    self.assertEqual(packaging_result, 1)

  def testWidevineEncryptionSignerOnly(self):
    flags = self._GetFlags(widevine_encryption=True)
    flags += ['--signer=widevine_test']
    packaging_result = self.packager.Package(
        self._GetStreams(['audio', 'video']), flags)
    self.assertEqual(packaging_result, 1)

  def testWidevineEncryptionAesSigningAndRsaSigning(self):
    flags = self._GetFlags(widevine_encryption=True)
    flags += [
        '--signer=widevine_test',
        '--aes_signing_key=1122',
        '--aes_signing_iv=3344',
        '--rsa_signing_key_path=/tmp/test',
    ]
    packaging_result = self.packager.Package(
        self._GetStreams(['audio', 'video']), flags)
    self.assertEqual(packaging_result, 1)

  def testPackageAudioVideoWithNotExistText(self):
    audio_video_stream = self._GetStreams(['audio', 'video'])
    text_stream = self._GetStreams(['text'], test_files=['not-exist.vtt'])
    packaging_result = self.packager.Package(audio_video_stream + text_stream,
                                             self._GetFlags())
    # Expect the test to fail but we do not expect a crash.
    self.assertEqual(packaging_result, 1)


if __name__ == '__main__':
  unittest.main()
