// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/file/io_cache.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

#include <gtest/gtest.h>

namespace {
const uint64_t kBlockSize = 256;
const uint64_t kCacheSize = 16 * kBlockSize;
}  // namespace

namespace shaka {

class IoCacheTest : public testing::Test {
 public:
  void WriteToCache(const std::vector<uint8_t>& test_buffer,
                    uint64_t num_writes,
                    int sleep_between_writes_ms,
                    bool close_when_done) {
    for (uint64_t write_idx = 0; write_idx < num_writes; ++write_idx) {
      uint64_t write_result =
          cache_->Write(test_buffer.data(), test_buffer.size());
      if (!write_result) {
        // Cache was closed.
        cache_closed_ = true;
        break;
      }
      EXPECT_EQ(test_buffer.size(), write_result);
      if (sleep_between_writes_ms) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(sleep_between_writes_ms));
      }
    }
    if (close_when_done)
      cache_->Close();
  }

 protected:
  void SetUp() override {
    for (unsigned int idx = 0; idx < kBlockSize; ++idx)
      reference_block_[idx] = idx & 0xff;
    cache_.reset(new IoCache(kCacheSize));
    cache_closed_ = false;
  }

  void TearDown() override { WaitForWriterThread(); }

  void GenerateTestBuffer(uint64_t size, std::vector<uint8_t>* test_buffer) {
    test_buffer->resize(size);
    uint8_t* w_ptr(test_buffer->data());
    while (size) {
      uint64_t copy_size(std::min(size, kBlockSize));
      memcpy(w_ptr, reference_block_, copy_size);
      w_ptr += copy_size;
      size -= copy_size;
    }
  }

  void WriteToCacheThreaded(const std::vector<uint8_t>& test_buffer,
                            uint64_t num_writes,
                            int sleep_between_writes_ms,
                            bool close_when_done) {
    writer_thread_.reset(new std::thread(
        std::bind(&IoCacheTest::WriteToCache, this, test_buffer, num_writes,
                  sleep_between_writes_ms, close_when_done)));
  }

  void WaitForWriterThread() {
    if (writer_thread_) {
      writer_thread_->join();
      writer_thread_.reset();
    }
  }

  std::unique_ptr<IoCache> cache_;
  std::unique_ptr<std::thread> writer_thread_;
  uint8_t reference_block_[kBlockSize];
  bool cache_closed_;
};

TEST_F(IoCacheTest, VerySmallWrite) {
  const uint64_t kTestBytes(5);

  std::vector<uint8_t> write_buffer;
  GenerateTestBuffer(kTestBytes, &write_buffer);
  WriteToCacheThreaded(write_buffer, 1, 0, false);

  std::vector<uint8_t> read_buffer(kTestBytes);
  EXPECT_EQ(kTestBytes, cache_->Read(read_buffer.data(), kTestBytes));
  EXPECT_EQ(write_buffer, read_buffer);
}

TEST_F(IoCacheTest, LotsOfAlignedBlocks) {
  const uint64_t kNumWrites(kCacheSize * 1000 / kBlockSize);

  std::vector<uint8_t> write_buffer;
  GenerateTestBuffer(kBlockSize, &write_buffer);
  WriteToCacheThreaded(write_buffer, kNumWrites, 0, false);
  for (uint64_t num_reads = 0; num_reads < kNumWrites; ++num_reads) {
    std::vector<uint8_t> read_buffer(kBlockSize);
    EXPECT_EQ(kBlockSize, cache_->Read(read_buffer.data(), kBlockSize));
    EXPECT_EQ(write_buffer, read_buffer);
  }
}

TEST_F(IoCacheTest, LotsOfUnalignedBlocks) {
  const uint64_t kNumWrites(kCacheSize * 1000 / kBlockSize);
  const uint64_t kUnalignBlockSize(55);

  std::vector<uint8_t> write_buffer1;
  GenerateTestBuffer(kUnalignBlockSize, &write_buffer1);
  WriteToCacheThreaded(write_buffer1, 1, 0, false);
  WaitForWriterThread();
  std::vector<uint8_t> write_buffer2;
  GenerateTestBuffer(kBlockSize, &write_buffer2);
  WriteToCacheThreaded(write_buffer2, kNumWrites, 0, false);

  std::vector<uint8_t> read_buffer1(kUnalignBlockSize);
  EXPECT_EQ(kUnalignBlockSize,
            cache_->Read(read_buffer1.data(), kUnalignBlockSize));
  EXPECT_EQ(write_buffer1, read_buffer1);
  std::vector<uint8_t> verify_buffer;
  for (uint64_t idx = 0; idx < kNumWrites; ++idx)
    verify_buffer.insert(verify_buffer.end(), write_buffer2.begin(),
                         write_buffer2.end());
  uint64_t verify_index(0);
  while (verify_index < verify_buffer.size()) {
    std::vector<uint8_t> read_buffer2(kBlockSize);
    uint64_t bytes_read = cache_->Read(read_buffer2.data(), kBlockSize);
    EXPECT_NE(0U, bytes_read);
    EXPECT_FALSE(
        memcmp(&verify_buffer[verify_index], read_buffer2.data(), bytes_read));
    verify_index += bytes_read;
  }
}

TEST_F(IoCacheTest, SlowWrite) {
  const int kWriteDelayMs(50);
  const uint64_t kNumWrites(kCacheSize * 5 / kBlockSize);

  std::vector<uint8_t> write_buffer;
  GenerateTestBuffer(kBlockSize, &write_buffer);
  WriteToCacheThreaded(write_buffer, kNumWrites, kWriteDelayMs, false);
  for (uint64_t num_reads = 0; num_reads < kNumWrites; ++num_reads) {
    std::vector<uint8_t> read_buffer(kBlockSize);
    EXPECT_EQ(kBlockSize, cache_->Read(read_buffer.data(), kBlockSize));
    EXPECT_EQ(write_buffer, read_buffer);
  }
}

TEST_F(IoCacheTest, SlowRead) {
  const int kReadDelayMs(50);
  const uint64_t kNumWrites(kCacheSize * 5 / kBlockSize);

  std::vector<uint8_t> write_buffer;
  GenerateTestBuffer(kBlockSize, &write_buffer);
  WriteToCacheThreaded(write_buffer, kNumWrites, 0, false);
  for (uint64_t num_reads = 0; num_reads < kNumWrites; ++num_reads) {
    std::vector<uint8_t> read_buffer(kBlockSize);
    EXPECT_EQ(kBlockSize, cache_->Read(read_buffer.data(), kBlockSize));
    EXPECT_EQ(write_buffer, read_buffer);
    std::this_thread::sleep_for(std::chrono::milliseconds(kReadDelayMs));
  }
}

TEST_F(IoCacheTest, CloseByReader) {
  const uint64_t kNumWrites(kCacheSize * 1000 / kBlockSize);

  std::vector<uint8_t> write_buffer;
  GenerateTestBuffer(kBlockSize, &write_buffer);
  WriteToCacheThreaded(write_buffer, kNumWrites, 0, false);
  while (cache_->BytesCached() < kCacheSize) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  cache_->Close();
  WaitForWriterThread();
  EXPECT_TRUE(cache_closed_);
}

TEST_F(IoCacheTest, CloseByWriter) {
  uint8_t test_buffer[kBlockSize];
  std::vector<uint8_t> write_buffer;
  WriteToCacheThreaded(write_buffer, 0, 0, true);
  EXPECT_EQ(0U, cache_->Read(test_buffer, kBlockSize));
  WaitForWriterThread();
}

TEST_F(IoCacheTest, Reopen) {
  const uint64_t kTestBytes1(5);
  const uint64_t kTestBytes2(10);

  std::vector<uint8_t> write_buffer;
  GenerateTestBuffer(kTestBytes1, &write_buffer);
  WriteToCacheThreaded(write_buffer, 1, 0, true);

  std::vector<uint8_t> read_buffer(kTestBytes1);
  EXPECT_EQ(kTestBytes1, cache_->Read(read_buffer.data(), kTestBytes1));
  EXPECT_EQ(write_buffer, read_buffer);

  WaitForWriterThread();
  ASSERT_TRUE(cache_->closed());
  cache_->Reopen();
  ASSERT_FALSE(cache_->closed());

  GenerateTestBuffer(kTestBytes2, &write_buffer);
  WriteToCacheThreaded(write_buffer, 1, 0, false);
  read_buffer.resize(kTestBytes2);
  EXPECT_EQ(kTestBytes2, cache_->Read(read_buffer.data(), kTestBytes2));
  EXPECT_EQ(write_buffer, read_buffer);
}

TEST_F(IoCacheTest, SingleLargeWrite) {
  const uint64_t kTestBytes(kCacheSize * 10);

  std::vector<uint8_t> write_buffer;
  GenerateTestBuffer(kTestBytes, &write_buffer);
  WriteToCacheThreaded(write_buffer, 1, 0, false);
  uint64_t bytes_read(0);
  std::vector<uint8_t> read_buffer(kTestBytes);
  while (bytes_read < kTestBytes) {
    EXPECT_EQ(kBlockSize, cache_->Read(&read_buffer[bytes_read], kBlockSize));
    bytes_read += kBlockSize;
  }
  EXPECT_EQ(write_buffer, read_buffer);
}

TEST_F(IoCacheTest, LargeRead) {
  const uint64_t kNumWrites(kCacheSize * 10 / kBlockSize);

  std::vector<uint8_t> write_buffer;
  GenerateTestBuffer(kBlockSize, &write_buffer);
  WriteToCacheThreaded(write_buffer, kNumWrites, 0, false);
  std::vector<uint8_t> verify_buffer;
  while (verify_buffer.size() < kCacheSize) {
    verify_buffer.insert(verify_buffer.end(), write_buffer.begin(),
                         write_buffer.end());
  }
  while (cache_->BytesCached() < kCacheSize) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  std::vector<uint8_t> read_buffer(kCacheSize);
  EXPECT_EQ(kCacheSize, cache_->Read(read_buffer.data(), kCacheSize));
  EXPECT_EQ(verify_buffer, read_buffer);
  cache_->Close();
}

}  // namespace shaka
