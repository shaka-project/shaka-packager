// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/buffer_writer.h>

#include <filesystem>
#include <limits>
#include <memory>

#include <absl/log/log.h>

#include <packager/file.h>
#include <packager/file/file_test_util.h>
#include <packager/macros/classes.h>
#include <packager/media/base/buffer_reader.h>
#include <packager/status/status_test_util.h>

namespace {
const int kReservedBufferCapacity = 1000;
const uint8_t kuint8 = 10;
const uint16_t kuint16 = 1000;
const int16_t kint16 = -1000;
const uint32_t kuint32 = 1000000;
const int32_t kint32 = -1000000;
const uint64_t kuint64 = 10000000000ULL;
const int64_t kint64 = -10000000000LL;
const uint8_t kuint8Array[] = {10, 1, 100, 5, 3, 60};
}  // namespace

namespace shaka {
namespace media {

class BufferWriterTest : public testing::Test {
 public:
  BufferWriterTest() : writer_(new BufferWriter(kReservedBufferCapacity)) {}

  void CreateReader() {
    reader_.reset(new BufferReader(writer_->Buffer(), writer_->Size()));
  }

  bool ReadInt(uint8_t* v) { return reader_->Read1(v); }
  bool ReadInt(uint16_t* v) { return reader_->Read2(v); }
  bool ReadInt(int16_t* v) { return reader_->Read2s(v); }
  bool ReadInt(uint32_t* v) { return reader_->Read4(v); }
  bool ReadInt(int32_t* v) { return reader_->Read4s(v); }
  bool ReadInt(uint64_t* v) { return reader_->Read8(v); }
  bool ReadInt(int64_t* v) { return reader_->Read8s(v); }

  template <typename T>
  void ReadAndExpect(T expectation) {
    T data_read;
    ASSERT_TRUE(ReadInt(&data_read));
    ASSERT_EQ(expectation, data_read);
  }

  template <typename T>
  void Verify(T val) {
    T min = std::numeric_limits<T>::min();
    T max = std::numeric_limits<T>::max();

    writer_->AppendInt(min);
    writer_->AppendInt(max);
    writer_->AppendInt(val);
    ASSERT_EQ(sizeof(min) + sizeof(max) + sizeof(val), writer_->Size());

    CreateReader();
    ReadAndExpect(min);
    ReadAndExpect(max);
    ReadAndExpect(val);
  }

 protected:
  std::unique_ptr<BufferWriter> writer_;
  std::unique_ptr<BufferReader> reader_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BufferWriterTest);
};

TEST_F(BufferWriterTest, Append1) { Verify(kuint8); }
TEST_F(BufferWriterTest, Append2) { Verify(kuint16); }
TEST_F(BufferWriterTest, Append2s) { Verify(kint16); }
TEST_F(BufferWriterTest, Append4) { Verify(kuint32); }
TEST_F(BufferWriterTest, Append4s) { Verify(kint32); }
TEST_F(BufferWriterTest, Append8) { Verify(kuint64); }
TEST_F(BufferWriterTest, Append8s) { Verify(kint64); }

TEST_F(BufferWriterTest, AppendNBytes) {
  // Write the least significant four bytes and verify the result.
  writer_->AppendNBytes(kuint64, sizeof(uint32_t));
  ASSERT_EQ(sizeof(uint32_t), writer_->Size());

  CreateReader();
  ReadAndExpect(static_cast<uint32_t>(kuint64 & 0xFFFFFFFF));
}

TEST_F(BufferWriterTest, AppendEmptyVector) {
  std::vector<uint8_t> v;
  writer_->AppendVector(v);
  ASSERT_EQ(0u, writer_->Size());
}

TEST_F(BufferWriterTest, AppendVector) {
  std::vector<uint8_t> v(kuint8Array, kuint8Array + sizeof(kuint8Array));
  writer_->AppendVector(v);
  ASSERT_EQ(sizeof(kuint8Array), writer_->Size());

  CreateReader();
  std::vector<uint8_t> data_read;
  ASSERT_TRUE(reader_->ReadToVector(&data_read, sizeof(kuint8Array)));
  ASSERT_EQ(v, data_read);
}

TEST_F(BufferWriterTest, AppendString) {
  const char kTestData[] = "test_data";
  writer_->AppendString(kTestData);
  // -1 to remove the null terminating character.
  ASSERT_EQ(strlen(kTestData), writer_->Size());

  CreateReader();
  std::string data_read;
  ASSERT_TRUE(reader_->ReadToString(&data_read, strlen(kTestData)));
  ASSERT_EQ(kTestData, data_read);
}

TEST_F(BufferWriterTest, AppendArray) {
  writer_->AppendArray(kuint8Array, sizeof(kuint8Array));
  ASSERT_EQ(sizeof(kuint8Array), writer_->Size());

  CreateReader();
  std::vector<uint8_t> data_read;
  ASSERT_TRUE(reader_->ReadToVector(&data_read, sizeof(kuint8Array)));
  for (size_t i = 0; i < sizeof(kuint8Array); ++i)
    EXPECT_EQ(kuint8Array[i], data_read[i]);
}

TEST_F(BufferWriterTest, AppendBufferWriter) {
  BufferWriter local_writer;
  local_writer.AppendInt(kuint16);
  local_writer.AppendInt(kint64);
  local_writer.AppendInt(kuint32);
  writer_->AppendBuffer(local_writer);
  ASSERT_EQ(sizeof(kuint16) + sizeof(kint64) + sizeof(kuint32),
            writer_->Size());

  CreateReader();
  ASSERT_NO_FATAL_FAILURE(ReadAndExpect(kuint16));
  ASSERT_NO_FATAL_FAILURE(ReadAndExpect(kint64));
  ASSERT_NO_FATAL_FAILURE(ReadAndExpect(kuint32));
}

TEST_F(BufferWriterTest, Swap) {
  BufferWriter local_writer;
  local_writer.AppendInt(kuint16);
  local_writer.AppendInt(kint64);
  writer_->AppendInt(kuint32);
  writer_->Swap(&local_writer);

  ASSERT_EQ(sizeof(kuint16) + sizeof(kint64), writer_->Size());
  ASSERT_EQ(sizeof(kuint32), local_writer.Size());

  CreateReader();
  ASSERT_NO_FATAL_FAILURE(ReadAndExpect(kuint16));
  ASSERT_NO_FATAL_FAILURE(ReadAndExpect(kint64));
}

TEST_F(BufferWriterTest, Clear) {
  writer_->AppendInt(kuint32);
  ASSERT_EQ(sizeof(kuint32), writer_->Size());
  writer_->Clear();
  ASSERT_EQ(0u, writer_->Size());
}

TEST_F(BufferWriterTest, WriteToFile) {
  TempFile temp_file;
  LOG(INFO) << "Created temporary file: " << temp_file.path();

  // Append an array to buffer and then write to the temporary file.
  File* const output_file = File::Open(temp_file.path().c_str(), "w");
  writer_->AppendArray(kuint8Array, sizeof(kuint8Array));
  ASSERT_EQ(sizeof(kuint8Array), writer_->Size());
  ASSERT_OK(writer_->WriteToFile(output_file));
  ASSERT_EQ(0u, writer_->Size());
  ASSERT_TRUE(output_file->Close());

  // Read the file and verify.
  File* const input_file = File::Open(temp_file.path().c_str(), "r");
  ASSERT_TRUE(input_file != NULL);
  std::vector<uint8_t> data_read(sizeof(kuint8Array), 0);
  EXPECT_EQ(sizeof(kuint8Array), static_cast<size_t>(input_file->Read(
                                     &data_read[0], data_read.size())));
  ASSERT_TRUE(input_file->Close());

  for (size_t i = 0; i < sizeof(kuint8Array); ++i)
    EXPECT_EQ(kuint8Array[i], data_read[i]);
}

}  // namespace media
}  // namespace shaka
