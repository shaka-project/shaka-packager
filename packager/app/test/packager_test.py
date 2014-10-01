#!/usr/bin/python
#
# Copyright 2014 Google Inc. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

"""Tests utilizing the sample packager binary."""


import os
import shutil
import tempfile
import unittest

import packager_app
import test_env


class PackagerAppTest(unittest.TestCase):

  def setUp(self):
    self.packager = packager_app.PackagerApp()
    self.input = os.path.join(
        test_env.SRC_DIR, 'media', 'test', 'data', 'bear-1280x720.mp4')
    self.tmpdir = tempfile.mkdtemp()
    fd, self.output = tempfile.mkstemp(dir=self.tmpdir)
    os.close(fd)

  def tearDown(self):
    shutil.rmtree(self.tmpdir)

  def testBuildingCode(self):
    self.assertEqual(0, self.packager.BuildSrc())

  def testDumpStreamInfo(self):
    stream_info = self.packager.DumpStreamInfo(self.input)
    expected_stream_info = ('Found 2 stream(s).\n'
                            'Stream [0] type: Audio\n'
                            ' codec_string: mp4a.40.2\n'
                            ' time_scale: 44100\n'
                            ' duration: 121856 (2.8 seconds)\n'
                            ' language: und\n'
                            ' is_encrypted: false\n'
                            ' codec: AAC\n'
                            ' sample_bits: 16\n'
                            ' num_channels: 2\n'
                            ' sampling_frequency: 44100\n\n'
                            'Stream [1] type: Video\n'
                            ' codec_string: avc1.64001f\n'
                            ' time_scale: 30000\n'
                            ' duration: 82082 (2.7 seconds)\n'
                            ' language: und\n'
                            ' is_encrypted: false\n'
                            ' codec: H264\n'
                            ' width: 1280\n'
                            ' height: 720\n'
                            ' nalu_length_size: 4')
    self.assertIn(expected_stream_info, stream_info)

  def testMuxFirstStream(self):
    stream = 'input=%s,stream=0,output=%s' % (self.input, self.output)
    streams = [stream]
    self.packager.Package(streams)
    self._AssertStreamInfo(self.output, 'type: Audio')

  def testMuxAudioStream(self):
    stream = 'input=%s,stream=%s,output=%s' % (self.input, 'audio', self.output)
    streams = [stream]
    self.packager.Package(streams)
    self._AssertStreamInfo(self.output, 'type: Audio')

  def testMuxMultiSegments(self):
    template = '%s$Number$.m4s' % self.output
    stream = ('input=%s,stream=%s,init_segment=%s,segment_template=%s' %
              (self.input, 'video', self.output, template))
    streams = [stream]
    flags = ['--nosingle_segment']
    self.packager.Package(streams, flags)
    self._AssertStreamInfo(self.output, 'type: Video')

  def testEncryptingVideoStream(self):
    stream = 'input=%s,stream=%s,output=%s' % (self.input, 'video', self.output)
    streams = [stream]
    flags = ['--enable_fixed_key_encryption',
             '--key_id=31323334353637383930313233343536',
             '--key=31',
             '--pssh=33']
    self.packager.Package(streams, flags)
    self._AssertStreamInfo(self.output, 'is_encrypted: true')

  @unittest.skipUnless(test_env.has_aes_flags,
                       'Requires AES and network credentials.')
  def testWidevineEncryptionWithAes(self):
    stream = 'input=%s,stream=%s,output=%s' % (self.input, 'video', self.output)
    streams = [stream]
    flags = ['--enable_widevine_encryption',
             '--key_server_url=' + test_env.options.key_server_url,
             '--content_id=' + test_env.options.content_id,
             '--signer=' + test_env.options.signer,
             '--aes_signing_key=' + test_env.options.aes_signing_key,
             '--aes_signing_iv=' + test_env.options.aes_signing_iv]
    self.packager.Package(streams, flags)
    self._AssertStreamInfo(self.output, 'is_encrypted: true')

  @unittest.skipUnless(test_env.has_aes_flags,
                       'Requires AES and network credentials.')
  def testKeyRotationWithAes(self):
    stream = 'input=%s,stream=%s,output=%s' % (self.input, 'video', self.output)
    streams = [stream]
    flags = ['--enable_widevine_encryption',
             '--key_server_url=' + test_env.options.key_server_url,
             '--content_id=' + test_env.options.content_id,
             '--signer=' + test_env.options.signer,
             '--aes_signing_key=' + test_env.options.aes_signing_key,
             '--aes_signing_iv=' + test_env.options.aes_signing_iv,
             '--crypto_period_duration=1']
    self.packager.Package(streams, flags)
    self._AssertStreamInfo(self.output, 'is_encrypted: true')

  @unittest.skipUnless(test_env.has_rsa_flags,
                       'Requires RSA and network credentials.')
  def testWidevineEncryptionWithRsa(self):
    stream = 'input=%s,stream=%s,output=%s' % (self.input, 'video', self.output)
    streams = [stream]
    flags = ['--enable_widevine_encryption',
             '--key_server_url=' + test_env.options.key_server_url,
             '--content_id=' + test_env.options.content_id,
             '--signer=' + test_env.options.signer,
             '--rsa_signing_key_path=' + test_env.options.rsa_signing_key_path]
    self.packager.Package(streams, flags)
    self._AssertStreamInfo(self.output, 'is_encrypted: true')

  def _AssertStreamInfo(self, stream, info):
    stream_info = self.packager.DumpStreamInfo(stream)
    self.assertIn('Found 1 stream(s).', stream_info)
    self.assertIn(info, stream_info)


if __name__ == '__main__':
  unittest.main()
