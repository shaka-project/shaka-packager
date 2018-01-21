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
"""


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
    self.hls_master_playlist_output = self.output_prefix + '.m3u8'
    self.output = None

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

  def _GetStreams(self,
                  stream_descriptors,
                  language_override=None,
                  output_format=None,
                  live=False,
                  hls=False,
                  test_files=None):
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
        # Replace ',', '=' with '_' to make it more like a filename, also
        # avoid potential illegal charactors for a filename.
        for ch in [',', '=']:
          output_prefix = output_prefix.replace(ch, '_')

        if live:
          if output_format == 'ts':
            stream = ('input=%s,stream=%s,segment_template=%s-$Number$.ts' %
                      (test_file, stream_descriptor, output_prefix))
          else:
            stream = (
                'input=%s,stream=%s,init_segment=%s-init.mp4,'
                'segment_template=%s-$Number$.m4s' %
                (test_file, stream_descriptor, output_prefix, output_prefix))
          self.output.append(output_prefix)
        else:
          output = '%s.%s' % (
              output_prefix,
              self._GetExtension(stream_descriptor, output_format))
          stream = ('input=%s,stream=%s,output=%s' %
                    (test_file, stream_descriptor, output))
          self.output.append(output)
        if output_format:
          stream += ',format=%s' % output_format
        if language_override:
          stream += ',lang=%s' % language_override
        if hls:
          stream += ',playlist_name=%s.m3u8' % stream_descriptor
        streams.append(stream)
    return streams

  def _GetExtension(self, stream_descriptor, output_format):
    # TODO(rkuroiwa): Support ttml.
    if stream_descriptor == 'text':
      return 'vtt'
    if output_format:
      return output_format
    # Default to mp4.
    return 'mp4'

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
        p = subprocess.Popen(['git', '--no-pager', 'diff', '--color=auto',
                              '--no-ext-diff', '--no-index', golden_file,
                              test_output],
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
        output, error = p.communicate()
        command_line = self.packager.GetCommandLine()
        failure_message = (output + error + '\n' +
                           _TEST_FAILURE_COMMAND_LINE_MESSAGE +
                           command_line)
        self.fail(failure_message)

  # '*.media_info' outputs contain media file names, which is changing for
  # every test run. These needs to be replaced for comparison.
  def _DiffMediaInfoGold(self, test_output, golden_file_name):
    if platform.system() == 'Windows':
      test_output = test_output.replace('\\', '\\\\')
    media_info_output = test_output + '.media_info'
    # Replaces filename, which is changing for every test run.
    with open(media_info_output, 'rb') as f:
      content = f.read()
    with open(media_info_output, 'wb') as f:
      f.write(content.replace(test_output, 'place_holder'))
    self._DiffGold(media_info_output, golden_file_name + '.media_info')

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

  # Live mpd contains current availabilityStartTime and publishTime, which
  # needs to be replaced for comparison.
  def _DiffLiveMpdGold(self, test_output, golden_file_name):
    with open(test_output, 'rb') as f:
      content = f.read()

    # Extract availabilityStartTime.
    m = re.search('availabilityStartTime="[^"]+"', content)
    self.assertIsNotNone(m)
    availability_start_time = m.group(0)
    print availability_start_time

    # Extract publishTime.
    m = re.search('publishTime="[^"]+"', content)
    self.assertIsNotNone(m)
    publish_time = m.group(0)
    print publish_time
    with open(test_output, 'wb') as f:
      f.write(content.replace(
          availability_start_time,
          'availabilityStartTime="some_availability_start_time"').replace(
              publish_time, 'publishTime="some_publish_time"'))

    self._DiffGold(test_output, golden_file_name)


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
    self._DiffGold(self.output[0], 'bear-640x360-v-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-v-golden.mpd')

  def testPackageText(self):
    self.assertPackageSuccess(
        self._GetStreams(['text'], test_files=['subtitle-english.vtt']),
        self._GetFlags())
    self._DiffGold(self.output[0], 'subtitle-english-golden.vtt')
    self._DiffGold(self.mpd_output, 'subtitle-english-vtt-golden.mpd')

  # Probably one of the most common scenarios is to package audio and video.
  def testPackageAudioVideo(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video']), self._GetFlags())
    self._DiffGold(self.output[0], 'bear-640x360-a-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-golden.mpd')

  def testPackageAudioVideoWithTrickPlay(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video', 'video,trick_play_factor=1']),
        self._GetFlags())
    self._DiffGold(self.output[0], 'bear-640x360-a-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-golden.mp4')
    self._DiffGold(self.output[2], 'bear-640x360-v-trick-1-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-trick-1-golden.mpd')

  def testPackageAudioVideoWithTwoTrickPlay(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video', 'video,trick_play_factor=1',
                          'video,trick_play_factor=2']),
        self._GetFlags())
    self._DiffGold(self.output[0], 'bear-640x360-a-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-golden.mp4')
    self._DiffGold(self.output[2], 'bear-640x360-v-trick-1-golden.mp4')
    self._DiffGold(self.output[3], 'bear-640x360-v-trick-2-golden.mp4')
    self._DiffGold(self.mpd_output,
                   'bear-640x360-av-trick-1-trick-2-golden.mpd')

  def testPackageAudioVideoWithTwoTrickPlayDecreasingRate(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video', 'video,trick_play_factor=2',
                          'video,trick_play_factor=1']),
        self._GetFlags())
    self._DiffGold(self.output[0], 'bear-640x360-a-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-golden.mp4')
    self._DiffGold(self.output[2], 'bear-640x360-v-trick-2-golden.mp4')
    self._DiffGold(self.output[3], 'bear-640x360-v-trick-1-golden.mp4')
    # Since the stream descriptors are sorted in packager app, a different
    # order of trick play factors gets the same mpd.
    self._DiffGold(self.mpd_output,
                   'bear-640x360-av-trick-1-trick-2-golden.mpd')

  def testPackageAudioVideoWithLanguageOverride(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], language_override='por-BR'),
        self._GetFlags())
    self._DiffGold(self.output[0], 'bear-640x360-a-por-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-por-golden.mpd')

  def testPackageAudioVideoWithLanguageOverrideWithSubtag(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], language_override='por-BR'),
        self._GetFlags())
    self._DiffGold(self.output[0], 'bear-640x360-a-por-BR-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-por-BR-golden.mpd')

  def testPackageAacHe(self):
    self.assertPackageSuccess(
        self._GetStreams(
            ['audio'], test_files=['bear-640x360-aac_he-silent_right.mp4']),
        self._GetFlags())
    self._DiffGold(self.output[0],
                   'bear-640x360-aac_he-silent_right-golden.mp4')
    self._DiffGold(self.mpd_output,
                   'bear-640x360-aac_he-silent_right-golden.mpd')

  # Package all video, audio, and text.
  def testPackageVideoAudioText(self):
    audio_video_streams = self._GetStreams(['audio', 'video'])
    text_stream = self._GetStreams(['text'],
                                   test_files=['subtitle-english.vtt'])
    self.assertPackageSuccess(audio_video_streams + text_stream,
                              self._GetFlags())
    self._DiffGold(self.output[0], 'bear-640x360-a-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-golden.mp4')
    self._DiffGold(self.output[2], 'subtitle-english-golden.vtt')
    self._DiffGold(self.mpd_output, 'bear-640x360-avt-golden.mpd')

  def testPackageAvcAacTs(self):
    # Currently we only support live packaging for ts.
    self.assertPackageSuccess(
        self._GetStreams(
            ['audio', 'video'],
            output_format='ts',
            live=True,
            hls=True,
            test_files=['bear-640x360.ts']),
        self._GetFlags(output_hls=True))
    self._DiffLiveGold(self.output[0],
                       'bear-640x360-a-golden',
                       output_format='ts')
    self._DiffLiveGold(self.output[1],
                       'bear-640x360-v-golden',
                       output_format='ts')
    self._DiffGold(self.hls_master_playlist_output,
                   'bear-640x360-av-master-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'audio.m3u8'), 'bear-640x360-a-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'video.m3u8'), 'bear-640x360-v-golden.m3u8')

  def testPackageAvcAc3Ts(self):
    # Currently we only support live packaging for ts.
    self.assertPackageSuccess(
        self._GetStreams(
            ['audio', 'video'],
            output_format='ts',
            live=True,
            hls=True,
            test_files=['bear-640x360-ac3.ts']),
        self._GetFlags(output_hls=True))
    self._DiffLiveGold(self.output[0],
                       'bear-640x360-ac3-golden',
                       output_format='ts')
    self._DiffLiveGold(self.output[1],
                       'bear-640x360-v-golden',
                       output_format='ts')
    self._DiffGold(self.hls_master_playlist_output,
                   'bear-640x360-av-ac3-master-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'audio.m3u8'),
        'bear-640x360-ac3-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'video.m3u8'), 'bear-640x360-v-golden.m3u8')

  def testPackageAvcAc3TsToMp4(self):
    self.assertPackageSuccess(
        self._GetStreams(
            ['audio', 'video'], hls=True, test_files=['bear-640x360-ac3.ts']),
        self._GetFlags(output_hls=True))
    self._DiffGold(self.output[0], 'bear-640x360-ac3-from-ts-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-from-ts-golden.mp4')
    self._DiffGold(self.hls_master_playlist_output,
                   'bear-640x360-av-ac3-ts-to-mp4-master-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'audio.m3u8'),
        'bear-640x360-ac3-ts-to-mp4-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'video.m3u8'),
        'bear-640x360-v-ts-to-mp4-golden.m3u8')

  def testPackageAvcTsLivePlaylist(self):
    self.assertPackageSuccess(
        self._GetStreams(
            ['audio', 'video'],
            output_format='ts',
            live=True,
            hls=True,
            test_files=['bear-640x360.ts']),
        self._GetFlags(
            output_hls=True,
            hls_playlist_type='LIVE',
            time_shift_buffer_depth=0.5))
    self._DiffLiveGold(self.output[0],
                       'bear-640x360-a-golden',
                       output_format='ts')
    self._DiffLiveGold(self.output[1],
                       'bear-640x360-v-golden',
                       output_format='ts')
    self._DiffGold(self.hls_master_playlist_output,
                   'bear-640x360-av-master-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'audio.m3u8'),
        'bear-640x360-a-live-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'video.m3u8'),
        'bear-640x360-v-live-golden.m3u8')

  def testPackageAvcTsLivePlaylistWithKeyRotation(self):
    self.packager.Package(
        self._GetStreams(
            ['audio', 'video'],
            output_format='ts',
            live=True,
            hls=True,
            test_files=['bear-640x360.ts']),
        self._GetFlags(
            encryption=True,
            key_rotation=True,
            output_hls=True,
            hls_playlist_type='LIVE',
            time_shift_buffer_depth=0.5))
    self._DiffLiveGold(self.output[0],
                       'bear-640x360-a-enc-rotation-golden',
                       output_format='ts')
    self._DiffLiveGold(self.output[1],
                       'bear-640x360-v-enc-rotation-golden',
                       output_format='ts')
    self._DiffGold(self.hls_master_playlist_output,
                   'bear-640x360-av-master-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'audio.m3u8'),
        'bear-640x360-a-live-enc-rotation-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'video.m3u8'),
        'bear-640x360-v-live-enc-rotation-golden.m3u8')

  def testPackageAvcTsEventPlaylist(self):
    self.assertPackageSuccess(
        self._GetStreams(
            ['audio', 'video'],
            output_format='ts',
            live=True,
            hls=True,
            test_files=['bear-640x360.ts']),
        self._GetFlags(
            output_hls=True,
            hls_playlist_type='EVENT',
            time_shift_buffer_depth=0.5))
    self._DiffLiveGold(self.output[0],
                       'bear-640x360-a-golden',
                       output_format='ts')
    self._DiffLiveGold(self.output[1],
                       'bear-640x360-v-golden',
                       output_format='ts')
    self._DiffGold(self.hls_master_playlist_output,
                   'bear-640x360-av-master-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'audio.m3u8'),
        'bear-640x360-a-event-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'video.m3u8'),
        'bear-640x360-v-event-golden.m3u8')

  def testPackageVp8Webm(self):
    self.assertPackageSuccess(
        self._GetStreams(['video'],
                         output_format='webm',
                         test_files=['bear-640x360.webm']),
        self._GetFlags())
    self._DiffGold(self.output[0], 'bear-640x360-vp8-golden.webm')
    self._DiffGold(self.mpd_output, 'bear-640x360-vp8-webm-golden.mpd')

  def testPackageVp9Webm(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'],
                         output_format='webm',
                         test_files=['bear-320x240-vp9-opus.webm']),
        self._GetFlags())
    self._DiffGold(self.output[0], 'bear-320x240-opus-golden.webm')
    self._DiffGold(self.output[1], 'bear-320x240-vp9-golden.webm')
    self._DiffGold(self.mpd_output, 'bear-320x240-vp9-opus-webm-golden.mpd')

  def testPackageVp9WebmWithBlockgroup(self):
    self.assertPackageSuccess(
        self._GetStreams(['video'],
                         output_format='webm',
                         test_files=['bear-vp9-blockgroup.webm']),
        self._GetFlags())
    self._DiffGold(self.output[0], 'bear-vp9-blockgroup-golden.webm')

  def testPackageVorbisWebm(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio'],
                         output_format='webm',
                         test_files=['bear-320x240-audio-only.webm']),
        self._GetFlags())
    self._DiffGold(self.output[0], 'bear-320x240-vorbis-golden.webm')
    self._DiffGold(self.mpd_output, 'bear-320x240-vorbis-webm-golden.mpd')

  def testPackageWithEncryption(self):
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
  def testPackageWithEncryptionUsingFixedKey(self):
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

  def testPackageWithEncryptionMultiKeys(self):
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

  def testPackageWithEncryptionMultiKeysWithStreamLabel(self):
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
    self.assertPackageSuccess(
        self._GetStreams(['audio,drm_label=MyAudio',
                          'video,drm_label=MyVideo']), flags)

    self.encryption_key_id = audio_key_id
    self.encryption_key = audio_key
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self.encryption_key_id = video_key_id
    self.encryption_key = video_key
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')

  def testPackageWithEncryptionOfOnlyVideoStream(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio,skip_encryption=1', 'video']),
        self._GetFlags(encryption=True))
    self._DiffGold(self.output[0], 'bear-640x360-a-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-a-clear-v-cenc-golden.mpd')
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')

  def testPackageWithEncryptionAndTrickPlay(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video', 'video,trick_play_factor=1']),
        self._GetFlags(encryption=True))
    self._DiffGold(self.output[0], 'bear-640x360-a-cenc-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')
    self._DiffGold(self.output[2], 'bear-640x360-v-trick-1-cenc-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-trick-1-cenc-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')
    self._VerifyDecryption(self.output[2], 'bear-640x360-v-trick-1-golden.mp4')

  # TODO(hmchen): Add a test case that SD and HD AdapatationSet share one trick
  # play stream.
  def testPackageWithEncryptionAndTwoTrickPlays(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video', 'video,trick_play_factor=1',
                          'video,trick_play_factor=2']),
        self._GetFlags(encryption=True))
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

  def testPackageWithEncryptionAndNoClearLead(self):
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

  def testPackageWithEncryptionAndNoPsshInStream(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True, include_pssh_in_stream=False))
    self._DiffGold(self.output[0], 'bear-640x360-a-cenc-no-pssh-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cenc-no-pssh-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-cenc-no-pssh-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')

  def testPackageWithEncryptionCbc1(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True,
                       protection_scheme='cbc1'))
    self._DiffGold(self.output[0], 'bear-640x360-a-cbc1-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cbc1-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-cbc1-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')

  def testPackageWithEncryptionCens(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True,
                       protection_scheme='cens'))
    self._DiffGold(self.output[0], 'bear-640x360-a-cens-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cens-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-cens-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')

  def testPackageWithEncryptionCbcs(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True,
                       protection_scheme='cbcs'))
    self._DiffGold(self.output[0], 'bear-640x360-a-cbcs-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cbcs-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-cbcs-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')

  def testPackageWithEncryptionAndAdCues(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True, ad_cues='1.5'))
    self._DiffGold(self.output[0], 'bear-640x360-a-cenc-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-cenc-ad_cues-golden.mpd')
    self._VerifyDecryption(self.output[0], 'bear-640x360-a-demuxed-golden.mp4')
    self._VerifyDecryption(self.output[1], 'bear-640x360-v-golden.mp4')

  def testPackageWithWebmSubsampleEncryption(self):
    self.assertPackageSuccess(
        self._GetStreams(['video'],
                         output_format='webm',
                         test_files=['bear-320x180-vp9-altref.webm']),
        self._GetFlags(encryption=True))
    self._DiffGold(self.output[0], 'bear-320x180-vp9-altref-enc-golden.webm')
    self._VerifyDecryption(self.output[0],
                           'bear-320x180-vp9-altref-dec-golden.webm')

  def testPackageWithWebmVp9FullSampleEncryption(self):
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
            live=True,
            hls=True,
            test_files=['bear-640x360.ts']),
        self._GetFlags(encryption=True, output_hls=True))
    self._DiffLiveGold(self.output[0],
                       'bear-640x360-a-enc-golden',
                       output_format='ts')
    self._DiffLiveGold(self.output[1],
                       'bear-640x360-v-enc-golden',
                       output_format='ts')
    self._DiffGold(self.hls_master_playlist_output,
                   'bear-640x360-av-master-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'audio.m3u8'),
        'bear-640x360-a-enc-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'video.m3u8'),
        'bear-640x360-v-enc-golden.m3u8')

  def testPackageAvcTsWithEncryptionAndFairplay(self):
    # Currently we only support live packaging for ts.
    self.assertPackageSuccess(
        self._GetStreams(
            ['audio', 'video'],
            output_format='ts',
            live=True,
            hls=True,
            test_files=['bear-640x360.ts']),
        self._GetFlags(encryption=True, output_hls=True, fairplay=True))
    self._DiffLiveGold(self.output[0],
                       'bear-640x360-a-enc-golden',
                       output_format='ts')
    self._DiffLiveGold(self.output[1],
                       'bear-640x360-v-enc-golden',
                       output_format='ts')
    self._DiffGold(self.hls_master_playlist_output,
                   'bear-640x360-av-master-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'audio.m3u8'),
        'bear-640x360-a-fairplay-enc-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'video.m3u8'),
        'bear-640x360-v-fairplay-enc-golden.m3u8')

  def testPackageAvcAc3TsWithEncryption(self):
    # Currently we only support live packaging for ts.
    self.assertPackageSuccess(
        self._GetStreams(
            ['audio', 'video'],
            output_format='ts',
            live=True,
            hls=True,
            test_files=['bear-640x360-ac3.ts']),
        self._GetFlags(encryption=True, output_hls=True))
    self._DiffLiveGold(self.output[0],
                       'bear-640x360-ac3-enc-golden',
                       output_format='ts')
    self._DiffLiveGold(self.output[1],
                       'bear-640x360-v-enc-golden',
                       output_format='ts')
    self._DiffGold(self.hls_master_playlist_output,
                   'bear-640x360-av-ac3-master-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'audio.m3u8'),
        'bear-640x360-ac3-enc-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'video.m3u8'),
        'bear-640x360-v-enc-golden.m3u8')

  def testPackageAvcTsWithEncryptionExerciseEmulationPrevention(self):
    self.encryption_key = 'ad7e9786def9159db6724be06dfcde7a'
    # Currently we only support live packaging for ts.
    self.assertPackageSuccess(
        self._GetStreams(
            ['video'],
            output_format='ts',
            live=True,
            hls=True,
            test_files=['sintel-1024x436.mp4']),
        self._GetFlags(
            encryption=True,
            output_hls=True))
    self._DiffLiveGold(self.output[0],
                       'sintel-1024x436-v-enc-golden',
                       output_format='ts')
    self._DiffGold(self.hls_master_playlist_output,
                   'sintel-1024x436-v-enc-master-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'video.m3u8'),
        'sintel-1024x436-v-enc-golden.m3u8')

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
    self._DiffGold(self.output[0], 'bear-320x180-v-wvm-golden.mp4')
    self._DiffGold(self.output[1], 'bear-320x180-a-wvm-golden.mp4')
    self._DiffGold(self.output[2], 'bear-640x360-v-wvm-golden.mp4')
    self._DiffGold(self.output[3], 'bear-640x360-a-wvm-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-wvm-golden.mpd')

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
    self._DiffGold(self.output[0], 'bear-320x180-avc3-wvm-golden.mp4')
    self._DiffGold(self.output[1], 'bear-320x180-a-wvm-golden.mp4')
    self._DiffGold(self.output[2], 'bear-640x360-avc3-wvm-golden.mp4')
    self._DiffGold(self.output[3], 'bear-640x360-a-wvm-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-avc3-wvm-golden.mpd')

  def testPackageWithEncryptionAndRandomIv(self):
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

  def testPackageWithEncryptionAndRealClock(self):
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

  def testPackageWithEncryptionAndNonDashIfIop(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True, dash_if_iop=False))
    self._DiffGold(self.output[0], 'bear-640x360-a-cenc-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-cenc-non-iop-golden.mpd')

  def testPackageWithEncryptionAndOutputMediaInfo(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video']),
        self._GetFlags(encryption=True, output_media_info=True))
    self._DiffGold(self.output[0], 'bear-640x360-a-cenc-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')
    self._DiffMediaInfoGold(self.output[0], 'bear-640x360-a-cenc-golden.mp4')
    self._DiffMediaInfoGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')

  def testPackageWithHlsSingleSegmentMp4Encrypted(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], hls=True),
        self._GetFlags(encryption=True, output_hls=True))
    self._DiffGold(self.output[0], 'bear-640x360-a-cenc-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')
    self._DiffGold(self.hls_master_playlist_output,
                   'bear-640x360-av-mp4-master-cenc-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'audio.m3u8'),
        'bear-640x360-a-mp4-cenc-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'video.m3u8'),
        'bear-640x360-v-mp4-cenc-golden.m3u8')

  def testPackageWithEc3AndHlsSingleSegmentMp4Encrypted(self):
    self.assertPackageSuccess(
        self._GetStreams(
            ['audio', 'video'], hls=True, test_files=['bear-640x360-ec3.mp4']),
        self._GetFlags(encryption=True, output_hls=True))
    self._DiffGold(self.output[0], 'bear-640x360-ec3-a-cenc-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-ec3-v-cenc-golden.mp4')
    self._DiffGold(self.hls_master_playlist_output,
                   'bear-640x360-ec3-av-mp4-master-cenc-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'audio.m3u8'),
        'bear-640x360-ec3-a-mp4-cenc-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'video.m3u8'),
        'bear-640x360-ec3-v-mp4-cenc-golden.m3u8')

  def testPackageWithHlsSingleSegmentMp4EncryptedAndAdCues(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], hls=True),
        self._GetFlags(encryption=True, output_hls=True, ad_cues='1.5'))
    self._DiffGold(self.output[0], 'bear-640x360-a-cenc-golden.mp4')
    self._DiffGold(self.output[1], 'bear-640x360-v-cenc-golden.mp4')
    self._DiffGold(self.hls_master_playlist_output,
                   'bear-640x360-av-mp4-master-cenc-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'audio.m3u8'),
        'bear-640x360-a-mp4-cenc-ad_cues-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'video.m3u8'),
        'bear-640x360-v-mp4-cenc-ad_cues-golden.m3u8')

  # Test HLS with multi-segment mp4 and content in subdirectories.
  def testPackageWithHlsMultiSegmentMp4WithCustomPath(self):
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
    self._DiffLiveGold(audio_output_prefix, 'bear-640x360-a-live-golden')
    self._DiffLiveGold(video_output_prefix, 'bear-640x360-v-live-golden')
    self._DiffGold(self.hls_master_playlist_output,
                   'bear-640x360-av-mp4-master-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'audio', 'audio.m3u8'),
        'bear-640x360-a-mp4-golden.m3u8')
    self._DiffGold(
        os.path.join(self.tmp_dir, 'video', 'video.m3u8'),
        'bear-640x360-v-mp4-golden.m3u8')

  def testPackageWithLiveProfile(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], live=True), self._GetFlags())
    self._DiffLiveGold(self.output[0], 'bear-640x360-a-live-golden')
    self._DiffLiveGold(self.output[1], 'bear-640x360-v-live-golden')
    self._DiffLiveMpdGold(self.mpd_output, 'bear-640x360-av-live-golden.mpd')

  def testPackageWithLiveStaticProfile(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], live=True),
        self._GetFlags(generate_static_mpd=True))
    self._DiffLiveGold(self.output[0], 'bear-640x360-a-live-golden')
    self._DiffLiveGold(self.output[1], 'bear-640x360-v-live-golden')
    self._DiffGold(self.mpd_output, 'bear-640x360-av-live-static-golden.mpd')

  def testPackageWithLiveStaticProfileAndAdCues(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], live=True),
        self._GetFlags(generate_static_mpd=True, ad_cues='1.5'))
    self._DiffLiveGold(self.output[0], 'bear-640x360-a-live-golden')
    self._DiffLiveGold(self.output[1], 'bear-640x360-v-live-golden')
    self._DiffGold(self.mpd_output,
                   'bear-640x360-av-live-static-ad_cues-golden.mpd')

  def testPackageWithLiveProfileAndEncryption(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], live=True),
        self._GetFlags(encryption=True))
    self._DiffLiveGold(self.output[0], 'bear-640x360-a-live-cenc-golden')
    self._DiffLiveGold(self.output[1], 'bear-640x360-v-live-cenc-golden')
    self._DiffLiveMpdGold(self.mpd_output,
                          'bear-640x360-av-live-cenc-golden.mpd')

  def testPackageWithLiveProfileAndEncryptionAndNonDashIfIop(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], live=True),
        self._GetFlags(encryption=True, dash_if_iop=False))
    self._DiffLiveGold(self.output[0], 'bear-640x360-a-live-cenc-golden')
    self._DiffLiveGold(self.output[1], 'bear-640x360-v-live-cenc-golden')
    self._DiffLiveMpdGold(self.mpd_output,
                          'bear-640x360-av-live-cenc-non-iop-golden.mpd')

  def testPackageWithLiveProfileAndEncryptionAndMultFiles(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'],
                         live=True,
                         test_files=['bear-1280x720.mp4', 'bear-640x360.mp4',
                                     'bear-320x180.mp4']),
        self._GetFlags(encryption=True))
    self._DiffLiveGold(self.output[2], 'bear-640x360-a-live-cenc-golden')
    self._DiffLiveGold(self.output[3], 'bear-640x360-v-live-cenc-golden')
    # Mpd cannot be validated right now since we don't generate determinstic
    # mpd with multiple inputs due to thread racing.
    # TODO(kqyang): Generate determinstic mpd or at least validate mpd schema.

  def testPackageWithLiveProfileAndKeyRotation(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], live=True),
        self._GetFlags(encryption=True, key_rotation=True))
    self._DiffLiveGold(self.output[0],
                       'bear-640x360-a-live-cenc-rotation-golden')
    self._DiffLiveGold(self.output[1],
                       'bear-640x360-v-live-cenc-rotation-golden')
    self._DiffLiveMpdGold(self.mpd_output,
                          'bear-640x360-av-live-cenc-rotation-golden.mpd')

  def testPackageWithLiveProfileAndKeyRotationAndNoPsshInStream(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], live=True),
        self._GetFlags(
            encryption=True, key_rotation=True, include_pssh_in_stream=False))
    self._DiffLiveGold(self.output[0],
                       'bear-640x360-a-live-cenc-rotation-no-pssh-golden')
    self._DiffLiveGold(self.output[1],
                       'bear-640x360-v-live-cenc-rotation-no-pssh-golden')
    self._DiffLiveMpdGold(
        self.mpd_output,
        'bear-640x360-av-live-cenc-rotation-no-pssh-golden.mpd')

  def testPackageWithLiveProfileAndKeyRotationAndNonDashIfIop(self):
    self.assertPackageSuccess(
        self._GetStreams(['audio', 'video'], live=True),
        self._GetFlags(encryption=True,
                       key_rotation=True,
                       dash_if_iop=False))
    self._DiffLiveGold(self.output[0],
                       'bear-640x360-a-live-cenc-rotation-golden')
    self._DiffLiveGold(self.output[1],
                       'bear-640x360-v-live-cenc-rotation-golden')
    self._DiffLiveMpdGold(
        self.mpd_output,
        'bear-640x360-av-live-cenc-rotation-non-iop-golden.mpd')

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

  def testPackageWithEncryptionWithIncorrectKeyIdLength1(self):
    self.encryption_key_id = self.encryption_key_id[0:-2]
    packaging_result = self.packager.Package(
        self._GetStreams(['video']), self._GetFlags(encryption=True))
    self.assertEqual(packaging_result, 1)

  def testPackageWithEncryptionWithIncorrectKeyIdLength2(self):
    self.encryption_key_id += '12'
    packaging_result = self.packager.Package(
        self._GetStreams(['video']), self._GetFlags(encryption=True))
    self.assertEqual(packaging_result, 1)

  def testPackageWithEncryptionWithInvalidKeyIdValue(self):
    self.encryption_key_id = self.encryption_key_id[0:-1] + 'g'
    packaging_result = self.packager.Package(
        self._GetStreams(['video']), self._GetFlags(encryption=True))
    self.assertEqual(packaging_result, 1)

  def testPackageWithEncryptionWithIncorrectKeyLength1(self):
    self.encryption_key = self.encryption_key[0:-2]
    packaging_result = self.packager.Package(
        self._GetStreams(['video']), self._GetFlags(encryption=True))
    self.assertEqual(packaging_result, 1)

  def testPackageWithEncryptionWithIncorrectKeyLength2(self):
    self.encryption_key += '12'
    packaging_result = self.packager.Package(
        self._GetStreams(['video']), self._GetFlags(encryption=True))
    self.assertEqual(packaging_result, 1)

  def testPackageWithEncryptionWithInvalidKeyValue(self):
    self.encryption_key = self.encryption_key[0:-1] + 'g'
    packaging_result = self.packager.Package(
        self._GetStreams(['video']), self._GetFlags(encryption=True))
    self.assertEqual(packaging_result, 1)

  def testPackageWithEncryptionWithIncorrectIvLength1(self):
    self.encryption_iv = self.encryption_iv[0:-2]
    packaging_result = self.packager.Package(
        self._GetStreams(['video']), self._GetFlags(encryption=True))
    self.assertEqual(packaging_result, 1)

  def testPackageWithEncryptionWithIncorrectIvLength2(self):
    self.encryption_iv += '12'
    packaging_result = self.packager.Package(
        self._GetStreams(['video']), self._GetFlags(encryption=True))
    self.assertEqual(packaging_result, 1)

  def testPackageWithEncryptionWithInvalidIvValue(self):
    self.encryption_iv = self.encryption_iv[0:-1] + 'g'
    packaging_result = self.packager.Package(
        self._GetStreams(['video']), self._GetFlags(encryption=True))
    self.assertEqual(packaging_result, 1)

  def testPackageWithEncryptionWithInvalidPsshValue1(self):
    packaging_result = self.packager.Package(
        self._GetStreams(['video']),
        self._GetFlags(encryption=True) + ['--pssh=ag'])
    self.assertEqual(packaging_result, 1)

  def testPackageWithEncryptionWithInvalidPsshValue2(self):
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


if __name__ == '__main__':
  unittest.main()
