// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/file/memory_file.h>

#include <memory>

#include <gtest/gtest.h>

#include <packager/file.h>
#include <packager/file/file_closer.h>

namespace shaka {
namespace {

const uint8_t kWriteBuffer[] = {1, 2, 3, 4, 5, 6, 7, 8};
const int64_t kWriteBufferSize = sizeof(kWriteBuffer);

}  // namespace

class MemoryFileTest : public testing::Test {
 protected:
  void TearDown() override { MemoryFile::DeleteAll(); }
};

TEST_F(MemoryFileTest, ModifiesSameFile) {
  std::unique_ptr<File, FileCloser> writer(File::Open("memory://file1", "w"));
  ASSERT_TRUE(writer);
  ASSERT_EQ(kWriteBufferSize, writer->Write(kWriteBuffer, kWriteBufferSize));
  writer.release()->Close();

  // Since File::Open should not create a ThreadedIoFile so there should be
  // no cache.

  std::unique_ptr<File, FileCloser> reader(File::Open("memory://file1", "r"));
  ASSERT_TRUE(reader);

  uint8_t read_buffer[kWriteBufferSize];
  ASSERT_EQ(kWriteBufferSize, reader->Read(read_buffer, kWriteBufferSize));
  EXPECT_EQ(0, memcmp(kWriteBuffer, read_buffer, kWriteBufferSize));
}

TEST_F(MemoryFileTest, SupportsDifferentFiles) {
  std::unique_ptr<File, FileCloser> writer(File::Open("memory://file1", "w"));
  std::unique_ptr<File, FileCloser> reader(File::Open("memory://file2", "w"));
  ASSERT_TRUE(writer);
  ASSERT_TRUE(reader);

  ASSERT_EQ(kWriteBufferSize, writer->Write(kWriteBuffer, kWriteBufferSize));
  ASSERT_EQ(0, reader->Size());
}

TEST_F(MemoryFileTest, SeekAndTell) {
  std::unique_ptr<File, FileCloser> file(File::Open("memory://file1", "w"));
  ASSERT_TRUE(file);

  ASSERT_EQ(kWriteBufferSize, file->Write(kWriteBuffer, kWriteBufferSize));
  ASSERT_TRUE(file->Seek(0));

  const int64_t seek_pos = kWriteBufferSize / 2;
  ASSERT_TRUE(file->Seek(seek_pos));

  uint64_t size;
  ASSERT_TRUE(file->Tell(&size));
  EXPECT_EQ(seek_pos, static_cast<int64_t>(size));
}

TEST_F(MemoryFileTest, EndOfFile) {
  std::unique_ptr<File, FileCloser> file(File::Open("memory://file1", "w"));
  ASSERT_TRUE(file);

  ASSERT_EQ(kWriteBufferSize, file->Write(kWriteBuffer, kWriteBufferSize));
  ASSERT_TRUE(file->Seek(0));

  uint8_t read_buffer[kWriteBufferSize];
  const int64_t seek_pos = kWriteBufferSize / 2;
  const int64_t read_size = kWriteBufferSize - seek_pos;
  ASSERT_TRUE(file->Seek(seek_pos));
  EXPECT_EQ(read_size, file->Read(read_buffer, kWriteBufferSize));
  EXPECT_EQ(0, memcmp(read_buffer, kWriteBuffer + seek_pos, read_size));
  EXPECT_EQ(0, file->Read(read_buffer, kWriteBufferSize));
}

TEST_F(MemoryFileTest, ExtendsSize) {
  std::unique_ptr<File, FileCloser> file(File::Open("memory://file1", "w"));
  ASSERT_TRUE(file);
  ASSERT_EQ(kWriteBufferSize, file->Write(kWriteBuffer, kWriteBufferSize));

  ASSERT_EQ(kWriteBufferSize, file->Size());
  ASSERT_EQ(kWriteBufferSize, file->Write(kWriteBuffer, kWriteBufferSize));
  EXPECT_EQ(2 * kWriteBufferSize, file->Size());

  uint64_t size;
  ASSERT_TRUE(file->Tell(&size));
  EXPECT_EQ(2 * kWriteBufferSize, static_cast<int64_t>(size));
}

TEST_F(MemoryFileTest, ReadMissingFileFails) {
  std::unique_ptr<File, FileCloser> file(File::Open("memory://file1", "r"));
  EXPECT_FALSE(file);
}

TEST_F(MemoryFileTest, WriteExistingFileDeletes) {
  std::unique_ptr<File, FileCloser> file1(File::Open("memory://file1", "w"));
  ASSERT_TRUE(file1);
  ASSERT_EQ(kWriteBufferSize, file1->Write(kWriteBuffer, kWriteBufferSize));
  file1.release()->Close();

  std::unique_ptr<File, FileCloser> file2(File::Open("memory://file1", "w"));
  ASSERT_TRUE(file2);
  EXPECT_EQ(0, file2->Size());
}

}  // namespace shaka
