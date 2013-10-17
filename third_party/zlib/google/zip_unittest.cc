// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <vector>

#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/zlib/google/zip.h"
#include "third_party/zlib/google/zip_reader.h"

namespace {

// Make the test a PlatformTest to setup autorelease pools properly on Mac.
class ZipTest : public PlatformTest {
 protected:
  virtual void SetUp() {
    PlatformTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_dir_ = temp_dir_.path();

    base::FilePath zip_path(test_dir_);
    zip_contents_.insert(zip_path.AppendASCII("foo.txt"));
    zip_path = zip_path.AppendASCII("foo");
    zip_contents_.insert(zip_path);
    zip_contents_.insert(zip_path.AppendASCII("bar.txt"));
    zip_path = zip_path.AppendASCII("bar");
    zip_contents_.insert(zip_path);
    zip_contents_.insert(zip_path.AppendASCII("baz.txt"));
    zip_contents_.insert(zip_path.AppendASCII("quux.txt"));
    zip_contents_.insert(zip_path.AppendASCII(".hidden"));

    // Include a subset of files in |zip_file_list_| to test ZipFiles().
    zip_file_list_.push_back(base::FilePath(FILE_PATH_LITERAL("foo.txt")));
    zip_file_list_.push_back(
        base::FilePath(FILE_PATH_LITERAL("foo/bar/quux.txt")));
    zip_file_list_.push_back(
        base::FilePath(FILE_PATH_LITERAL("foo/bar/.hidden")));
  }

  virtual void TearDown() {
    PlatformTest::TearDown();
  }

  bool GetTestDataDirectory(base::FilePath* path) {
    bool success = PathService::Get(base::DIR_SOURCE_ROOT, path);
    EXPECT_TRUE(success);
    if (!success)
      return false;
    *path = path->AppendASCII("third_party");
    *path = path->AppendASCII("zlib");
    *path = path->AppendASCII("google");
    *path = path->AppendASCII("test");
    *path = path->AppendASCII("data");
    return true;
  }

  void TestUnzipFile(const base::FilePath::StringType& filename,
                     bool expect_hidden_files) {
    base::FilePath test_dir;
    ASSERT_TRUE(GetTestDataDirectory(&test_dir));
    TestUnzipFile(test_dir.Append(filename), expect_hidden_files);
  }

  void TestUnzipFile(const base::FilePath& path, bool expect_hidden_files) {
    ASSERT_TRUE(base::PathExists(path)) << "no file " << path.value();
    ASSERT_TRUE(zip::Unzip(path, test_dir_));

    base::FileEnumerator files(test_dir_, true,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    base::FilePath next_path = files.Next();
    size_t count = 0;
    while (!next_path.value().empty()) {
      if (next_path.value().find(FILE_PATH_LITERAL(".svn")) ==
          base::FilePath::StringType::npos) {
        EXPECT_EQ(zip_contents_.count(next_path), 1U) <<
            "Couldn't find " << next_path.value();
        count++;
      }
      next_path = files.Next();
    }

    size_t expected_count = 0;
    for (std::set<base::FilePath>::iterator iter = zip_contents_.begin();
         iter != zip_contents_.end(); ++iter) {
      if (expect_hidden_files || iter->BaseName().value()[0] != '.')
        ++expected_count;
    }

    EXPECT_EQ(expected_count, count);
  }

  // The path to temporary directory used to contain the test operations.
  base::FilePath test_dir_;

  base::ScopedTempDir temp_dir_;

  // Hard-coded contents of a known zip file.
  std::set<base::FilePath> zip_contents_;

  // Hard-coded list of relative paths for a zip file created with ZipFiles.
  std::vector<base::FilePath> zip_file_list_;
};

TEST_F(ZipTest, Unzip) {
  TestUnzipFile(FILE_PATH_LITERAL("test.zip"), true);
}

TEST_F(ZipTest, UnzipUncompressed) {
  TestUnzipFile(FILE_PATH_LITERAL("test_nocompress.zip"), true);
}

TEST_F(ZipTest, UnzipEvil) {
  base::FilePath path;
  ASSERT_TRUE(GetTestDataDirectory(&path));
  path = path.AppendASCII("evil.zip");
  // Unzip the zip file into a sub directory of test_dir_ so evil.zip
  // won't create a persistent file outside test_dir_ in case of a
  // failure.
  base::FilePath output_dir = test_dir_.AppendASCII("out");
  ASSERT_FALSE(zip::Unzip(path, output_dir));
  base::FilePath evil_file = output_dir;
  evil_file = evil_file.AppendASCII(
      "../levilevilevilevilevilevilevilevilevilevilevilevil");
  ASSERT_FALSE(base::PathExists(evil_file));
}

TEST_F(ZipTest, UnzipEvil2) {
  base::FilePath path;
  ASSERT_TRUE(GetTestDataDirectory(&path));
  // The zip file contains an evil file with invalid UTF-8 in its file
  // name.
  path = path.AppendASCII("evil_via_invalid_utf8.zip");
  // See the comment at UnzipEvil() for why we do this.
  base::FilePath output_dir = test_dir_.AppendASCII("out");
  // This should fail as it contains an evil file.
  ASSERT_FALSE(zip::Unzip(path, output_dir));
  base::FilePath evil_file = output_dir;
  evil_file = evil_file.AppendASCII("../evil.txt");
  ASSERT_FALSE(base::PathExists(evil_file));
}

TEST_F(ZipTest, Zip) {
  base::FilePath src_dir;
  ASSERT_TRUE(GetTestDataDirectory(&src_dir));
  src_dir = src_dir.AppendASCII("test");

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath zip_file = temp_dir.path().AppendASCII("out.zip");

  EXPECT_TRUE(zip::Zip(src_dir, zip_file, true));
  TestUnzipFile(zip_file, true);
}

TEST_F(ZipTest, ZipIgnoreHidden) {
  base::FilePath src_dir;
  ASSERT_TRUE(GetTestDataDirectory(&src_dir));
  src_dir = src_dir.AppendASCII("test");

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath zip_file = temp_dir.path().AppendASCII("out.zip");

  EXPECT_TRUE(zip::Zip(src_dir, zip_file, false));
  TestUnzipFile(zip_file, false);
}

#if defined(OS_POSIX)
TEST_F(ZipTest, ZipFiles) {
  base::FilePath src_dir;
  ASSERT_TRUE(GetTestDataDirectory(&src_dir));
  src_dir = src_dir.AppendASCII("test");

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath zip_file = temp_dir.path().AppendASCII("out.zip");

  const int flags = base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE;
  const base::PlatformFile zip_fd =
      base::CreatePlatformFile(zip_file, flags, NULL, NULL);
  ASSERT_LE(0, zip_fd);
  EXPECT_TRUE(zip::ZipFiles(src_dir, zip_file_list_, zip_fd));
  base::ClosePlatformFile(zip_fd);

  zip::ZipReader reader;
  EXPECT_TRUE(reader.Open(zip_file));
  EXPECT_EQ(zip_file_list_.size(), static_cast<size_t>(reader.num_entries()));
  for (size_t i = 0; i < zip_file_list_.size(); ++i) {
    EXPECT_TRUE(reader.LocateAndOpenEntry(zip_file_list_[i]));
    // Check the path in the entry just in case.
    const zip::ZipReader::EntryInfo* entry_info = reader.current_entry_info();
    EXPECT_EQ(entry_info->file_path(), zip_file_list_[i]);
  }
}
#endif  // defined(OS_POSIX)

}  // namespace

