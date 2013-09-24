// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util_proxy.h"

#include <map>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/platform_file.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class FileUtilProxyTest : public testing::Test {
 public:
  FileUtilProxyTest()
      : message_loop_(MessageLoop::TYPE_IO),
        file_thread_("FileUtilProxyTestFileThread"),
        error_(PLATFORM_FILE_OK),
        created_(false),
        file_(kInvalidPlatformFileValue),
        bytes_written_(-1),
        weak_factory_(this) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    ASSERT_TRUE(file_thread_.Start());
  }

  virtual void TearDown() OVERRIDE {
    if (file_ != kInvalidPlatformFileValue)
      ClosePlatformFile(file_);
  }

  void DidFinish(PlatformFileError error) {
    error_ = error;
    MessageLoop::current()->QuitWhenIdle();
  }

  void DidCreateOrOpen(PlatformFileError error,
                       PassPlatformFile file,
                       bool created) {
    error_ = error;
    file_ = file.ReleaseValue();
    created_ = created;
    MessageLoop::current()->QuitWhenIdle();
  }

  void DidCreateTemporary(PlatformFileError error,
                          PassPlatformFile file,
                          const FilePath& path) {
    error_ = error;
    file_ = file.ReleaseValue();
    path_ = path;
    MessageLoop::current()->QuitWhenIdle();
  }

  void DidGetFileInfo(PlatformFileError error,
                      const PlatformFileInfo& file_info) {
    error_ = error;
    file_info_ = file_info;
    MessageLoop::current()->QuitWhenIdle();
  }

  void DidRead(PlatformFileError error,
               const char* data,
               int bytes_read) {
    error_ = error;
    buffer_.resize(bytes_read);
    memcpy(&buffer_[0], data, bytes_read);
    MessageLoop::current()->QuitWhenIdle();
  }

  void DidWrite(PlatformFileError error,
                int bytes_written) {
    error_ = error;
    bytes_written_ = bytes_written;
    MessageLoop::current()->QuitWhenIdle();
  }

 protected:
  PlatformFile GetTestPlatformFile(int flags) {
    if (file_ != kInvalidPlatformFileValue)
      return file_;
    bool created;
    PlatformFileError error;
    file_ = CreatePlatformFile(test_path(), flags, &created, &error);
    EXPECT_EQ(PLATFORM_FILE_OK, error);
    EXPECT_NE(kInvalidPlatformFileValue, file_);
    return file_;
  }

  TaskRunner* file_task_runner() const {
    return file_thread_.message_loop_proxy().get();
  }
  const FilePath& test_dir_path() const { return dir_.path(); }
  const FilePath test_path() const { return dir_.path().AppendASCII("test"); }

  MessageLoop message_loop_;
  Thread file_thread_;

  ScopedTempDir dir_;
  PlatformFileError error_;
  bool created_;
  PlatformFile file_;
  FilePath path_;
  PlatformFileInfo file_info_;
  std::vector<char> buffer_;
  int bytes_written_;
  WeakPtrFactory<FileUtilProxyTest> weak_factory_;
};

TEST_F(FileUtilProxyTest, CreateOrOpen_Create) {
  FileUtilProxy::CreateOrOpen(
      file_task_runner(),
      test_path(),
      PLATFORM_FILE_CREATE | PLATFORM_FILE_READ,
      Bind(&FileUtilProxyTest::DidCreateOrOpen, weak_factory_.GetWeakPtr()));
  MessageLoop::current()->Run();

  EXPECT_EQ(PLATFORM_FILE_OK, error_);
  EXPECT_TRUE(created_);
  EXPECT_NE(kInvalidPlatformFileValue, file_);
  EXPECT_TRUE(PathExists(test_path()));
}

TEST_F(FileUtilProxyTest, CreateOrOpen_Open) {
  // Creates a file.
  file_util::WriteFile(test_path(), NULL, 0);
  ASSERT_TRUE(PathExists(test_path()));

  // Opens the created file.
  FileUtilProxy::CreateOrOpen(
      file_task_runner(),
      test_path(),
      PLATFORM_FILE_OPEN | PLATFORM_FILE_READ,
      Bind(&FileUtilProxyTest::DidCreateOrOpen, weak_factory_.GetWeakPtr()));
  MessageLoop::current()->Run();

  EXPECT_EQ(PLATFORM_FILE_OK, error_);
  EXPECT_FALSE(created_);
  EXPECT_NE(kInvalidPlatformFileValue, file_);
}

TEST_F(FileUtilProxyTest, CreateOrOpen_OpenNonExistent) {
  FileUtilProxy::CreateOrOpen(
      file_task_runner(),
      test_path(),
      PLATFORM_FILE_OPEN | PLATFORM_FILE_READ,
      Bind(&FileUtilProxyTest::DidCreateOrOpen, weak_factory_.GetWeakPtr()));
  MessageLoop::current()->Run();
  EXPECT_EQ(PLATFORM_FILE_ERROR_NOT_FOUND, error_);
  EXPECT_FALSE(created_);
  EXPECT_EQ(kInvalidPlatformFileValue, file_);
  EXPECT_FALSE(PathExists(test_path()));
}

TEST_F(FileUtilProxyTest, Close) {
  // Creates a file.
  PlatformFile file = GetTestPlatformFile(
      PLATFORM_FILE_CREATE | PLATFORM_FILE_WRITE);

#if defined(OS_WIN)
  // This fails on Windows if the file is not closed.
  EXPECT_FALSE(base::Move(test_path(),
                               test_dir_path().AppendASCII("new")));
#endif

  FileUtilProxy::Close(
      file_task_runner(),
      file,
      Bind(&FileUtilProxyTest::DidFinish, weak_factory_.GetWeakPtr()));
  MessageLoop::current()->Run();
  EXPECT_EQ(PLATFORM_FILE_OK, error_);

  // Now it should pass on all platforms.
  EXPECT_TRUE(base::Move(test_path(), test_dir_path().AppendASCII("new")));
}

TEST_F(FileUtilProxyTest, CreateTemporary) {
  FileUtilProxy::CreateTemporary(
      file_task_runner(), 0 /* additional_file_flags */,
      Bind(&FileUtilProxyTest::DidCreateTemporary, weak_factory_.GetWeakPtr()));
  MessageLoop::current()->Run();
  EXPECT_EQ(PLATFORM_FILE_OK, error_);
  EXPECT_TRUE(PathExists(path_));
  EXPECT_NE(kInvalidPlatformFileValue, file_);

  // The file should be writable.
#if defined(OS_WIN)
  HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  OVERLAPPED overlapped = {0};
  overlapped.hEvent = hEvent;
  DWORD bytes_written;
  if (!::WriteFile(file_, "test", 4, &bytes_written, &overlapped)) {
    // Temporary file is created with ASYNC flag, so WriteFile may return 0
    // with ERROR_IO_PENDING.
    EXPECT_EQ(ERROR_IO_PENDING, GetLastError());
    GetOverlappedResult(file_, &overlapped, &bytes_written, TRUE);
  }
  EXPECT_EQ(4, bytes_written);
#else
  // On POSIX ASYNC flag does not affect synchronous read/write behavior.
  EXPECT_EQ(4, WritePlatformFile(file_, 0, "test", 4));
#endif
  EXPECT_TRUE(ClosePlatformFile(file_));
  file_ = kInvalidPlatformFileValue;

  // Make sure the written data can be read from the returned path.
  std::string data;
  EXPECT_TRUE(file_util::ReadFileToString(path_, &data));
  EXPECT_EQ("test", data);

  // Make sure we can & do delete the created file to prevent leaks on the bots.
  EXPECT_TRUE(base::DeleteFile(path_, false));
}

TEST_F(FileUtilProxyTest, GetFileInfo_File) {
  // Setup.
  ASSERT_EQ(4, file_util::WriteFile(test_path(), "test", 4));
  PlatformFileInfo expected_info;
  file_util::GetFileInfo(test_path(), &expected_info);

  // Run.
  FileUtilProxy::GetFileInfo(
      file_task_runner(),
      test_path(),
      Bind(&FileUtilProxyTest::DidGetFileInfo, weak_factory_.GetWeakPtr()));
  MessageLoop::current()->Run();

  // Verify.
  EXPECT_EQ(PLATFORM_FILE_OK, error_);
  EXPECT_EQ(expected_info.size, file_info_.size);
  EXPECT_EQ(expected_info.is_directory, file_info_.is_directory);
  EXPECT_EQ(expected_info.is_symbolic_link, file_info_.is_symbolic_link);
  EXPECT_EQ(expected_info.last_modified, file_info_.last_modified);
  EXPECT_EQ(expected_info.last_accessed, file_info_.last_accessed);
  EXPECT_EQ(expected_info.creation_time, file_info_.creation_time);
}

TEST_F(FileUtilProxyTest, GetFileInfo_Directory) {
  // Setup.
  ASSERT_TRUE(file_util::CreateDirectory(test_path()));
  PlatformFileInfo expected_info;
  file_util::GetFileInfo(test_path(), &expected_info);

  // Run.
  FileUtilProxy::GetFileInfo(
      file_task_runner(),
      test_path(),
      Bind(&FileUtilProxyTest::DidGetFileInfo, weak_factory_.GetWeakPtr()));
  MessageLoop::current()->Run();

  // Verify.
  EXPECT_EQ(PLATFORM_FILE_OK, error_);
  EXPECT_EQ(expected_info.size, file_info_.size);
  EXPECT_EQ(expected_info.is_directory, file_info_.is_directory);
  EXPECT_EQ(expected_info.is_symbolic_link, file_info_.is_symbolic_link);
  EXPECT_EQ(expected_info.last_modified, file_info_.last_modified);
  EXPECT_EQ(expected_info.last_accessed, file_info_.last_accessed);
  EXPECT_EQ(expected_info.creation_time, file_info_.creation_time);
}

TEST_F(FileUtilProxyTest, Read) {
  // Setup.
  const char expected_data[] = "bleh";
  int expected_bytes = arraysize(expected_data);
  ASSERT_EQ(expected_bytes,
            file_util::WriteFile(test_path(), expected_data, expected_bytes));

  // Run.
  FileUtilProxy::Read(
      file_task_runner(),
      GetTestPlatformFile(PLATFORM_FILE_OPEN | PLATFORM_FILE_READ),
      0,  // offset
      128,
      Bind(&FileUtilProxyTest::DidRead, weak_factory_.GetWeakPtr()));
  MessageLoop::current()->Run();

  // Verify.
  EXPECT_EQ(PLATFORM_FILE_OK, error_);
  EXPECT_EQ(expected_bytes, static_cast<int>(buffer_.size()));
  for (size_t i = 0; i < buffer_.size(); ++i) {
    EXPECT_EQ(expected_data[i], buffer_[i]);
  }
}

TEST_F(FileUtilProxyTest, WriteAndFlush) {
  const char data[] = "foo!";
  int data_bytes = ARRAYSIZE_UNSAFE(data);
  PlatformFile file = GetTestPlatformFile(
      PLATFORM_FILE_CREATE | PLATFORM_FILE_WRITE);

  FileUtilProxy::Write(
      file_task_runner(),
      file,
      0,  // offset
      data,
      data_bytes,
      Bind(&FileUtilProxyTest::DidWrite, weak_factory_.GetWeakPtr()));
  MessageLoop::current()->Run();
  EXPECT_EQ(PLATFORM_FILE_OK, error_);
  EXPECT_EQ(data_bytes, bytes_written_);

  // Flush the written data.  (So that the following read should always
  // succeed.  On some platforms it may work with or without this flush.)
  FileUtilProxy::Flush(
      file_task_runner(),
      file,
      Bind(&FileUtilProxyTest::DidFinish, weak_factory_.GetWeakPtr()));
  MessageLoop::current()->Run();
  EXPECT_EQ(PLATFORM_FILE_OK, error_);

  // Verify the written data.
  char buffer[10];
  EXPECT_EQ(data_bytes, file_util::ReadFile(test_path(), buffer, data_bytes));
  for (int i = 0; i < data_bytes; ++i) {
    EXPECT_EQ(data[i], buffer[i]);
  }
}

TEST_F(FileUtilProxyTest, Touch) {
  Time last_accessed_time = Time::Now() - TimeDelta::FromDays(12345);
  Time last_modified_time = Time::Now() - TimeDelta::FromHours(98765);

  FileUtilProxy::Touch(
      file_task_runner(),
      GetTestPlatformFile(PLATFORM_FILE_CREATE |
                          PLATFORM_FILE_WRITE |
                          PLATFORM_FILE_WRITE_ATTRIBUTES),
      last_accessed_time,
      last_modified_time,
      Bind(&FileUtilProxyTest::DidFinish, weak_factory_.GetWeakPtr()));
  MessageLoop::current()->Run();
  EXPECT_EQ(PLATFORM_FILE_OK, error_);

  PlatformFileInfo info;
  file_util::GetFileInfo(test_path(), &info);

  // The returned values may only have the seconds precision, so we cast
  // the double values to int here.
  EXPECT_EQ(static_cast<int>(last_modified_time.ToDoubleT()),
            static_cast<int>(info.last_modified.ToDoubleT()));
  EXPECT_EQ(static_cast<int>(last_accessed_time.ToDoubleT()),
            static_cast<int>(info.last_accessed.ToDoubleT()));
}

TEST_F(FileUtilProxyTest, Truncate_Shrink) {
  // Setup.
  const char kTestData[] = "0123456789";
  ASSERT_EQ(10, file_util::WriteFile(test_path(), kTestData, 10));
  PlatformFileInfo info;
  file_util::GetFileInfo(test_path(), &info);
  ASSERT_EQ(10, info.size);

  // Run.
  FileUtilProxy::Truncate(
      file_task_runner(),
      GetTestPlatformFile(PLATFORM_FILE_OPEN | PLATFORM_FILE_WRITE),
      7,
      Bind(&FileUtilProxyTest::DidFinish, weak_factory_.GetWeakPtr()));
  MessageLoop::current()->Run();

  // Verify.
  file_util::GetFileInfo(test_path(), &info);
  ASSERT_EQ(7, info.size);

  char buffer[7];
  EXPECT_EQ(7, file_util::ReadFile(test_path(), buffer, 7));
  int i = 0;
  for (; i < 7; ++i)
    EXPECT_EQ(kTestData[i], buffer[i]);
}

TEST_F(FileUtilProxyTest, Truncate_Expand) {
  // Setup.
  const char kTestData[] = "9876543210";
  ASSERT_EQ(10, file_util::WriteFile(test_path(), kTestData, 10));
  PlatformFileInfo info;
  file_util::GetFileInfo(test_path(), &info);
  ASSERT_EQ(10, info.size);

  // Run.
  FileUtilProxy::Truncate(
      file_task_runner(),
      GetTestPlatformFile(PLATFORM_FILE_OPEN | PLATFORM_FILE_WRITE),
      53,
      Bind(&FileUtilProxyTest::DidFinish, weak_factory_.GetWeakPtr()));
  MessageLoop::current()->Run();

  // Verify.
  file_util::GetFileInfo(test_path(), &info);
  ASSERT_EQ(53, info.size);

  char buffer[53];
  EXPECT_EQ(53, file_util::ReadFile(test_path(), buffer, 53));
  int i = 0;
  for (; i < 10; ++i)
    EXPECT_EQ(kTestData[i], buffer[i]);
  for (; i < 53; ++i)
    EXPECT_EQ(0, buffer[i]);
}

}  // namespace base
