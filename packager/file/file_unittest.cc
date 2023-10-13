// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/file.h>

#include <cstdio>
#include <filesystem>
#include <locale>

#include <absl/flags/declare.h>
#include <gtest/gtest.h>

#include <packager/file/file_test_util.h>
#include <packager/flag_saver.h>

ABSL_DECLARE_FLAG(uint64_t, io_cache_size);
ABSL_DECLARE_FLAG(uint64_t, io_block_size);

namespace {
const int kDataSize = 1024;

// Write a file with standard C library routines.
void WriteFile(const std::string& path, const std::string& data) {
  FILE* f = fopen(path.c_str(), "wb");
  ASSERT_EQ(data.size(), fwrite(data.data(), 1, data.size(), f));
  fclose(f);
}

void DeleteFile(const std::string& path) {
  std::error_code ec;
  std::filesystem::remove(std::filesystem::u8path(path), ec);
  // Ignore errors.
}

int64_t FileSize(const std::string& path) {
  std::error_code ec;
  int64_t file_size =
      std::filesystem::file_size(std::filesystem::u8path(path), ec);
  if (ec) {
    return -1;
  }
  return file_size;
}

// Returns num bytes read, up to max_size.
uint64_t ReadFile(const std::string& path,
                  std::string* data,
                  uint32_t max_size) {
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) {
    return 0;
  }

  data->resize(max_size);
  uint64_t bytes = fread(data->data(), 1, max_size, f);
  data->resize(bytes);
  return bytes;
}

}  // namespace

namespace shaka {

class LocalFileTest : public testing::Test {
 protected:
  static std::string original_locale_;

  static void SetUpTestSuite() {
    original_locale_ = setlocale(LC_ALL, NULL);
    setlocale(LC_ALL, ".UTF8");
  }

  static void TearDownTestSuite() {
    setlocale(LC_ALL, original_locale_.c_str());
  }

  void SetUp() override {
    data_.resize(kDataSize);
    for (int i = 0; i < kDataSize; ++i)
      data_[i] = i % 256;

    local_file_name_no_prefix_ = generate_unique_temp_path();

    // Local file name with prefix for File API.
    local_file_name_ = kLocalFilePrefix;
    local_file_name_ += local_file_name_no_prefix_;

    // Use LocalFile directly without ThreadedIoFile.
    backup_io_cache_size.reset(new FlagSaver<uint64_t>(&FLAGS_io_cache_size));
    absl::SetFlag(&FLAGS_io_cache_size, 0);
  }

  void TearDown() override {
    // Remove test file if created.
    DeleteFile(local_file_name_no_prefix_);
  }

  std::unique_ptr<FlagSaver<uint64_t>> backup_io_cache_size;

  std::string data_;

  // A path to a temporary test file.
  std::string local_file_name_no_prefix_;

  // Same as |local_file_name_no_prefix_| but with the file prefix.
  std::string local_file_name_;
};

// static
std::string LocalFileTest::original_locale_;

TEST_F(LocalFileTest, ReadNotExist) {
  // Remove test file if it exists.
  DeleteFile(local_file_name_no_prefix_);
  ASSERT_TRUE(File::Open(local_file_name_.c_str(), "r") == NULL);
}

TEST_F(LocalFileTest, Size) {
  WriteFile(local_file_name_no_prefix_, data_);
  ASSERT_EQ(kDataSize, File::GetFileSize(local_file_name_.c_str()));
}

TEST_F(LocalFileTest, Copy) {
  WriteFile(local_file_name_no_prefix_, data_);

  TempFile temp_file;
  std::string destination = temp_file.path();

  ASSERT_TRUE(File::Copy(local_file_name_.c_str(), destination.c_str()));

  ASSERT_EQ(kDataSize, FileSize(destination));

  // Try to read twice as much data as expected, to make sure that there isn't
  // extra stuff appended.
  std::string read_data;
  ASSERT_EQ(kDataSize, ReadFile(destination, &read_data, kDataSize * 2));
  ASSERT_EQ(data_, read_data);
}

TEST_F(LocalFileTest, Write) {
  // Write file using File API.
  File* file = File::Open(local_file_name_.c_str(), "w");
  ASSERT_TRUE(file != NULL);
  EXPECT_EQ(kDataSize, file->Write(&data_[0], kDataSize));
  EXPECT_EQ(kDataSize, file->Size());
  EXPECT_TRUE(file->Close());

  std::string read_data;
  ASSERT_EQ(kDataSize, FileSize(local_file_name_no_prefix_));
  ASSERT_EQ(kDataSize,
            ReadFile(local_file_name_no_prefix_, &read_data, kDataSize));

  // Compare data written and read.
  EXPECT_EQ(data_, read_data);
}

TEST_F(LocalFileTest, Read_And_Eof) {
  WriteFile(local_file_name_no_prefix_, data_);

  // Read file using File API.
  File* file = File::Open(local_file_name_.c_str(), "r");
  ASSERT_TRUE(file != NULL);

  // Read half of the file.
  const int kFirstReadBytes = kDataSize / 2;
  std::string read_data(kFirstReadBytes + kDataSize, 0);
  EXPECT_EQ(kFirstReadBytes, file->Read(&read_data[0], kFirstReadBytes));

  // Read the remaining half of the file and verify EOF.
  EXPECT_EQ(kDataSize - kFirstReadBytes,
            file->Read(&read_data[kFirstReadBytes], kDataSize));
  uint8_t single_byte;
  EXPECT_EQ(0, file->Read(&single_byte, sizeof(single_byte)));
  EXPECT_TRUE(file->Close());

  // Compare data written and read.
  read_data.resize(kDataSize);
  EXPECT_EQ(data_, read_data);
}

TEST_F(LocalFileTest, WriteRead) {
  // Write file using File API, using file name directly (without prefix).
  File* file = File::Open(local_file_name_no_prefix_.c_str(), "w");
  ASSERT_TRUE(file != NULL);
  EXPECT_EQ(kDataSize, file->Write(&data_[0], kDataSize));
  EXPECT_EQ(kDataSize, file->Size());
  EXPECT_TRUE(file->Close());

  // Read file using File API, using local file prefix + file name.
  file = File::Open(local_file_name_.c_str(), "r");
  ASSERT_TRUE(file != NULL);

  // Read half of the file and verify that Eof is not true.
  std::string read_data(kDataSize, 0);
  EXPECT_EQ(kDataSize, file->Read(&read_data[0], kDataSize));
  EXPECT_TRUE(file->Close());

  // Compare data written and read.
  EXPECT_EQ(data_, read_data);
}

TEST_F(LocalFileTest, WriteStringReadString) {
  ASSERT_TRUE(
      File::WriteStringToFile(local_file_name_no_prefix_.c_str(), data_));
  std::string read_data;
  ASSERT_TRUE(
      File::ReadFileToString(local_file_name_no_prefix_.c_str(), &read_data));
  EXPECT_EQ(data_, read_data);
}

// There is no easy way to test if a write operation is atomic. This test only
// ensures the data is written correctly.
TEST_F(LocalFileTest, AtomicWriteRead) {
  ASSERT_TRUE(
      File::WriteFileAtomically(local_file_name_no_prefix_.c_str(), data_));
  std::string read_data;
  ASSERT_TRUE(
      File::ReadFileToString(local_file_name_no_prefix_.c_str(), &read_data));
  EXPECT_EQ(data_, read_data);
}

TEST_F(LocalFileTest, WriteFlushCheckSize) {
  const uint32_t kNumCycles(10);
  const uint32_t kNumWrites(10);

  for (uint32_t cycle_idx = 0; cycle_idx < kNumCycles; ++cycle_idx) {
    // Write file using File API, using file name directly (without prefix).
    File* file = File::Open(local_file_name_no_prefix_.c_str(), "w");
    ASSERT_TRUE(file != NULL);
    for (uint32_t write_idx = 0; write_idx < kNumWrites; ++write_idx)
      EXPECT_EQ(kDataSize, file->Write(data_.data(), kDataSize));
    ASSERT_NO_FATAL_FAILURE(file->Flush());
    EXPECT_TRUE(file->Close());

    file = File::Open(local_file_name_.c_str(), "r");
    ASSERT_TRUE(file != NULL);
    EXPECT_EQ(static_cast<int64_t>(data_.size() * kNumWrites), file->Size());

    EXPECT_TRUE(file->Close());
  }
}

TEST_F(LocalFileTest, IsLocalRegular) {
  WriteFile(local_file_name_no_prefix_, data_);
  ASSERT_TRUE(File::IsLocalRegularFile(local_file_name_.c_str()));
}

TEST_F(LocalFileTest, UnicodePath) {
  // Delete the temp file already created.
  DeleteFile(local_file_name_no_prefix_);

  // Modify the local file name for this test to include non-ASCII characters.
  // This is used in TearDown() to clean up the file we create in the test.
  const std::string unicode_suffix = "από.txt";
  local_file_name_ += unicode_suffix;
  local_file_name_no_prefix_ += unicode_suffix;

  // Write file using File API.
  File* file = File::Open(local_file_name_.c_str(), "w");
  ASSERT_TRUE(file != NULL);
  EXPECT_EQ(kDataSize, file->Write(&data_[0], kDataSize));

  // Check the size.
  EXPECT_EQ(kDataSize, file->Size());
  ASSERT_TRUE(file->Close());

  // Open file using File API.
  file = File::Open(local_file_name_.c_str(), "r");
  ASSERT_TRUE(file != NULL);

  // Read the entire file.
  std::string read_data(kDataSize, 0);
  EXPECT_EQ(kDataSize, file->Read(&read_data[0], kDataSize));

  // Verify EOF.
  uint8_t single_byte;
  EXPECT_EQ(0, file->Read(&single_byte, sizeof(single_byte)));
  ASSERT_TRUE(file->Close());

  // Compare data written and read.
  EXPECT_EQ(data_, read_data);
}

class ParamLocalFileTest : public LocalFileTest,
                           public ::testing::WithParamInterface<uint8_t> {};

TEST_P(ParamLocalFileTest, SeekWriteAndSeekRead) {
  const uint32_t kBlockSize(10);
  const uint32_t kInitialWriteSize(100);
  const uint32_t kFinalFileSize(200);

  FlagSaver local_backup_io_block_size(&FLAGS_io_block_size);
  FlagSaver local_backup_io_cache_size(&FLAGS_io_cache_size);
  absl::SetFlag(&FLAGS_io_block_size, kBlockSize);
  absl::SetFlag(&FLAGS_io_cache_size, GetParam());

  std::vector<uint8_t> buffer(kInitialWriteSize);
  File* file = File::Open(local_file_name_no_prefix_.c_str(), "w");
  ASSERT_TRUE(file != nullptr);
  ASSERT_EQ(kInitialWriteSize, file->Write(buffer.data(), kInitialWriteSize));
  EXPECT_EQ(kInitialWriteSize, file->Size());
  uint64_t position;
  ASSERT_TRUE(file->Tell(&position));
  ASSERT_EQ(kInitialWriteSize, position);

  for (uint8_t offset = 0; offset < kFinalFileSize; ++offset) {
    // Seek to each offset, check that the position matches.
    EXPECT_TRUE(file->Seek(offset));
    ASSERT_TRUE(file->Tell(&position));
    EXPECT_EQ(offset, position);

    // Write two bytes of data at this offset (NULs), check that the position
    // was advanced by two bytes.
    EXPECT_EQ(2u, file->Write(buffer.data(), 2u));
    ASSERT_TRUE(file->Tell(&position));
    EXPECT_EQ(offset + 2u, position);

    // Seek to the byte right after the original offset (the second NUL we
    // wrote), check that the position matches.
    ++offset;
    EXPECT_TRUE(file->Seek(offset));
    ASSERT_TRUE(file->Tell(&position));
    EXPECT_EQ(offset, position);

    // Overwrite the byte at this position, with a value matching the current
    // offset, check that the position was advanced by one byte.
    EXPECT_EQ(1, file->Write(&offset, 1));
    ASSERT_TRUE(file->Tell(&position));
    EXPECT_EQ(offset + 1u, position);

    // The pattern in bytes will be:
    //   0x00, 0x01, 0x00, 0x03, 0x00, 0x05, ...
  }
  EXPECT_EQ(kFinalFileSize, file->Size());
  ASSERT_TRUE(file->Close());

  file = File::Open(local_file_name_no_prefix_.c_str(), "r");
  ASSERT_TRUE(file != nullptr);
  for (uint8_t offset = 1; offset < kFinalFileSize; offset += 2) {
    uint8_t read_byte;

    // Seek to the odd bytes, which should have values matching their offsets.
    EXPECT_TRUE(file->Seek(offset));
    ASSERT_TRUE(file->Tell(&position));
    EXPECT_EQ(offset, position);

    // Read a byte, check that the position was advanced by one byte, and that
    // the value matches what we wrote in the loop above (the offset).
    EXPECT_EQ(1, file->Read(&read_byte, 1));
    ASSERT_TRUE(file->Tell(&position));
    EXPECT_EQ(offset + 1u, position);
    EXPECT_EQ(offset, read_byte);
  }

  // We can't read any more at this position (the end).
  EXPECT_EQ(0, file->Read(buffer.data(), 1));
  // If we seek back to 0, we can read another byte.
  ASSERT_TRUE(file->Seek(0));
  EXPECT_EQ(1, file->Read(buffer.data(), 1));

  EXPECT_TRUE(file->Close());
}
INSTANTIATE_TEST_SUITE_P(TestSeekWithDifferentCacheSizes,
                         ParamLocalFileTest,
                         // 0 disables cache, 20 is small, 61 is prime, and 1000
                         // is just under the data size of 1k.
                         ::testing::Values(0u, 20u, 61u, 1000u));

TEST(FileTest, MakeCallbackFileName) {
  const BufferCallbackParams* params =
      reinterpret_cast<BufferCallbackParams*>(1000);
  EXPECT_EQ("callback://1000/some name",
            File::MakeCallbackFileName(*params, "some name"));
  EXPECT_EQ("", File::MakeCallbackFileName(*params, ""));
}

TEST(FileTest, ParseCallbackFileName) {
  const BufferCallbackParams* params = nullptr;
  std::string name;
  ASSERT_TRUE(File::ParseCallbackFileName("1000/some name", &params, &name));
  EXPECT_EQ(1000, reinterpret_cast<int64_t>(params));
  EXPECT_EQ("some name", name);
}

TEST(FileTest, ParseCallbackFileNameFailed) {
  const BufferCallbackParams* params = nullptr;
  std::string name;
  ASSERT_FALSE(File::ParseCallbackFileName("1000\\some name", &params, &name));
  ASSERT_FALSE(File::ParseCallbackFileName("abc/some name", &params, &name));
}

}  // namespace shaka
