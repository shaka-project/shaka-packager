// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/file/file.h"
#include "packager/media/file/file_closer.h"
#include "packager/media/file/memory_file.h"

namespace edash_packager {
namespace media {
namespace {

const uint8_t kWriteBuffer[] = {1, 2, 3, 4, 5, 6, 7, 8};
const int64_t kWriteBufferSize = sizeof(kWriteBuffer);

}

class MemoryFileTest : public testing::Test {
 protected:
  void TearDown() override {
    MemoryFile::DeleteAll();
  }
};

TEST_F(MemoryFileTest, ModifiesSameFile) {
  scoped_ptr<File, FileCloser> writer(File::Open("memory://file1", "w"));
  ASSERT_EQ(kWriteBufferSize, writer->Write(kWriteBuffer, kWriteBufferSize));

  // Since File::Open should not create a ThreadedIoFile so there should be
  // no cache.

  scoped_ptr<File, FileCloser> reader(File::Open("memory://file1", "r"));

  uint8_t read_buffer[kWriteBufferSize];
  ASSERT_EQ(kWriteBufferSize, reader->Read(read_buffer, kWriteBufferSize));
  EXPECT_EQ(0, memcmp(kWriteBuffer, read_buffer, kWriteBufferSize));
}

TEST_F(MemoryFileTest, SupportsDifferentFiles) {
  scoped_ptr<MemoryFile, FileCloser> writer(new MemoryFile("memory://file1"));
  scoped_ptr<MemoryFile, FileCloser> reader(new MemoryFile("memory://file2"));

  ASSERT_EQ(kWriteBufferSize, writer->Write(kWriteBuffer, kWriteBufferSize));
  ASSERT_EQ(0, reader->Size());
}

TEST_F(MemoryFileTest, SeekAndTell) {
  scoped_ptr<MemoryFile, FileCloser> file(new MemoryFile("memory://file1"));
  ASSERT_EQ(kWriteBufferSize, file->Write(kWriteBuffer, kWriteBufferSize));
  ASSERT_TRUE(file->Seek(0));

  const int64_t seek_pos = kWriteBufferSize / 2;
  ASSERT_TRUE(file->Seek(seek_pos));

  uint64_t size;
  ASSERT_TRUE(file->Tell(&size));
  EXPECT_EQ(seek_pos, static_cast<int64_t>(size));
}

TEST_F(MemoryFileTest, EndOfFile) {
  scoped_ptr<MemoryFile, FileCloser> file(new MemoryFile("memory://file1"));
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
  scoped_ptr<MemoryFile, FileCloser> file(new MemoryFile("memory://file1"));
  ASSERT_EQ(kWriteBufferSize, file->Write(kWriteBuffer, kWriteBufferSize));

  ASSERT_EQ(kWriteBufferSize, file->Size());
  ASSERT_EQ(kWriteBufferSize, file->Write(kWriteBuffer, kWriteBufferSize));
  EXPECT_EQ(2 * kWriteBufferSize, file->Size());

  uint64_t size;
  ASSERT_TRUE(file->Tell(&size));
  EXPECT_EQ(2 * kWriteBufferSize, static_cast<int64_t>(size));
}

}  // namespace media
}  // namespace edash_packager
