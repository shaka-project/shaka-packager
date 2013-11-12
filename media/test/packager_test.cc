// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "media/base/demuxer.h"
#include "media/base/fixed_encryptor_source.h"
#include "media/base/media_sample.h"
#include "media/base/media_stream.h"
#include "media/base/muxer.h"
#include "media/base/muxer_options.h"
#include "media/base/status_test_util.h"
#include "media/base/stream_info.h"
#include "media/base/test_data_util.h"
#include "media/mp4/mp4_muxer.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Combine;
using ::testing::Values;
using ::testing::ValuesIn;

namespace {
const char* kMediaFiles[] = {"bear-1280x720.mp4", "bear-1280x720-av_frag.mp4"};

// Encryption constants.
const char kKeyIdHex[] = "e5007e6e9dcd5ac095202ed3758382cd";
const char kKeyHex[] = "6fc96fe628a265b13aeddec0bc421f4d";
const char kPsshHex[] =
    "08011210e5007e6e9dcd5ac095202ed3"
    "758382cd1a0d7769646576696e655f746573742211544553545f"
    "434f4e54454e545f49445f312a025344";
const uint32 kClearMilliseconds = 1500;

}  // namespace

namespace media {

class TestingMuxer : public Muxer {
 public:
  TestingMuxer(const MuxerOptions& options, EncryptorSource* encryptor_source)
      : Muxer(options, encryptor_source) {}

  virtual Status Initialize() OVERRIDE {
    DVLOG(1) << "Initialize is called.";
    return Status::OK;
  }

  virtual Status AddSample(const MediaStream* stream,
                           scoped_refptr<MediaSample> sample) OVERRIDE {
    DVLOG(1) << "Add Sample: " << sample->ToString();
    DVLOG(2) << "To Stream: " << stream->ToString();
    return Status::OK;
  }

  virtual Status Finalize() OVERRIDE {
    DVLOG(1) << "Finalize is called.";
    return Status::OK;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingMuxer);
};

typedef Muxer* CreateMuxerFunc(const std::string& input_file_name,
                               EncryptorSource* encryptor_source);

Muxer* CreateTestingMuxer(const std::string& input_file_name,
                          EncryptorSource* encryptor_source) {
  MuxerOptions options;
  return new TestingMuxer(options, NULL);
}

Muxer* CreateNormalMP4Muxer(const std::string& input_file_name,
                            EncryptorSource* encryptor_source) {
  MuxerOptions options;
  options.single_segment = true;
  options.segment_duration = 0.005;
  options.fragment_duration = 0.002;
  options.segment_sap_aligned = true;
  options.fragment_sap_aligned = true;
  options.num_subsegments_per_sidx = 1;
  options.output_file_name = "/tmp/clear_" + input_file_name;
  options.segment_template = "/tmp/template$Number$.m4s";
  options.temp_file_name = "/tmp/tmp.mp4";
  return new mp4::MP4Muxer(options, NULL);
}

Muxer* CreateEncryptionMP4Muxer(const std::string& input_file_name,
                                EncryptorSource* encryptor_source) {
  MuxerOptions options;
  options.single_segment = true;
  options.segment_duration = 0.005;
  options.fragment_duration = 0.002;
  options.segment_sap_aligned = true;
  options.fragment_sap_aligned = true;
  options.num_subsegments_per_sidx = 1;
  options.output_file_name = "/tmp/enc_" + input_file_name;
  options.segment_template = "/tmp/template$Number$.m4s";
  options.temp_file_name = "/tmp/tmp.mp4";
  return new mp4::MP4Muxer(options, encryptor_source);
}

class PackagerTest : public ::testing::TestWithParam<
                         ::std::tr1::tuple<const char*, CreateMuxerFunc*> > {};

TEST_P(PackagerTest, Remux) {
  std::string file_name = ::std::tr1::get<0>(GetParam());
  CreateMuxerFunc* CreateMuxer = ::std::tr1::get<1>(GetParam());

  Demuxer demuxer(GetTestDataFilePath(file_name).value(), NULL);
  ASSERT_OK(demuxer.Initialize());

  LOG(INFO) << "Num Streams: " << demuxer.streams().size();
  for (int i = 0; i < demuxer.streams().size(); ++i) {
    LOG(INFO) << "Streams " << i << " " << demuxer.streams()[i]->ToString();
  }

  FixedEncryptorSource encryptor_source(
      kKeyIdHex, kKeyHex, kPsshHex, kClearMilliseconds);
  EXPECT_OK(encryptor_source.Initialize());

  scoped_ptr<Muxer> muxer(CreateMuxer(file_name, &encryptor_source));

  ASSERT_OK(muxer->AddStream(demuxer.streams()[0]));
  ASSERT_OK(muxer->Initialize());

  // Starts remuxing process.
  ASSERT_OK(demuxer.Run());

  ASSERT_OK(muxer->Finalize());
}

INSTANTIATE_TEST_CASE_P(PackagerE2ETest,
                        PackagerTest,
                        Combine(ValuesIn(kMediaFiles),
                                Values(&CreateTestingMuxer,
                                       &CreateNormalMP4Muxer,
                                       &CreateEncryptionMP4Muxer)));

}  // namespace media
