// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <iostream>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

// Usage: just run in the directory where you want your test source root to be.

int files_written = 0;
int targets_written = 0;

base::FilePath UTF8ToFilePath(const std::string& s) {
#if defined(OS_WIN)
  return base::FilePath(UTF8ToWide(s));
#else
  return base::FilePath(s);
#endif
}

std::string FilePathToUTF8(const base::FilePath& path) {
#if defined(OS_WIN)
  return WideToUTF8(path.value());
#else
  return path.value();
#endif
}

base::FilePath RepoPathToPathName(const std::vector<int>& repo_path) {
  base::FilePath ret;
  for (size_t i = 0; i < repo_path.size(); i++) {
    ret = ret.Append(UTF8ToFilePath(base::IntToString(repo_path[i])));
  }
  return ret;
}

std::string TargetIndexToLetter(int target_index) {
  char ret[2];
  ret[0] = 'a' + target_index;
  ret[1] = 0;
  return ret;
}

std::string RepoPathToTargetName(const std::vector<int>& repo_path,
                                 int target_index) {
  std::string ret;
  for (size_t i = 0; i < repo_path.size(); i++) {
    if (i != 0)
      ret.push_back('_');
    ret.append(base::IntToString(repo_path[i]));
  }
  ret += TargetIndexToLetter(target_index);
  return ret;
}

std::string RepoPathToFullTargetName(const std::vector<int>& repo_path,
                                 int target_index) {
  std::string ret;
  for (size_t i = 0; i < repo_path.size(); i++) {
    ret.push_back('/');
    ret.append(base::IntToString(repo_path[i]));
  }

  ret += ":" + RepoPathToTargetName(repo_path, target_index);
  return ret;
}

void WriteLevel(const std::vector<int>& repo_path,
                int spread,
                int max_depth,
                int targets_per_level,
                int files_per_target) {
  base::FilePath dirname = RepoPathToPathName(repo_path);
  base::FilePath filename = dirname.AppendASCII("BUILD.gn");
  std::cout << "Writing " << FilePathToUTF8(filename) << "\n";

  // Don't keep the file open while recursing.
  {
    file_util::CreateDirectory(dirname);

    std::ofstream file;
    file.open(FilePathToUTF8(filename).c_str(),
              std::ios_base::out | std::ios_base::binary);
    files_written++;

    for (int i = 0; i < targets_per_level; i++) {
      targets_written++;
      file << "executable(\"" << RepoPathToTargetName(repo_path, i)
           << "\") {\n";
      file << "  sources = [\n";
      for (int f = 0; f < files_per_target; f++)
        file << "    \"" << base::IntToString(f) << ".cc\",\n";

      if (repo_path.size() < (size_t)max_depth) {
        file << "  ]\n";
        file << "  deps = [\n";
        for (int d = 0; d < spread; d++) {
          std::vector<int> cur = repo_path;
          cur.push_back(d);
          for (int t = 0; t < targets_per_level; t++)
            file << "    \"" << RepoPathToFullTargetName(cur, t) << "\",\n";
        }
      }
      file << "  ]\n}\n\n";
    }
  }
  if (repo_path.size() < (size_t)max_depth) {
    // Recursively generate subdirs.
    for (int i = 0; i < spread; i++) {
      std::vector<int> cur = repo_path;
      cur.push_back(i);
      WriteLevel(cur, spread, max_depth, targets_per_level, files_per_target);
    }
  }
}

int main() {
  WriteLevel(std::vector<int>(), 5, 4, 3, 50);  // 781 files, 2343 targets
  //WriteLevel(std::vector<int>(), 6, 4, 2, 50);
  std::cout << "Wrote " << files_written << " files and "
            << targets_written << " targets.\n";
  return 0;
}
