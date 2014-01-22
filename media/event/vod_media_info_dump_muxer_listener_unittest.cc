#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "media/event/vod_media_info_dump_muxer_listener.h"
#include "media/file/file.h"
#include "media/base/muxer_options.h"
#include "media/base/video_stream_info.h"
#include "mpd/base/media_info.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/text_format.h"

using dash_packager::MediaInfo;

namespace media {
namespace event {

namespace {
struct VideoStreamInfoParameters {
  int track_id;
  uint32 time_scale;
  uint64 duration;
  VideoCodec codec;
  std::string codec_string;
  std::string language;
  uint16 width;
  uint16 height;
  uint8 nalu_length_size;
  std::vector<uint8> extra_data;
  bool is_encrypted;
};

// Note that this does not have vector of StreamInfo pointer.
struct OnMediaEndParameters {
  bool has_init_range;
  uint64 init_range_start;
  uint64 init_range_end;
  bool has_index_range;
  uint64 index_range_start;
  uint64 index_range_end;
  float duration_seconds;
  uint64 file_size;
};

scoped_refptr<StreamInfo> CreateVideoStreamInfo(
    const VideoStreamInfoParameters& param) {
  return scoped_refptr<StreamInfo>(
      new VideoStreamInfo(param.track_id,
                          param.time_scale,
                          param.duration,
                          param.codec,
                          param.codec_string,
                          param.language,
                          param.width,
                          param.height,
                          param.nalu_length_size,
                          &param.extra_data[0],
                          param.extra_data.size(),
                          param.is_encrypted));
}

VideoStreamInfoParameters GetDefaultVideoStreamInfoParams() {
  const int kTrackId = 0;
  const uint32 kTimeScale = 10;
  const uint64 kVideoStreamDuration = 200;
  const VideoCodec kH264Codec = kCodecH264;
  const uint8 kH264Profile = 1;
  const uint8 kH264CompatibleProfile = 1;
  const uint8 kH264Level = 1;
  const char* kLanuageUndefined = "und";
  const uint16 kWidth = 720;
  const uint16 kHeight = 480;
  const uint8 kNaluLengthSize = 1;
  const std::vector<uint8> kExtraData;
  const bool kEncryptedFlag = false;

  return {kTrackId, kTimeScale, kVideoStreamDuration, kH264Codec,
          VideoStreamInfo::GetCodecString(
              kCodecH264, kH264Profile, kH264CompatibleProfile, kH264Level),
          kLanuageUndefined, kWidth, kHeight, kNaluLengthSize, kExtraData,
          kEncryptedFlag};
}

OnMediaEndParameters GetDefaultOnMediaEndParams() {
  // Values for {init, index} range {start, end} are arbitrary, but makes sure
  // that it is monotonically increasing and contiguous.
  const bool kHasInitRange = true;
  const uint64 kInitRangeStart = 0;
  const uint64 kInitRangeEnd = kInitRangeStart + 120;
  const uint64 kHasIndexRange = true;
  const uint64 kIndexRangeStart = kInitRangeEnd + 1;
  const uint64 kIndexRangeEnd = kIndexRangeStart + 100;
  const float kMediaDuration = 10.5f;
  const uint64 kFileSize = 10000;
  return {kHasInitRange,    kInitRangeStart, kInitRangeEnd,  kHasIndexRange,
          kIndexRangeStart, kIndexRangeEnd,  kMediaDuration, kFileSize};
}

void SetDefaultMuxerOptionsValues(MuxerOptions* muxer_options) {
  muxer_options->single_segment = false;
  muxer_options->segment_duration = 10.0;
  muxer_options->fragment_duration = 10.0;
  muxer_options->segment_sap_aligned = true;
  muxer_options->fragment_sap_aligned = true;
  muxer_options->num_subsegments_per_sidx = 0;
  muxer_options->output_file_name = "test_output_file_name.mp4";
  muxer_options->segment_template.clear();
  muxer_options->temp_file_name.clear();
}

void ExpectMediaInfoEqual(const MediaInfo& expect, const MediaInfo& actual) {
  // I found out here
  // https://groups.google.com/forum/#!msg/protobuf/5sOExQkB2eQ/ZSBNZI0K54YJ
  // that the best way to check equality is to serialize and check equality.
  std::string expect_serialized;
  std::string actual_serialized;
  ASSERT_TRUE(expect.SerializeToString(&expect_serialized));
  ASSERT_TRUE(actual.SerializeToString(&actual_serialized));
  ASSERT_EQ(expect_serialized, actual_serialized);
}

void ExpectTextFormatMediaInfoEqual(const std::string& expect,
                                    const std::string& actual) {
  MediaInfo expect_media_info;
  MediaInfo actual_media_info;
  typedef ::google::protobuf::TextFormat TextFormat;
  ASSERT_TRUE(TextFormat::ParseFromString(expect, &expect_media_info))
      << "Failed to parse " << std::endl << expect;
  ASSERT_TRUE(TextFormat::ParseFromString(actual, &actual_media_info))
      << "Failed to parse " << std::endl <<  actual;
  ASSERT_NO_FATAL_FAILURE(
      ExpectMediaInfoEqual(expect_media_info, actual_media_info))
      << "Expect:" << std::endl << expect << std::endl
      << "Actual:" << std::endl << actual;
}
}  // namespace

class VodMediaInfoDumpMuxerListenerTest : public ::testing::Test {
 public:
  VodMediaInfoDumpMuxerListenerTest() {}
  virtual ~VodMediaInfoDumpMuxerListenerTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(file_util::CreateTemporaryFile(&temp_file_path_));
    DLOG(INFO) << "Created temp file: " << temp_file_path_.value();

    temp_file_ = File::Open(temp_file_path_.value().c_str(), "w");
    ASSERT_TRUE(temp_file_);
    listener_.reset(new VodMediaInfoDumpMuxerListener(temp_file_));
  }

  virtual void TearDown() OVERRIDE {
    base::DeleteFile(temp_file_path_, false);
  }

  void FireOnMediaStartWithDefaultValues(
      const std::vector<StreamInfo*> stream_infos) {
    MuxerOptions muxer_options;
    SetDefaultMuxerOptionsValues(&muxer_options);
    const uint32 kReferenceTimeScale = 1000;
    listener_->OnMediaStart(muxer_options,
                            stream_infos,
                            kReferenceTimeScale,
                            MuxerListener::kContainerMp4);
  }

  void FireOnMediaEndWithParams(const std::vector<StreamInfo*> stream_infos,
                                const OnMediaEndParameters& params) {
    // On success, this writes the result to |temp_file_|.
    listener_->OnMediaEnd(stream_infos,
                          params.has_init_range,
                          params.init_range_start,
                          params.init_range_end,
                          params.has_index_range,
                          params.index_range_start,
                          params.index_range_end,
                          params.duration_seconds,
                          params.file_size);
  }

  void ExpectTempFileToEqual(const std::string& expected_protobuf) {
    std::string temp_file_media_info_str;
    ASSERT_TRUE(File::ReadFileToString(temp_file_path_.value().c_str(),
                                       &temp_file_media_info_str));
    ASSERT_TRUE(!temp_file_media_info_str.empty());

    ASSERT_NO_FATAL_FAILURE((ExpectTextFormatMediaInfoEqual(
        expected_protobuf, temp_file_media_info_str)));
  }

 protected:
  File* temp_file_;
  base::FilePath temp_file_path_;
  scoped_ptr<VodMediaInfoDumpMuxerListener> listener_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VodMediaInfoDumpMuxerListenerTest);
};

// TODO(rkuroiwa): Enable these when implemented.
TEST_F(VodMediaInfoDumpMuxerListenerTest, DISABLED_UnencryptedStream_Normal) {
  VideoStreamInfoParameters params = GetDefaultVideoStreamInfoParams();
  params.is_encrypted = false;

  std::vector<StreamInfo*> stream_infos;
  scoped_refptr<StreamInfo> stream_info = CreateVideoStreamInfo(params);
  stream_infos.push_back(stream_info.get());

  FireOnMediaStartWithDefaultValues(stream_infos);
  FireOnMediaEndWithParams(stream_infos,
                           GetDefaultOnMediaEndParams());
  ASSERT_TRUE(temp_file_->Close());

  const char* kExpectedProtobufOutput = "\
      bandwidth: 7620\n\
      video_info {\n\
        codec: \"avc1.010101\"\n\
        width: 720\n\
        height: 480\n\
        time_scale: 10\n\
      }\n\
      init_range {\n\
        begin: 0\n\
        end: 120\n\
      }\n\
      index_range {\n\
        begin: 121\n\
        end: 221\n\
      }\n\
      reference_time_scale: 1000\n\
      container_type: 1\n\
      media_file_name: \"test_output_file_name.mp4\"\n\
      media_duration_seconds: 10.5\n";
  ASSERT_NO_FATAL_FAILURE(ExpectTempFileToEqual(kExpectedProtobufOutput));
}

TEST_F(VodMediaInfoDumpMuxerListenerTest, DISABLED_EncryptedStream_Normal) {
  listener_->SetContentProtectionSchemeIdUri("http://foo.com/bar");

  VideoStreamInfoParameters params = GetDefaultVideoStreamInfoParams();
  params.is_encrypted = true;

  std::vector<StreamInfo*> stream_infos;
  scoped_refptr<StreamInfo> stream_info = CreateVideoStreamInfo(params);
  stream_infos.push_back(stream_info.get());

  FireOnMediaStartWithDefaultValues(stream_infos);
  FireOnMediaEndWithParams(stream_infos,
                           GetDefaultOnMediaEndParams());
  ASSERT_TRUE(temp_file_->Close());

  const char* kExpectedProtobufOutput = "\
      bandwidth: 7620\n\
      video_info {\n\
        codec: \"avc1.010101\"\n\
        width: 720\n\
        height: 480\n\
        time_scale: 10\n\
      }\n\
      content_protections {\n\
        scheme_id_uri: \"http://foo.com/bar\"\n\
      }\n\
      init_range {\n\
        begin: 0\n\
        end: 120\n\
      }\n\
      index_range {\n\
        begin: 121\n\
        end: 221\n\
      }\n\
      reference_time_scale: 1000\n\
      container_type: 1\n\
      media_file_name: \"test_output_file_name.mp4\"\n\
      media_duration_seconds: 10.5\n";
  ASSERT_NO_FATAL_FAILURE(ExpectTempFileToEqual(kExpectedProtobufOutput));
}

}  // namespace event
}  // namespace media
