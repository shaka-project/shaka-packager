// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/base/buffer_writer.h"

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/buffer_reader.h"
#include "media/base/status_test_util.h"
#include "media/file/file.h"

namespace {
const int kReservedBufferCapacity = 1000;
// Min values for various integers of different size. Min values for signed
// integers are already defined in //base/basictypes.h.
const uint8 kuint8min = 0;
const uint16 kuint16min = 0;
const uint32 kuint32min = 0;
const uint64 kuint64min = 0;
// Max values for various integers are already defined in //base/basictypes.h.
// Other integer values.
const uint8 kuint8 = 10;
const uint16 kuint16 = 1000;
const int16 kint16 = -1000;
const uint32 kuint32 = 1000000;
const int32 kint32 = -1000000;
const uint64 kuint64 = 10000000000;
const int64 kint64 = -10000000000;
const uint8 kuint8Array[] = {10, 1, 100, 5, 3, 60};
}  // namespace

namespace media {

class BufferWriterTest : public testing::Test {
 public:
  BufferWriterTest()
      : writer_(new BufferWriter(kReservedBufferCapacity)),
        reader_(new BufferReader(writer_->Buffer(), kReservedBufferCapacity)) {}

  bool ReadInt(uint8* v) { return reader_->Read1(v); }
  bool ReadInt(uint16* v) { return reader_->Read2(v); }
  bool ReadInt(int16* v) { return reader_->Read2s(v); }
  bool ReadInt(uint32* v) { return reader_->Read4(v); }
  bool ReadInt(int32* v) { return reader_->Read4s(v); }
  bool ReadInt(uint64* v) { return reader_->Read8(v); }
  bool ReadInt(int64* v) { return reader_->Read8s(v); }

  template <typename T>
  void ReadAndExpect(T expectation) {
    T data_read;
    ASSERT_TRUE(ReadInt(&data_read));
    ASSERT_EQ(expectation, data_read);
  }

  template <typename T>
  void Verify(T min, T max, T val) {
    writer_->AppendInt(min);
    writer_->AppendInt(max);
    writer_->AppendInt(val);
    ASSERT_EQ(sizeof(min) + sizeof(max) + sizeof(val), writer_->Size());
    ReadAndExpect(min);
    ReadAndExpect(max);
    ReadAndExpect(val);
  }

 protected:
  scoped_ptr<BufferWriter> writer_;
  scoped_ptr<BufferReader> reader_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BufferWriterTest);
};

TEST_F(BufferWriterTest, Append1) { Verify(kuint8min, kuint8max, kuint8); }
TEST_F(BufferWriterTest, Append2) { Verify(kuint16min, kuint16max, kuint16); }
TEST_F(BufferWriterTest, Append2s) { Verify(kint16min, kint16max, kint16); }
TEST_F(BufferWriterTest, Append4) { Verify(kuint32min, kuint32max, kuint32); }
TEST_F(BufferWriterTest, Append4s) { Verify(kint32min, kint32max, kint32); }
TEST_F(BufferWriterTest, Append8) { Verify(kuint64min, kuint64max, kuint64); }
TEST_F(BufferWriterTest, Append8s) { Verify(kint64min, kint64max, kint64); }

TEST_F(BufferWriterTest, AppendNBytes) {
  // Write the least significant four bytes and verify the result.
  writer_->AppendNBytes(kuint64, sizeof(uint32));
  ASSERT_EQ(sizeof(uint32), writer_->Size());
  ReadAndExpect(static_cast<uint32>(kuint64 & 0xFFFFFFFF));
}

TEST_F(BufferWriterTest, AppendEmptyVector) {
  std::vector<uint8> v;
  writer_->AppendVector(v);
  ASSERT_EQ(0, writer_->Size());
}

TEST_F(BufferWriterTest, AppendVector) {
  std::vector<uint8> v(kuint8Array, kuint8Array + sizeof(kuint8Array));
  writer_->AppendVector(v);
  ASSERT_EQ(sizeof(kuint8Array), writer_->Size());

  std::vector<uint8> data_read;
  ASSERT_TRUE(reader_->ReadToVector(&data_read, sizeof(kuint8Array)));
  ASSERT_EQ(v, data_read);
}

TEST_F(BufferWriterTest, AppendArray) {
  writer_->AppendArray(kuint8Array, sizeof(kuint8Array));
  ASSERT_EQ(sizeof(kuint8Array), writer_->Size());

  std::vector<uint8> data_read;
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

  reader_.reset(new BufferReader(writer_->Buffer(), writer_->Size()));
  ASSERT_NO_FATAL_FAILURE(ReadAndExpect(kuint16));
  ASSERT_NO_FATAL_FAILURE(ReadAndExpect(kint64));
}

TEST_F(BufferWriterTest, Clear) {
  writer_->AppendInt(kuint32);
  ASSERT_EQ(sizeof(kuint32), writer_->Size());
  writer_->Clear();
  ASSERT_EQ(0, writer_->Size());
}

TEST_F(BufferWriterTest, WriteToFile) {
  base::FilePath path;
  ASSERT_TRUE(file_util::CreateTemporaryFile(&path));
  LOG(INFO) << "Created temporary file: " << path.value();

  // Append an array to buffer and then write to the temporary file.
  File* const output_file = File::Open(path.value().c_str(), "w");
  writer_->AppendArray(kuint8Array, sizeof(kuint8Array));
  ASSERT_EQ(sizeof(kuint8Array), writer_->Size());
  ASSERT_OK(writer_->WriteToFile(output_file));
  ASSERT_EQ(0, writer_->Size());
  ASSERT_TRUE(output_file->Close());

  // Read the file and verify.
  File* const input_file = File::Open(path.value().c_str(), "r");
  ASSERT_TRUE(input_file != NULL);
  std::vector<uint8> data_read(sizeof(kuint8Array), 0);
  EXPECT_EQ(sizeof(kuint8Array),
            input_file->Read(&data_read[0], data_read.size()));
  ASSERT_TRUE(input_file->Close());

  for (size_t i = 0; i < sizeof(kuint8Array); ++i)
    EXPECT_EQ(kuint8Array[i], data_read[i]);
}

}  // namespace media
