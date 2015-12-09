#!/usr/bin/python
#
# Copyright 2014 Google Inc. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd
"""Tests utilizing the sample packager binary."""

import filecmp
import os
import re
import shutil
import subprocess
import tempfile
import unittest

import packager_app
import test_env


class PackagerAppTest(unittest.TestCase):

  def setUp(self):
    self.packager = packager_app.PackagerApp()
    self.tmp_dir = tempfile.mkdtemp()
    self.test_data_dir = os.path.join(test_env.SRC_DIR, 'packager', 'media',
                                      'test', 'data')
    self.golden_file_dir = os.path.join(test_env.SRC_DIR, 'packager', 'app',
                                        'test', 'testdata')
    self.output_prefix = os.path.join(self.tmp_dir, 'output')
    self.mpd_output = self.output_prefix + '.mpd'
    self.output = None

  def tearDown(self):
    shutil.rmtree(self.tmp_dir)

  def testBuildingCode(self):
    self.assertEqual(0, self.packager.BuildSrc())

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
                            ' trick_play_rate: 0\n'
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
    self.assertIn(expected_stream_info, stream_info,
                  '\nExpecting: \n %s\n\nBut seeing: \n%s' % (
                      expected_stream_info, stream_info))

  def testPackageFirstStream(self):
    self.packager.Package(self._GetStreams(['0']), self._GetFlags())
    self._DiffGold(self.output[0], 'bear-640x360-v-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-v-golden.mpd')

  def testPackageText(self):
    self.packager.Package(
        self._GetStreams(['text'],
                         test_files=['subtitle-english.vtt']),
        self._GetFlags())
    self._DiffGold(self.output[0], 'subtitle-english-golden.vtt')
    self._DiffGold(self.mpd_output, 'subtitle-english-vtt-golden.mpd')

  # Probably one of the most common scenarios is to package audio and video.
  def testPackageAudioVideo(self):
    self.packager.Package(
        self._GetStreams(['audio', 'video']), self._GetFlags())
    self._DiffGold(self.output[0], 'bear-640x360-a-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-golden.mpd')

  # Package all video, audio, and text.
  def testPackageVideoAudioText(self):
    audio_video_streams = self._GetStreams(['audio', 'video'])
    text_stream = self._GetStreams(['text'],
                                   test_files=['subtitle-english.vtt'])
    self.packager.Package(audio_video_streams + text_stream, self._GetFlags())
    self._DiffGold(self.output[0], 'bear-640x360-a-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-golden.mp4')
    self._DiffGold(self.output[2], 'subtitle-english-golden.vtt')
    self._DiffGold(self.mpd_output, 'bear-640x360-avt-golden.mpd')

  def testPackageWithEncryption(self):
    self.packager.Package(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True))
    self._DiffGold(self.output[0], 'bear-640x360-a-cenc-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-cenc-golden.mpd')

  def testPackageHevcWithEncryption(self):
    self.packager.Package(
        self._GetStreams(['video'],
                         test_files=['bear-640x360-hevc.mp4']),
        self._GetFlags(encryption=True))
    self._DiffGold(self.output[0], 'bear-640x360-hevc-v-cenc-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-hevc-v-cenc-golden.mpd')

  def testPackageWithEncryptionAndRandomIv(self):
    self.packager.Package(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True,
                       random_iv=True))
    self._AssertStreamInfo(self.output[0], 'is_encrypted: true')
    self._AssertStreamInfo(self.output[1], 'is_encrypted: true')
    # The outputs are encrypted with random iv, so they are not the same as
    # golden files.
    self.assertFalse(self._CompareWithGold(self.output[0],
                                           'bear-640x360-a-cenc-golden.mp4'))
    self.assertFalse(self._CompareWithGold(self.output[1],
                                           'bear-640x360-v-cenc-golden.mp4'))
    self._DiffGold(self.mpd_output, 'bear-640x360-av-cenc-golden.mpd')

  def testPackageWithEncryptionAndRealClock(self):
    self.packager.Package(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True,
                       use_fake_clock=False))
    self._AssertStreamInfo(self.output[0], 'is_encrypted: true')
    self._AssertStreamInfo(self.output[1], 'is_encrypted: true')
    # The outputs are generated with real clock, so they are not the same as
    # golden files.
    self.assertFalse(self._CompareWithGold(self.output[0],
                                           'bear-640x360-a-cenc-golden.mp4'))
    self.assertFalse(self._CompareWithGold(self.output[1],
                                           'bear-640x360-v-cenc-golden.mp4'))
    self._DiffGold(self.mpd_output, 'bear-640x360-av-cenc-golden.mpd')

  def testPackageWithEncryptionAndDashIfIop(self):
    self.packager.Package(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True,
                       dash_if_iop=True))
    self._DiffGold(self.output[0], 'bear-640x360-a-cenc-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-cenc-iop-golden.mpd')

  def testPackageWithEncryptionAndOutputMediaInfo(self):
    self.packager.Package(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True,
                       output_media_info=True))
    self._DiffGold(self.output[0], 'bear-640x360-a-cenc-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')
    self._DiffMediaInfoGold(self.output[0], 'bear-640x360-a-cenc-golden.mp4')
    self._DiffMediaInfoGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')

  def testPackageWithLiveProfile(self):
    self.packager.Package(
        self._GetStreams(
            ['audio', 'video'],
            live=True),
        self._GetFlags(live=True))
    self._DiffLiveGold(self.output[0], 'bear-640x360-a-live-golden')
    self._DiffLiveGold(self.output[1], 'bear-640x360-v-live-golden')
    self._DiffLiveMpdGold(self.mpd_output, 'bear-640x360-av-live-golden.mpd')

  def testPackageWithLiveProfileAndEncryption(self):
    self.packager.Package(
        self._GetStreams(
            ['audio', 'video'],
            live=True),
        self._GetFlags(encryption=True,
                       live=True))
    self._DiffLiveGold(self.output[0], 'bear-640x360-a-live-cenc-golden')
    self._DiffLiveGold(self.output[1], 'bear-640x360-v-live-cenc-golden')
    self._DiffLiveMpdGold(self.mpd_output,
                          'bear-640x360-av-live-cenc-golden.mpd')

  def testPackageWithLiveProfileAndEncryptionAndDashIfIop(self):
    self.packager.Package(
        self._GetStreams(
            ['audio', 'video'],
            live=True),
        self._GetFlags(encryption=True,
                       live=True,
                       dash_if_iop=True))
    self._DiffLiveGold(self.output[0], 'bear-640x360-a-live-cenc-golden')
    self._DiffLiveGold(self.output[1], 'bear-640x360-v-live-cenc-golden')
    self._DiffLiveMpdGold(self.mpd_output,
                          'bear-640x360-av-live-cenc-iop-golden.mpd')

  def testPackageWithLiveProfileAndEncryptionAndDashIfIopWithMultFiles(self):
    self.packager.Package(
        self._GetStreams(
            ['audio', 'video'],
            live=True,
            test_files=['bear-1280x720.mp4', 'bear-640x360.mp4',
                        'bear-320x180.mp4']),
        self._GetFlags(encryption=True,
                       live=True,
                       dash_if_iop=True))
    self._DiffLiveGold(self.output[2], 'bear-640x360-a-live-cenc-golden')
    self._DiffLiveGold(self.output[3], 'bear-640x360-v-live-cenc-golden')
    # Mpd cannot be validated right now since we don't generate determinstic
    # mpd with multiple inputs due to thread racing.
    # TODO(kqyang): Generate determinstic mpd or at least validate mpd schema.

  def testPackageWithLiveProfileAndKeyRotation(self):
    self.packager.Package(
        self._GetStreams(
            ['audio', 'video'],
            live=True),
        self._GetFlags(encryption=True,
                       key_rotation=True,
                       live=True))
    self._DiffLiveGold(self.output[0],
                       'bear-640x360-a-live-cenc-rotation-golden')
    self._DiffLiveGold(self.output[1],
                       'bear-640x360-v-live-cenc-rotation-golden')
    self._DiffLiveMpdGold(self.mpd_output,
                          'bear-640x360-av-live-cenc-rotation-golden.mpd')

  def testPackageWithLiveProfileAndKeyRotationAndDashIfIop(self):
    self.packager.Package(
        self._GetStreams(
            ['audio', 'video'],
            live=True),
        self._GetFlags(encryption=True,
                       key_rotation=True,
                       live=True,
                       dash_if_iop=True))
    self._DiffLiveGold(self.output[0],
                       'bear-640x360-a-live-cenc-rotation-golden')
    self._DiffLiveGold(self.output[1],
                       'bear-640x360-v-live-cenc-rotation-golden')
    self._DiffLiveMpdGold(self.mpd_output,
                          'bear-640x360-av-live-cenc-rotation-iop-golden.mpd')

  @unittest.skipUnless(test_env.has_aes_flags, 'Requires AES credentials.')
  def testWidevineEncryptionWithAes(self):
    flags = self._GetFlags(widevine_encryption=True)
    flags += ['--aes_signing_key=' + test_env.options.aes_signing_key,
              '--aes_signing_iv=' + test_env.options.aes_signing_iv]
    self.packager.Package(self._GetStreams(['audio', 'video']), flags)
    self._AssertStreamInfo(self.output[0], 'is_encrypted: true')
    self._AssertStreamInfo(self.output[1], 'is_encrypted: true')

  @unittest.skipUnless(test_env.has_aes_flags, 'Requires AES credentials.')
  def testKeyRotationWithAes(self):
    flags = self._GetFlags(widevine_encryption=True, key_rotation=True)
    flags += ['--aes_signing_key=' + test_env.options.aes_signing_key,
              '--aes_signing_iv=' + test_env.options.aes_signing_iv]
    self.packager.Package(self._GetStreams(['audio', 'video']), flags)
    self._AssertStreamInfo(self.output[0], 'is_encrypted: true')
    self._AssertStreamInfo(self.output[1], 'is_encrypted: true')

  @unittest.skipUnless(test_env.has_rsa_flags, 'Requires RSA credentials.')
  def testWidevineEncryptionWithRsa(self):
    flags = self._GetFlags(widevine_encryption=True)
    flags += ['--rsa_signing_key_path=' + test_env.options.rsa_signing_key_path]
    self.packager.Package(self._GetStreams(['audio', 'video']), flags)
    self._AssertStreamInfo(self.output[0], 'is_encrypted: true')
    self._AssertStreamInfo(self.output[1], 'is_encrypted: true')

  def _GetStreams(self, stream_descriptors, live=False, test_files=None):
    if test_files is None:
      test_files = ['bear-640x360.mp4']
    streams = []
    if not self.output:
      self.output = []

    for test_file_index, test_file_name in enumerate(test_files):
      test_file = os.path.join(self.test_data_dir, test_file_name)
      for stream_descriptor in stream_descriptors:
        if len(test_files) == 1:
          output_prefix = '%s_%s' % (self.output_prefix, stream_descriptor)
        else:
          output_prefix = '%s_%d_%s' % (self.output_prefix, test_file_index,
                                        stream_descriptor)
        if live:
          stream = ('input=%s,stream=%s,init_segment=%s-init.mp4,'
                    'segment_template=%s-$Number$.m4s')
          streams.append(stream % (test_file, stream_descriptor, output_prefix,
                                   output_prefix))
          self.output.append(output_prefix)
        else:
          output = '%s.%s' % (output_prefix,
                              self._GetExtension(stream_descriptor))
          stream = 'input=%s,stream=%s,output=%s'
          streams.append(stream % (test_file, stream_descriptor, output))
          self.output.append(output)
    return streams

  def _GetExtension(self, stream_descriptor):
    # TODO(rkuroiwa): Support ttml.
    if stream_descriptor == 'text':
      return 'vtt'
    return 'mp4'

  def _GetFlags(self,
                encryption=False,
                random_iv=False,
                widevine_encryption=False,
                key_rotation=False,
                live=False,
                dash_if_iop=False,
                output_media_info=False,
                use_fake_clock=True):
    flags = []
    if widevine_encryption:
      widevine_server_url = ('https://license.uat.widevine.com/cenc'
                             '/getcontentkey/widevine_test')
      flags += ['--enable_widevine_encryption',
                '--key_server_url=' + widevine_server_url,
                '--content_id=3031323334353637', '--signer=widevine_test']
    elif encryption:
      flags += ['--enable_fixed_key_encryption',
                '--key_id=31323334353637383930313233343536',
                '--key=32333435363738393021323334353637',
                '--pssh=31323334353637383930313233343536', '--clear_lead=1']
      if not random_iv:
        flags.append('--iv=3334353637383930')
    if key_rotation:
      flags.append('--crypto_period_duration=1')

    if live:
      flags.append('--profile=live')
    if dash_if_iop:
      flags.append('--generate_dash_if_iop_compliant_mpd')
    if output_media_info:
      flags.append('--output_media_info')
    else:
      flags += ['--mpd_output', self.mpd_output]

    flags.append('--segment_duration=1')
    # Use fake clock, so output can be compared.
    if use_fake_clock:
      flags.append('--use_fake_clock_for_muxer')
    return flags

  def _CompareWithGold(self, test_output, golden_file_name):
    golden_file = os.path.join(self.golden_file_dir, golden_file_name)
    return filecmp.cmp(test_output, golden_file)

  def _DiffGold(self, test_output, golden_file_name):
    golden_file = os.path.join(self.golden_file_dir, golden_file_name)
    if test_env.options.test_update_golden_files:
      if not filecmp.cmp(test_output, golden_file):
        print 'Updating golden file: ', golden_file_name
        shutil.copyfile(test_output, golden_file)
    else:
      match = filecmp.cmp(test_output, golden_file)
      if not match:
        p = subprocess.Popen(
            ['git', '--no-pager', 'diff', '--color=auto', '--no-ext-diff',
             '--no-index', golden_file, test_output],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)
        output, error = p.communicate()
        self.fail(output + error)

  # '*.media_info' outputs contain media file names, which is changing for
  # every test run. These needs to be replaced for comparison.
  def _DiffMediaInfoGold(self, test_output, golden_file_name):
    media_info_output = test_output + '.media_info'
    # Replaces filename, which is changing for every test run.
    with open(media_info_output, 'r') as f:
      content = f.read()
    with open(media_info_output, 'w') as f:
      f.write(content.replace(test_output, 'place_holder'))
    self._DiffGold(media_info_output, golden_file_name + '.media_info')

  def _DiffLiveGold(self, test_output_prefix, golden_file_name_prefix):
    # Compare init and the first three segments.
    self._DiffGold(test_output_prefix + '-init.mp4',
                   golden_file_name_prefix + '-init.mp4')
    for i in range(1, 4):
      self._DiffGold(test_output_prefix + '-%d.m4s' % i,
                     golden_file_name_prefix + '-%d.m4s' % i)

  # Live mpd contains a current availabilityStartTime, which needs to be
  # replaced for comparison.
  def _DiffLiveMpdGold(self, test_output, golden_file_name):
    with open(test_output, 'r') as f:
      content = f.read()
    # Extract availabilityStartTime
    m = re.search('(?<=availabilityStartTime=")[^"]+', content)
    self.assertIsNotNone(m)
    availability_start_time = m.group(0)
    print 'availabilityStartTime: ', availability_start_time
    with open(test_output, 'w') as f:
      f.write(content.replace(availability_start_time, 'place_holder'))
    self._DiffGold(test_output, golden_file_name)

  def _AssertStreamInfo(self, stream, info):
    stream_info = self.packager.DumpStreamInfo(stream)
    self.assertIn('Found 1 stream(s).', stream_info)
    self.assertIn(info, stream_info)


if __name__ == '__main__':
  unittest.main()
