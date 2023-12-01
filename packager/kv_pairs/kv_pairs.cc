// Copyright 2022 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/kv_pairs/kv_pairs.h>

#include <absl/strings/str_split.h>

namespace shaka {

std::vector<KVPair> SplitStringIntoKeyValuePairs(std::string_view str,
                                                 char kv_separator,
                                                 char list_separator) {
  std::vector<KVPair> kv_pairs;

  // Edge case: 0 pairs.
  if (str.size() == 0) {
    return kv_pairs;
  }

  std::vector<std::string> kv_strings = absl::StrSplit(str, list_separator);
  for (const auto& kv_string : kv_strings) {
    KVPair pair = absl::StrSplit(kv_string, absl::MaxSplits(kv_separator, 1));
    kv_pairs.push_back(pair);
  }

  return kv_pairs;
}

}  // namespace shaka
