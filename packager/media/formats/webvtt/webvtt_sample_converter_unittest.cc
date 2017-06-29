#include "packager/media/formats/webvtt/webvtt_sample_converter.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/base/strings/string_number_conversions.h"
#include "packager/media/base/media_sample.h"
#include "packager/status_test_util.h"

namespace shaka {
namespace media {
namespace mp4 {

namespace {
// The actual messages don't matter.
const char kCueMessage1[] = "hi";
const char kCueMessage2[] = "hello";
const char kCueMessage3[] = "some multi word message";
const char kCueMessage4[] = "message!!";

// Data is a vector and must not be empty.
MATCHER_P3(MatchesStartTimeEndTimeAndData, start_time, end_time, data, "") {
  *result_listener << "which is (" << arg->pts() << ", "
                   << (arg->pts() + arg->duration()) << ", "
                   << base::HexEncode(arg->data(), arg->data_size()) << ")";
  return arg->pts() == start_time &&
         (arg->pts() + arg->duration() == end_time) &&
         arg->data_size() == data.size() &&
         (memcmp(&data[0], arg->data(), arg->data_size()) == 0);
}
}  // namespace

class WebVttFragmenterTest : public ::testing::Test {
 protected:
  WebVttSampleConverter webvtt_sample_converter_;
};

// Verify that AppednBoxToVector works.
TEST_F(WebVttFragmenterTest, AppendBoxToVector) {
  const uint8_t kExpected[] = {
    0x0,  0x0,  0x0,  0x1c,  // Size.
    0x76, 0x74, 0x74, 0x63,  // 'vttc'.
    0x0,  0x0,  0x0,  0x14,  // Size of payload Box.
    0x70, 0x61, 0x79, 0x6c,  // 'payl'.
    // "some message" as hex without null terminator.
    0x73, 0x6f, 0x6d, 0x65, 0x20, 0x6d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65,
  };
  VTTCueBox cue_box;
  cue_box.cue_payload.cue_text = "some message";
  std::vector<uint8_t> serialized;
  AppendBoxToVector(&cue_box, &serialized);
  std::vector<uint8_t> expected_in_vector_form(
      kExpected, kExpected + arraysize(kExpected));
  EXPECT_EQ(expected_in_vector_form, serialized);
}

// There are 6 ways the cues can be arranged.
// 1. No overlap, contiguous. Test: NoOverlapContiguous
//   |-- cue1 --|
//              |-- cue2 --|
//
// 2. No overlap, gap. Test: Gap
//   |-- cue1 --|
//                 |-- cue2 --|
//
// 3. Overlap sequential (like a staircase). Test: OverlappingCuesSequential
//   |-- cue1 --|
//      |-- cue2 --|
//         |-- cue3 --|
//
// 4. Longer cues overlapping with shorter cues. Test: OverlappingLongCue
//   |---------- cue1 ----------|
//     |--- cue2 ---|
//       |- cue3 -|
//                    |- cue4 -|
//
// 5. The first cue doesn't start at 00:00.000. Test: GapAtBeginning
//   <start>   |--- cue1 ---|
//
// 6. 2 or more cues start at the same time. Test: Same start time.
//   |--- cue1 ---|
//   |-- cue2 --|

TEST_F(WebVttFragmenterTest, NoOverlapContiguous) {
  Cue cue1;
  cue1.payload = kCueMessage1;
  cue1.start_time = 0;
  cue1.duration = 2000;
  webvtt_sample_converter_.PushCue(cue1);

  Cue cue2;
  cue2.payload = kCueMessage2;
  cue2.start_time = 2000;
  cue2.duration = 1000;

  webvtt_sample_converter_.PushCue(cue2);
  webvtt_sample_converter_.Flush();
  EXPECT_EQ(2u, webvtt_sample_converter_.ReadySamplesSize());

  VTTCueBox first_cue_data;
  first_cue_data.cue_payload.cue_text = kCueMessage1;
  std::vector<uint8_t> expected;
  AppendBoxToVector(&first_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(0, 2000, expected));

  VTTCueBox second_cue_data;
  second_cue_data.cue_payload.cue_text = kCueMessage2;
  expected.clear();
  AppendBoxToVector(&second_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(2000, 3000, expected));
}

// Verify that if is a gap, then a sample is created for the gap.
TEST_F(WebVttFragmenterTest, Gap) {
  Cue cue1;
  cue1.payload = kCueMessage1;
  cue1.start_time = 0;
  cue1.duration = 1000;
  webvtt_sample_converter_.PushCue(cue1);

  Cue cue2;
  cue2.payload = kCueMessage2;
  cue2.start_time = 2000;
  cue2.duration = 1000;
  webvtt_sample_converter_.PushCue(cue2);

  EXPECT_EQ(2u, webvtt_sample_converter_.ReadySamplesSize());

  webvtt_sample_converter_.Flush();
  EXPECT_EQ(3u, webvtt_sample_converter_.ReadySamplesSize());

  VTTCueBox first_cue_data;
  first_cue_data.cue_payload.cue_text = kCueMessage1;
  std::vector<uint8_t> expected;
  AppendBoxToVector(&first_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(0, 1000, expected));

  VTTEmptyCueBox empty_cue;
  expected.clear();
  AppendBoxToVector(&empty_cue, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(1000, 2000, expected));

  VTTCueBox second_cue_data;
  second_cue_data.cue_payload.cue_text = kCueMessage2;
  expected.clear();
  AppendBoxToVector(&second_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(2000, 3000, expected));
}

// The previous cue always ends before the current cue ends.
// Cues are overlapping, no samples should be created in PushSample().
TEST_F(WebVttFragmenterTest, OverlappingCuesSequential) {
  Cue cue1;
  cue1.payload = kCueMessage1;
  cue1.start_time = 0;
  cue1.duration = 2000;
  webvtt_sample_converter_.PushCue(cue1);

  Cue cue2;
  cue2.payload = kCueMessage2;
  cue2.start_time = 1000;
  cue2.duration = 2000;
  webvtt_sample_converter_.PushCue(cue2);

  Cue cue3;
  cue3.payload = kCueMessage3;
  cue3.start_time = 1500;
  cue3.duration = 4000;
  webvtt_sample_converter_.PushCue(cue3);

  webvtt_sample_converter_.Flush();
  // There should be 5 samples for [0,1000], [1000,1500], [1500,2000],
  // [2000,3000], and [3000, 5500].
  EXPECT_EQ(5u, webvtt_sample_converter_.ReadySamplesSize());

  VTTCueBox first_cue_data;
  first_cue_data.cue_payload.cue_text = kCueMessage1;
  std::vector<uint8_t> expected;
  AppendBoxToVector(&first_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(0, 1000, expected));

  VTTCueBox second_cue_data;
  second_cue_data.cue_payload.cue_text = kCueMessage2;
  expected.clear();
  AppendBoxToVector(&first_cue_data, &expected);
  AppendBoxToVector(&second_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(1000, 1500, expected));

  VTTCueBox third_cue_data;
  third_cue_data.cue_payload.cue_text = kCueMessage3;
  expected.clear();
  AppendBoxToVector(&first_cue_data, &expected);
  AppendBoxToVector(&second_cue_data, &expected);
  AppendBoxToVector(&third_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(1500, 2000, expected));

  expected.clear();
  AppendBoxToVector(&second_cue_data, &expected);
  AppendBoxToVector(&third_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(2000, 3000, expected));

  expected.clear();
  AppendBoxToVector(&third_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(3000, 5500, expected));
}

TEST_F(WebVttFragmenterTest, OverlappingLongCue) {
  Cue cue1;
  cue1.payload = kCueMessage1;
  cue1.start_time = 0;
  cue1.duration = 10000;
  webvtt_sample_converter_.PushCue(cue1);

  Cue cue2;
  cue2.payload = kCueMessage2;
  cue2.start_time = 1000;
  cue2.duration = 5000;
  webvtt_sample_converter_.PushCue(cue2);

  Cue cue3;
  cue3.payload = kCueMessage3;
  cue3.start_time = 2000;
  cue3.duration = 1000;
  webvtt_sample_converter_.PushCue(cue3);

  Cue cue4;
  cue4.payload = kCueMessage4;
  cue4.start_time = 8000;
  cue4.duration = 1000;
  webvtt_sample_converter_.PushCue(cue4);
  webvtt_sample_converter_.Flush();

  // There should be 7 samples for [0,1000], [1000,2000], [2000,3000],
  // [3000,6000], [6000, 8000], [8000, 9000], [9000, 10000].
  EXPECT_EQ(7u, webvtt_sample_converter_.ReadySamplesSize());

  VTTCueBox first_long_cue_data;
  first_long_cue_data.cue_payload.cue_text = kCueMessage1;
  std::vector<uint8_t> expected;
  AppendBoxToVector(&first_long_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(0, 1000, expected));

  VTTCueBox second_cue_data;
  second_cue_data.cue_payload.cue_text = kCueMessage2;
  expected.clear();
  AppendBoxToVector(&first_long_cue_data, &expected);
  AppendBoxToVector(&second_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(1000, 2000, expected));

  VTTCueBox third_cue_data;
  third_cue_data.cue_payload.cue_text = kCueMessage3;
  expected.clear();
  AppendBoxToVector(&first_long_cue_data, &expected);
  AppendBoxToVector(&second_cue_data, &expected);
  AppendBoxToVector(&third_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(2000, 3000, expected));

  expected.clear();
  AppendBoxToVector(&first_long_cue_data, &expected);
  AppendBoxToVector(&second_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(3000, 6000, expected));

  expected.clear();
  AppendBoxToVector(&first_long_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(6000, 8000, expected));

  VTTCueBox fourth_cue_data;
  fourth_cue_data.cue_payload.cue_text = kCueMessage4;
  expected.clear();
  AppendBoxToVector(&first_long_cue_data, &expected);
  AppendBoxToVector(&fourth_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(8000, 9000, expected));

  expected.clear();
  AppendBoxToVector(&first_long_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(9000, 10000, expected));
}

TEST_F(WebVttFragmenterTest, GapAtBeginning) {
  Cue cue;
  cue.payload = kCueMessage1;
  cue.start_time = 1200;
  cue.duration = 2000;
  webvtt_sample_converter_.PushCue(cue);

  webvtt_sample_converter_.Flush();
  EXPECT_EQ(1u, webvtt_sample_converter_.ReadySamplesSize());

  VTTCueBox cue_data;
  cue_data.cue_payload.cue_text = kCueMessage1;
  std::vector<uint8_t> expected;
  AppendBoxToVector(&cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(1200, 3200, expected));
}

TEST_F(WebVttFragmenterTest, SameStartTime) {
  Cue cue1;
  cue1.payload = kCueMessage1;
  cue1.start_time = 0;
  cue1.duration = 2000;
  webvtt_sample_converter_.PushCue(cue1);

  Cue cue2;
  cue2.payload = kCueMessage2;
  cue2.start_time = 0;
  cue2.duration = 1500;
  webvtt_sample_converter_.PushCue(cue2);

  webvtt_sample_converter_.Flush();
  EXPECT_EQ(2u, webvtt_sample_converter_.ReadySamplesSize());

  VTTCueBox first_cue_data;
  first_cue_data.cue_payload.cue_text = kCueMessage1;
  VTTCueBox second_cue_data;
  second_cue_data.cue_payload.cue_text = kCueMessage2;

  std::vector<uint8_t> expected;
  AppendBoxToVector(&first_cue_data, &expected);
  AppendBoxToVector(&second_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(0, 1500, expected));

  expected.clear();
  AppendBoxToVector(&first_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(1500, 2000, expected));
}

// This test is a combination of the test cases above.
TEST_F(WebVttFragmenterTest, MoreCases) {
  Cue cue1;
  cue1.payload = kCueMessage1;
  cue1.start_time = 0;
  cue1.duration = 2000;
  webvtt_sample_converter_.PushCue(cue1);

  Cue cue2;
  cue2.payload = kCueMessage2;
  cue2.start_time = 100;
  cue2.duration = 100;
  webvtt_sample_converter_.PushCue(cue2);

  Cue cue3;
  cue3.payload = kCueMessage3;
  cue3.start_time = 1500;
  cue3.duration = 1000;
  webvtt_sample_converter_.PushCue(cue3);

  Cue cue4;
  cue4.payload = kCueMessage4;
  cue4.start_time = 1500;
  cue4.duration = 800;
  webvtt_sample_converter_.PushCue(cue4);

  webvtt_sample_converter_.Flush();
  EXPECT_EQ(6u, webvtt_sample_converter_.ReadySamplesSize());

  VTTCueBox first_cue_data;
  first_cue_data.cue_payload.cue_text = kCueMessage1;
  VTTCueBox second_cue_data;
  second_cue_data.cue_payload.cue_text = kCueMessage2;
  VTTCueBox third_cue_data;
  third_cue_data.cue_payload.cue_text = kCueMessage3;
  VTTCueBox fourth_cue_data;
  fourth_cue_data.cue_payload.cue_text = kCueMessage4;

  std::vector<uint8_t> expected;
  AppendBoxToVector(&first_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(0, 100, expected));

  expected.clear();
  AppendBoxToVector(&first_cue_data, &expected);
  AppendBoxToVector(&second_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(100, 200, expected));

  expected.clear();
  AppendBoxToVector(&first_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(200, 1500, expected));

  expected.clear();
  AppendBoxToVector(&first_cue_data, &expected);
  AppendBoxToVector(&third_cue_data, &expected);
  AppendBoxToVector(&fourth_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(1500, 2000, expected));

  expected.clear();
  AppendBoxToVector(&third_cue_data, &expected);
  AppendBoxToVector(&fourth_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(2000, 2300, expected));

  expected.clear();
  AppendBoxToVector(&third_cue_data, &expected);
  EXPECT_THAT(webvtt_sample_converter_.PopSample(),
              MatchesStartTimeEndTimeAndData(2300, 2500, expected));
}

}  // namespace shaka
}  // namespace media
}  // namespace edash_packager
