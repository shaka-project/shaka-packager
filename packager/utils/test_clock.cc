// Copyright 2023 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/utils/test_clock.h>

#include <ctime>
#include <iostream>
#include <string>

#include <absl/strings/str_split.h>
#include <absl/strings/string_view.h>

std::tm parseISO8601(const std::string& date_string) {
  std::tm tm = {};
  std::vector<absl::string_view> date_time_parts =
      absl::StrSplit(date_string, 'T');
  if (date_time_parts.size() == 2) {
    std::vector<absl::string_view> date_parts =
        absl::StrSplit(date_time_parts[0], '-');
    std::vector<absl::string_view> time_parts =
        absl::StrSplit(date_time_parts[1], ':');
    if (date_parts.size() == 3 && time_parts.size() == 3) {
      tm.tm_year = std::stoi(std::string(date_parts[0])) - 1900;
      tm.tm_mon = std::stoi(std::string(date_parts[1])) - 1;
      tm.tm_mday = std::stoi(std::string(date_parts[2]));
      tm.tm_hour = std::stoi(std::string(time_parts[0]));
      tm.tm_min = std::stoi(std::string(time_parts[1]));
      tm.tm_sec = std::stoi(std::string(time_parts[2]));
    }
  }
  return tm;
}

shaka::TestClock::TestClock(std::string utc_time_8601) {
  std::tm tm = parseISO8601(utc_time_8601);
  std::time_t utc_time_t = std::mktime(&tm);
  std::time_t offset = utc_time_t - std::mktime(std::gmtime(&utc_time_t));
  mock_time_ = std::chrono::system_clock::from_time_t(utc_time_t + offset);
}
