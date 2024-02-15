// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/base/container_names.h>

#include <iterator>

#include <gtest/gtest.h>

#include <packager/media/test/test_data_util.h>

namespace shaka {
namespace media {

// Using a macros to simplify tests. Since EXPECT_EQ outputs the second argument
// as a string when it fails, this lets the output identify what item actually
// failed.
#define VERIFY(buffer, name)                                             \
  EXPECT_EQ(name,                                                        \
            DetermineContainer(reinterpret_cast<const uint8_t*>(buffer), \
                               sizeof(buffer)))

// Test that small buffers are handled correctly.
TEST(ContainerNamesTest, CheckSmallBuffer) {
  // Empty buffer.
  char buffer[1] = {'\0'};  // ([0] not allowed on win)
  VERIFY(buffer, CONTAINER_UNKNOWN);

  // Try a simple SRT file.
  char buffer1[] =
      "1\n"
      "00:03:23,550 --> 00:03:24,375\n"
      "You always had a hard time finding your place in this world.\n"
      "\n"
      "2\n"
      "00:03:24,476 --> 00:03:25,175\n"
      "What are you talking about?\n";
  VERIFY(buffer1, CONTAINER_SRT);

  // HLS has it's own loop.
  char buffer2[] = "#EXTM3U"
                   "some other random stuff"
                   "#EXT-X-MEDIA-SEQUENCE:";
  VERIFY(buffer2, CONTAINER_HLS);

  // Try a large buffer all zeros.
  char buffer3[4096];
  memset(buffer3, 0, sizeof(buffer3));
  VERIFY(buffer3, CONTAINER_UNKNOWN);

  // Reuse buffer, but all \n this time.
  memset(buffer3, '\n', sizeof(buffer3));
  VERIFY(buffer3, CONTAINER_UNKNOWN);
}

#define BYTE_ORDER_MARK "\xef\xbb\xbf"

// Note that the comparisons need at least 12 bytes, so make sure the buffer is
// at least that size.
const char kAmrBuffer[12] = "#!AMR";
uint8_t kAsfBuffer[] = {0x30, 0x26, 0xb2, 0x75, 0x8e, 0x66, 0xcf, 0x11,
                        0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c};
const char kAss1Buffer[] = "[Script Info]";
const char kAss2Buffer[] = BYTE_ORDER_MARK "[Script Info]";
uint8_t kCafBuffer[] = {
    'c', 'a', 'f', 'f', 0,   1, 0, 0, 'd', 'e', 's', 'c', 0,   0, 0, 0, 0, 0, 0,
    32,  64,  229, 136, 128, 0, 0, 0, 0,   'a', 'a', 'c', ' ', 0, 0, 0, 2, 0, 0,
    0,   0,   0,   0,   4,   0, 0, 0, 0,   2,   0,   0,   0,   0};
const char kDtshdBuffer[12] = "DTSHDHDR";
const char kDxaBuffer[16] = "DEXA";
const char kFlacBuffer[12] = "fLaC";
uint8_t kFlvBuffer[12] = {'F', 'L', 'V', 0, 0, 0, 0, 1, 0, 0, 0, 0};
uint8_t kIrcamBuffer[] = {0x64, 0xa3, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1};
const char kRm1Buffer[12] = ".RMF\0\0";
const char kRm2Buffer[12] = ".ra\xfd";
uint8_t kWtvBuffer[] = {0xb7, 0xd8, 0x00, 0x20, 0x37, 0x49, 0xda, 0x11,
                        0xa6, 0x4e, 0x00, 0x07, 0xe9, 0x5e, 0xad, 0x8d};
uint8_t kBug263073Buffer[] = {
    0x00, 0x00, 0x00, 0x18, 0x66, 0x74, 0x79, 0x70, 0x6d, 0x70, 0x34,
    0x32, 0x00, 0x00, 0x00, 0x00, 0x69, 0x73, 0x6f, 0x6d, 0x6d, 0x70,
    0x34, 0x32, 0x00, 0x00, 0x00, 0x01, 0x6d, 0x64, 0x61, 0x74, 0x00,
    0x00, 0x00, 0x00, 0xaa, 0x2e, 0x22, 0xcf, 0x00, 0x00, 0x00, 0x37,
    0x67, 0x64, 0x00, 0x28, 0xac, 0x2c, 0xa4, 0x01, 0xe0, 0x08, 0x9f,
    0x97, 0x01, 0x52, 0x02, 0x02, 0x02, 0x80, 0x00, 0x01};

TEST(ContainerNamesTest, FromFormatName) {
  EXPECT_EQ(CONTAINER_WEBM, DetermineContainerFromFormatName("webm"));
  EXPECT_EQ(CONTAINER_WEBM, DetermineContainerFromFormatName("WeBm"));
  EXPECT_EQ(CONTAINER_MOV, DetermineContainerFromFormatName("m4a"));
  EXPECT_EQ(CONTAINER_MOV, DetermineContainerFromFormatName("m4v"));
  EXPECT_EQ(CONTAINER_MOV, DetermineContainerFromFormatName("M4v"));
  EXPECT_EQ(CONTAINER_MOV, DetermineContainerFromFormatName("m4s"));
  EXPECT_EQ(CONTAINER_MOV, DetermineContainerFromFormatName("mov"));
  EXPECT_EQ(CONTAINER_MOV, DetermineContainerFromFormatName("mp4"));
  EXPECT_EQ(CONTAINER_MOV, DetermineContainerFromFormatName("Mp4"));
  EXPECT_EQ(CONTAINER_MOV, DetermineContainerFromFormatName("cmfa"));
  EXPECT_EQ(CONTAINER_MOV, DetermineContainerFromFormatName("cmft"));
  EXPECT_EQ(CONTAINER_MOV, DetermineContainerFromFormatName("cmfv"));
  EXPECT_EQ(CONTAINER_MPEG2TS, DetermineContainerFromFormatName("ts"));
  EXPECT_EQ(CONTAINER_MPEG2TS, DetermineContainerFromFormatName("mpeg2ts"));
  EXPECT_EQ(CONTAINER_UNKNOWN, DetermineContainerFromFormatName("cat"));
  EXPECT_EQ(CONTAINER_UNKNOWN, DetermineContainerFromFormatName("amp4"));
  EXPECT_EQ(CONTAINER_UNKNOWN, DetermineContainerFromFormatName(" mp4"));
  EXPECT_EQ(CONTAINER_UNKNOWN, DetermineContainerFromFormatName(""));
}

TEST(ContainerNamesTest, FromFileName) {
  EXPECT_EQ(CONTAINER_WEBM, DetermineContainerFromFileName("test.webm"));
  EXPECT_EQ(CONTAINER_WEBM, DetermineContainerFromFileName("another.wEbM"));
  EXPECT_EQ(CONTAINER_MOV, DetermineContainerFromFileName("test.m4a"));
  EXPECT_EQ(CONTAINER_MOV, DetermineContainerFromFileName("file.m4v"));
  EXPECT_EQ(CONTAINER_MOV, DetermineContainerFromFileName("a file .m4V"));
  EXPECT_EQ(CONTAINER_MOV, DetermineContainerFromFileName("segment.m4s"));
  EXPECT_EQ(CONTAINER_MOV, DetermineContainerFromFileName("2_more-files.mp4"));
  EXPECT_EQ(CONTAINER_MOV, DetermineContainerFromFileName("foo.bar.MP4"));
  EXPECT_EQ(CONTAINER_MPEG2TS, DetermineContainerFromFileName("a.ts"));
  EXPECT_EQ(CONTAINER_MPEG2TS, DetermineContainerFromFileName("a.TS"));
  EXPECT_EQ(CONTAINER_UNKNOWN, DetermineContainerFromFileName("a_bad.gif"));
  EXPECT_EQ(CONTAINER_UNKNOWN, DetermineContainerFromFileName("a bad.m4v-"));
  EXPECT_EQ(CONTAINER_UNKNOWN, DetermineContainerFromFileName("a.m4v."));
  EXPECT_EQ(CONTAINER_UNKNOWN, DetermineContainerFromFileName(""));
}

// Test that containers that start with fixed strings are handled correctly.
// This is to verify that the TAG matches the first 4 characters of the string.
TEST(ContainerNamesTest, CheckFixedStrings) {
  VERIFY(kAmrBuffer, CONTAINER_AMR);
  VERIFY(kAsfBuffer, CONTAINER_ASF);
  VERIFY(kAss1Buffer, CONTAINER_ASS);
  VERIFY(kAss2Buffer, CONTAINER_ASS);
  VERIFY(kCafBuffer, CONTAINER_CAF);
  VERIFY(kDtshdBuffer, CONTAINER_DTSHD);
  VERIFY(kDxaBuffer, CONTAINER_DXA);
  VERIFY(kFlacBuffer, CONTAINER_FLAC);
  VERIFY(kFlvBuffer, CONTAINER_FLV);
  VERIFY(kIrcamBuffer, CONTAINER_IRCAM);
  VERIFY(kRm1Buffer, CONTAINER_RM);
  VERIFY(kRm2Buffer, CONTAINER_RM);
  VERIFY(kWtvBuffer, CONTAINER_WTV);
  VERIFY(kBug263073Buffer, CONTAINER_MOV);
}

// Determine the container type of a specified file.
void TestFile(MediaContainerName expected, const std::string& name) {
  auto path = GetTestDataFilePath(name);
  std::vector<uint8_t> data = ReadTestDataFile(name);
  ASSERT_FALSE(data.empty());

  // Now verify the type.
  EXPECT_EQ(expected, DetermineContainer(data.data(), data.size()))
      << "Failure with file " << path;
}

TEST(ContainerNamesTest, Ttml) {
  // One of the actual TTMLs from the TTML spec page.
  const char kTtml[] =
      "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
      "<tt xml:lang=\"en\" xmlns=\"http://www.w3.org/ns/ttml\">\n"
      "  <body>\n"
      "    <div>\n"
      "      <p dur=\"10s\">\n"
      "        Some subtitle.\n"
      "      </p>\n"
      "    </div>\n"
      "  </body>\n"
      "</tt>\n";

  EXPECT_EQ(CONTAINER_TTML,
            DetermineContainer(reinterpret_cast<const uint8_t*>(kTtml),
                               std::size(kTtml)));
}

TEST(ContainerNamesTest, WebVtt) {
  const char kWebVtt[] =
      "WEBVTT\n"
      "\n"
      "00:1.000 --> 00:2.000\n"
      "Subtitle";
  EXPECT_EQ(CONTAINER_WEBVTT,
            DetermineContainer(reinterpret_cast<const uint8_t*>(kWebVtt),
                               std::size(kWebVtt)));

  const uint8_t kUtf8ByteOrderMark[] = {0xef, 0xbb, 0xbf};
  std::vector<uint8_t> webvtt_with_utf8_byte_order_mark(
      kUtf8ByteOrderMark, kUtf8ByteOrderMark + std::size(kUtf8ByteOrderMark));
  webvtt_with_utf8_byte_order_mark.insert(
      webvtt_with_utf8_byte_order_mark.end(), kWebVtt,
      kWebVtt + std::size(kWebVtt));

  EXPECT_EQ(CONTAINER_WEBVTT,
            DetermineContainer(
                webvtt_with_utf8_byte_order_mark.data(),
                static_cast<int>(webvtt_with_utf8_byte_order_mark.size())));
}

TEST(ContainerNamesTest, FileCheckOGG) {
  TestFile(CONTAINER_OGG, "bear.ogv");
  TestFile(CONTAINER_OGG, "9ch.ogg");
}

TEST(ContainerNamesTest, FileCheckWAV) {
  TestFile(CONTAINER_WAV, "4ch.wav");
}

TEST(ContainerNamesTest, FileCheckMOV) {
  TestFile(CONTAINER_MOV, "bear-640x360.mp4");
}

TEST(ContainerNamesTest, FileCheckWEBM) {
  TestFile(CONTAINER_WEBM, "bear-640x360.webm");
  TestFile(CONTAINER_WEBM, "no_streams.webm");
}

TEST(ContainerNamesTest, FileCheckMP3) {
  TestFile(CONTAINER_MP3, "id3_test.mp3");
}

TEST(ContainerNamesTest, FileCheckAC3) {
  TestFile(CONTAINER_AC3, "bear.ac3");
}

TEST(ContainerNamesTest, FileCheckAAC) {
  TestFile(CONTAINER_AAC, "bear.adts");
}

TEST(ContainerNamesTest, FileCheckAIFF) {
  TestFile(CONTAINER_AIFF, "bear.aiff");
}

TEST(ContainerNamesTest, FileCheckASF) {
  TestFile(CONTAINER_ASF, "bear.asf");
}

TEST(ContainerNamesTest, FileCheckAVI) {
  TestFile(CONTAINER_AVI, "bear.avi");
}

TEST(ContainerNamesTest, FileCheckEAC3) {
  TestFile(CONTAINER_EAC3, "bear.eac3");
}

TEST(ContainerNamesTest, FileCheckFLAC) {
  TestFile(CONTAINER_FLAC, "bear.flac");
}

TEST(ContainerNamesTest, FileCheckFLV) {
  TestFile(CONTAINER_FLV, "bear.flv");
}

TEST(ContainerNamesTest, FileCheckH261) {
  TestFile(CONTAINER_H261, "bear.h261");
}

TEST(ContainerNamesTest, FileCheckH263) {
  TestFile(CONTAINER_H263, "bear.h263");
}

TEST(ContainerNamesTest, FileCheckMJPEG) {
  TestFile(CONTAINER_MJPEG, "bear.mjpeg");
}

TEST(ContainerNamesTest, FileCheckMPEG2PS) {
  TestFile(CONTAINER_MPEG2PS, "bear.mpeg");
}

TEST(ContainerNamesTest, FileCheckMPEG2TS) {
  TestFile(CONTAINER_MPEG2TS, "bear.m2ts");
}

TEST(ContainerNamesTest, FileCheckRM) {
  TestFile(CONTAINER_RM, "bear.rm");
}

TEST(ContainerNamesTest, FileCheckSWF) {
  TestFile(CONTAINER_SWF, "bear.swf");
}

// Try a few non containers.
TEST(ContainerNamesTest, FileCheckUNKNOWN) {
  TestFile(CONTAINER_UNKNOWN, "ten_byte_file");
  TestFile(CONTAINER_UNKNOWN, "README");
}

}  // namespace media
}  // namespace shaka
