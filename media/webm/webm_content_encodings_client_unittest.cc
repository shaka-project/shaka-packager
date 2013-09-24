// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "media/webm/webm_constants.h"
#include "media/webm/webm_content_encodings_client.h"
#include "media/webm/webm_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class WebMContentEncodingsClientTest : public testing::Test {
 public:
  WebMContentEncodingsClientTest()
      : client_(LogCB()),
        parser_(kWebMIdContentEncodings, &client_) {}

  void ParseAndExpectToFail(const uint8* buf, int size) {
    int result = parser_.Parse(buf, size);
    EXPECT_EQ(-1, result);
  }

 protected:
  WebMContentEncodingsClient client_;
  WebMListParser parser_;
};

TEST_F(WebMContentEncodingsClientTest, EmptyContentEncodings) {
  const uint8 kContentEncodings[] = {
    0x6D, 0x80, 0x80,  // ContentEncodings (size = 0)
  };
  int size = sizeof(kContentEncodings);
  ParseAndExpectToFail(kContentEncodings, size);
}

TEST_F(WebMContentEncodingsClientTest, EmptyContentEncoding) {
  const uint8 kContentEncodings[] = {
    0x6D, 0x80, 0x83,  // ContentEncodings (size = 3)
    0x63, 0x40, 0x80,  //   ContentEncoding (size = 0)
  };
  int size = sizeof(kContentEncodings);
  ParseAndExpectToFail(kContentEncodings, size);
}

TEST_F(WebMContentEncodingsClientTest, SingleContentEncoding) {
  const uint8 kContentEncodings[] = {
    0x6D, 0x80, 0xA1,        // ContentEncodings (size = 33)
    0x62, 0x40, 0x9e,        //   ContentEncoding (size = 30)
    0x50, 0x31, 0x81, 0x00,  //     ContentEncodingOrder (size = 1)
    0x50, 0x32, 0x81, 0x01,  //     ContentEncodingScope (size = 1)
    0x50, 0x33, 0x81, 0x01,  //     ContentEncodingType (size = 1)
    0x50, 0x35, 0x8F,        //     ContentEncryption (size = 15)
    0x47, 0xE1, 0x81, 0x05,  //       ContentEncAlgo (size = 1)
    0x47, 0xE2, 0x88,        //       ContentEncKeyID (size = 8)
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
  };
  int size = sizeof(kContentEncodings);

  int result = parser_.Parse(kContentEncodings, size);
  ASSERT_EQ(size, result);
  const ContentEncodings& content_encodings = client_.content_encodings();

  ASSERT_EQ(1u, content_encodings.size());
  ASSERT_TRUE(content_encodings[0]);
  EXPECT_EQ(0, content_encodings[0]->order());
  EXPECT_EQ(ContentEncoding::kScopeAllFrameContents,
            content_encodings[0]->scope());
  EXPECT_EQ(ContentEncoding::kTypeEncryption, content_encodings[0]->type());
  EXPECT_EQ(ContentEncoding::kEncAlgoAes,
            content_encodings[0]->encryption_algo());
  EXPECT_EQ(8u, content_encodings[0]->encryption_key_id().size());
}

TEST_F(WebMContentEncodingsClientTest, MultipleContentEncoding) {
  const uint8 kContentEncodings[] = {
    0x6D, 0x80, 0xC2,        // ContentEncodings (size = 66)
    0x62, 0x40, 0x9e,        //   ContentEncoding (size = 30)
    0x50, 0x31, 0x81, 0x00,  //     ContentEncodingOrder (size = 1)
    0x50, 0x32, 0x81, 0x03,  //     ContentEncodingScope (size = 1)
    0x50, 0x33, 0x81, 0x01,  //     ContentEncodingType (size = 1)
    0x50, 0x35, 0x8F,        //     ContentEncryption (size = 15)
    0x47, 0xE1, 0x81, 0x05,  //       ContentEncAlgo (size = 1)
    0x47, 0xE2, 0x88,        //       ContentEncKeyID (size = 8)
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
    0x62, 0x40, 0x9e,        //   ContentEncoding (size = 30)
    0x50, 0x31, 0x81, 0x01,  //     ContentEncodingOrder (size = 1)
    0x50, 0x32, 0x81, 0x03,  //     ContentEncodingScope (size = 1)
    0x50, 0x33, 0x81, 0x01,  //     ContentEncodingType (size = 1)
    0x50, 0x35, 0x8F,        //     ContentEncryption (size = 15)
    0x47, 0xE1, 0x81, 0x01,  //       ContentEncAlgo (size = 1)
    0x47, 0xE2, 0x88,        //       ContentEncKeyID (size = 8)
    0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB,
  };
  int size = sizeof(kContentEncodings);

  int result = parser_.Parse(kContentEncodings, size);
  ASSERT_EQ(size, result);
  const ContentEncodings& content_encodings = client_.content_encodings();
  ASSERT_EQ(2u, content_encodings.size());

  for (int i = 0; i < 2; ++i) {
    ASSERT_TRUE(content_encodings[i]);
    EXPECT_EQ(i, content_encodings[i]->order());
    EXPECT_EQ(ContentEncoding::kScopeAllFrameContents |
              ContentEncoding::kScopeTrackPrivateData,
              content_encodings[i]->scope());
    EXPECT_EQ(ContentEncoding::kTypeEncryption, content_encodings[i]->type());
    EXPECT_EQ(!i ? ContentEncoding::kEncAlgoAes : ContentEncoding::kEncAlgoDes,
              content_encodings[i]->encryption_algo());
    EXPECT_EQ(8u, content_encodings[i]->encryption_key_id().size());
  }
}

TEST_F(WebMContentEncodingsClientTest, DefaultValues) {
  const uint8 kContentEncodings[] = {
    0x6D, 0x80, 0x8A,        // ContentEncodings (size = 10)
    0x62, 0x40, 0x87,        //   ContentEncoding (size = 7)
                             //     ContentEncodingOrder missing
                             //     ContentEncodingScope missing
    0x50, 0x33, 0x81, 0x01,  //     ContentEncodingType (size = 1)
    0x50, 0x35, 0x80,        //     ContentEncryption (size = 0)
                             //     ContentEncAlgo missing
  };
  int size = sizeof(kContentEncodings);

  int result = parser_.Parse(kContentEncodings, size);
  ASSERT_EQ(size, result);
  const ContentEncodings& content_encodings = client_.content_encodings();

  ASSERT_EQ(1u, content_encodings.size());
  ASSERT_TRUE(content_encodings[0]);
  EXPECT_EQ(0, content_encodings[0]->order());
  EXPECT_EQ(ContentEncoding::kScopeAllFrameContents,
            content_encodings[0]->scope());
  EXPECT_EQ(ContentEncoding::kTypeEncryption, content_encodings[0]->type());
  EXPECT_EQ(ContentEncoding::kEncAlgoNotEncrypted,
            content_encodings[0]->encryption_algo());
  EXPECT_TRUE(content_encodings[0]->encryption_key_id().empty());
}

TEST_F(WebMContentEncodingsClientTest, ContentEncodingsClientReuse) {
  const uint8 kContentEncodings[] = {
    0x6D, 0x80, 0xA1,        // ContentEncodings (size = 33)
    0x62, 0x40, 0x9e,        //   ContentEncoding (size = 30)
    0x50, 0x31, 0x81, 0x00,  //     ContentEncodingOrder (size = 1)
    0x50, 0x32, 0x81, 0x01,  //     ContentEncodingScope (size = 1)
    0x50, 0x33, 0x81, 0x01,  //     ContentEncodingType (size = 1)
    0x50, 0x35, 0x8F,        //     ContentEncryption (size = 15)
    0x47, 0xE1, 0x81, 0x05,  //       ContentEncAlgo (size = 1)
    0x47, 0xE2, 0x88,        //       ContentEncKeyID (size = 8)
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
  };
  int size = sizeof(kContentEncodings);

  // Parse for the first time.
  int result = parser_.Parse(kContentEncodings, size);
  ASSERT_EQ(size, result);

  // Parse again.
  parser_.Reset();
  result = parser_.Parse(kContentEncodings, size);
  ASSERT_EQ(size, result);
  const ContentEncodings& content_encodings = client_.content_encodings();

  ASSERT_EQ(1u, content_encodings.size());
  ASSERT_TRUE(content_encodings[0]);
  EXPECT_EQ(0, content_encodings[0]->order());
  EXPECT_EQ(ContentEncoding::kScopeAllFrameContents,
            content_encodings[0]->scope());
  EXPECT_EQ(ContentEncoding::kTypeEncryption, content_encodings[0]->type());
  EXPECT_EQ(ContentEncoding::kEncAlgoAes,
            content_encodings[0]->encryption_algo());
  EXPECT_EQ(8u, content_encodings[0]->encryption_key_id().size());
}

TEST_F(WebMContentEncodingsClientTest, InvalidContentEncodingOrder) {
  const uint8 kContentEncodings[] = {
    0x6D, 0x80, 0x8E,        // ContentEncodings (size = 14)
    0x62, 0x40, 0x8B,        //   ContentEncoding (size = 11)
    0x50, 0x31, 0x81, 0xEE,  //     ContentEncodingOrder (size = 1), invalid
    0x50, 0x33, 0x81, 0x01,  //     ContentEncodingType (size = 1)
    0x50, 0x35, 0x80,        //     ContentEncryption (size = 0)
  };
  int size = sizeof(kContentEncodings);
  ParseAndExpectToFail(kContentEncodings, size);
}

TEST_F(WebMContentEncodingsClientTest, InvalidContentEncodingScope) {
  const uint8 kContentEncodings[] = {
    0x6D, 0x80, 0x8E,        // ContentEncodings (size = 14)
    0x62, 0x40, 0x8B,        //   ContentEncoding (size = 11)
    0x50, 0x32, 0x81, 0xEE,  //     ContentEncodingScope (size = 1), invalid
    0x50, 0x33, 0x81, 0x01,  //     ContentEncodingType (size = 1)
    0x50, 0x35, 0x80,        //     ContentEncryption (size = 0)
  };
  int size = sizeof(kContentEncodings);
  ParseAndExpectToFail(kContentEncodings, size);
}

TEST_F(WebMContentEncodingsClientTest, InvalidContentEncodingType) {
  const uint8 kContentEncodings[] = {
    0x6D, 0x80, 0x8E,        // ContentEncodings (size = 14)
    0x62, 0x40, 0x8B,        //   ContentEncoding (size = 11)
    0x50, 0x33, 0x81, 0x00,  //     ContentEncodingType (size = 1), invalid
    0x50, 0x35, 0x80,        //     ContentEncryption (size = 0)
  };
  int size = sizeof(kContentEncodings);
  ParseAndExpectToFail(kContentEncodings, size);
}

// ContentEncodingType is encryption but no ContentEncryption present.
TEST_F(WebMContentEncodingsClientTest, MissingContentEncryption) {
  const uint8 kContentEncodings[] = {
    0x6D, 0x80, 0x87,        // ContentEncodings (size = 7)
    0x62, 0x40, 0x84,        //   ContentEncoding (size = 4)
    0x50, 0x33, 0x81, 0x01,  //     ContentEncodingType (size = 1)
                             //     ContentEncryption missing
  };
  int size = sizeof(kContentEncodings);
  ParseAndExpectToFail(kContentEncodings, size);
}

TEST_F(WebMContentEncodingsClientTest, InvalidContentEncAlgo) {
  const uint8 kContentEncodings[] = {
    0x6D, 0x80, 0x99,        // ContentEncodings (size = 25)
    0x62, 0x40, 0x96,        //   ContentEncoding (size = 22)
    0x50, 0x33, 0x81, 0x01,  //     ContentEncodingType (size = 1)
    0x50, 0x35, 0x8F,        //     ContentEncryption (size = 15)
    0x47, 0xE1, 0x81, 0xEE,  //       ContentEncAlgo (size = 1), invalid
    0x47, 0xE2, 0x88,        //       ContentEncKeyID (size = 8)
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
  };
  int size = sizeof(kContentEncodings);
  ParseAndExpectToFail(kContentEncodings, size);
}

}  // namespace media
