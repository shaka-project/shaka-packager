// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/path_output.h"
#include "tools/gn/source_dir.h"
#include "tools/gn/source_file.h"

TEST(PathOutput, Basic) {
  SourceDir build_dir("//out/Debug/");
  PathOutput writer(build_dir, ESCAPE_NONE, false);
  {
    // Normal source-root path.
    std::ostringstream out;
    writer.WriteFile(out, SourceFile("//foo/bar.cc"));
    EXPECT_EQ("../../foo/bar.cc", out.str());
  }
  {
    // File in the root dir.
    std::ostringstream out;
    writer.WriteFile(out, SourceFile("//foo.cc"));
    EXPECT_EQ("../../foo.cc", out.str());
  }
#if defined(OS_WIN)
  {
    // System-absolute path.
    std::ostringstream out;
    writer.WriteFile(out, SourceFile("/C:/foo/bar.cc"));
    EXPECT_EQ("C:/foo/bar.cc", out.str());
  }
#else
  {
    // System-absolute path.
    std::ostringstream out;
    writer.WriteFile(out, SourceFile("/foo/bar.cc"));
    EXPECT_EQ("/foo/bar.cc", out.str());
  }
#endif
}

// Same as basic but the output dir is the root.
TEST(PathOutput, BasicInRoot) {
  SourceDir build_dir("//");
  PathOutput writer(build_dir, ESCAPE_NONE, false);
  {
    // Normal source-root path.
    std::ostringstream out;
    writer.WriteFile(out, SourceFile("//foo/bar.cc"));
    EXPECT_EQ("foo/bar.cc", out.str());
  }
  {
    // File in the root dir.
    std::ostringstream out;
    writer.WriteFile(out, SourceFile("//foo.cc"));
    EXPECT_EQ("foo.cc", out.str());
  }
}

TEST(PathOutput, NinjaEscaping) {
  SourceDir build_dir("//out/Debug/");
  PathOutput writer(build_dir, ESCAPE_NINJA, false);
  {
    // Spaces and $ in filenames.
    std::ostringstream out;
    writer.WriteFile(out, SourceFile("//foo/foo bar$.cc"));
    EXPECT_EQ("../../foo/foo$ bar$$.cc", out.str());
  }
  {
    // Not other weird stuff
    std::ostringstream out;
    writer.WriteFile(out, SourceFile("//foo/\"foo\\bar\".cc"));
    EXPECT_EQ("../../foo/\"foo\\bar\".cc", out.str());
  }
}

TEST(PathOutput, ShellEscaping) {
  SourceDir build_dir("//out/Debug/");
  PathOutput writer(build_dir, ESCAPE_SHELL, false);
  {
    // Spaces in filenames should get quoted.
    std::ostringstream out;
    writer.WriteFile(out, SourceFile("//foo/foo bar.cc"));
    EXPECT_EQ("\"../../foo/foo bar.cc\"", out.str());
  }
  {
    // Quotes should get blackslash-escaped.
    std::ostringstream out;
    writer.WriteFile(out, SourceFile("//foo/\"foobar\".cc"));
    EXPECT_EQ("../../foo/\\\"foobar\\\".cc", out.str());
  }
  {
    // Backslashes should get escaped on non-Windows and preserved on Windows.
    std::ostringstream out;
    writer.WriteFile(out, SourceFile("//foo\\bar.cc"));
#if defined(OS_WIN)
    EXPECT_EQ("../../foo\\bar.cc", out.str());
#else
    EXPECT_EQ("../../foo\\\\bar.cc", out.str());
#endif
  }
}

TEST(PathOutput, SlashConversion) {
  SourceDir build_dir("//out/Debug/");
  PathOutput writer(build_dir, ESCAPE_NINJA, true);
  {
    std::ostringstream out;
    writer.WriteFile(out, SourceFile("//foo/bar.cc"));
#if defined(OS_WIN)
    EXPECT_EQ("..\\..\\foo\\bar.cc", out.str());
#else
    EXPECT_EQ("../../foo/bar.cc", out.str());
#endif
  }
}

TEST(PathOutput, InhibitQuoting) {
  SourceDir build_dir("//out/Debug/");
  PathOutput writer(build_dir, ESCAPE_SHELL, false);
  writer.set_inhibit_quoting(true);
  {
    // We should get unescaped spaces in the output with no quotes.
    std::ostringstream out;
    writer.WriteFile(out, SourceFile("//foo/foo bar.cc"));
    EXPECT_EQ("../../foo/foo bar.cc", out.str());
  }
}

TEST(PathOutput, WriteDir) {
  {
    SourceDir build_dir("//out/Debug/");
    PathOutput writer(build_dir, ESCAPE_NINJA, false);
    {
      std::ostringstream out;
      writer.WriteDir(out, SourceDir("//foo/bar/"),
                      PathOutput::DIR_INCLUDE_LAST_SLASH);
      EXPECT_EQ("../../foo/bar/", out.str());
    }
    {
      std::ostringstream out;
      writer.WriteDir(out, SourceDir("//foo/bar/"),
                      PathOutput::DIR_NO_LAST_SLASH);
      EXPECT_EQ("../../foo/bar", out.str());
    }

    // Output source root dir.
    {
      std::ostringstream out;
      writer.WriteDir(out, SourceDir("//"),
                      PathOutput::DIR_INCLUDE_LAST_SLASH);
      EXPECT_EQ("../../", out.str());
    }
    {
      std::ostringstream out;
      writer.WriteDir(out, SourceDir("//"),
                      PathOutput::DIR_NO_LAST_SLASH);
      EXPECT_EQ("../..", out.str());
    }

    // Output system root dir.
    {
      std::ostringstream out;
      writer.WriteDir(out, SourceDir("/"),
                      PathOutput::DIR_INCLUDE_LAST_SLASH);
      EXPECT_EQ("/", out.str());
    }
    {
      std::ostringstream out;
      writer.WriteDir(out, SourceDir("/"),
                      PathOutput::DIR_NO_LAST_SLASH);
      EXPECT_EQ("/", out.str());
    }
  }
  {
    // Empty build dir writer.
    PathOutput root_writer(SourceDir("//"), ESCAPE_NINJA, false);
    {
      std::ostringstream out;
      root_writer.WriteDir(out, SourceDir("//"),
                           PathOutput::DIR_INCLUDE_LAST_SLASH);
      EXPECT_EQ("./", out.str());
    }
    {
      std::ostringstream out;
      root_writer.WriteDir(out, SourceDir("//"),
                           PathOutput::DIR_NO_LAST_SLASH);
      EXPECT_EQ(".", out.str());
    }
  }
}
