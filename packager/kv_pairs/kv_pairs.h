// Copyright 2022 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace shaka {

typedef std::pair<std::string, std::string> KVPair;
std::vector<KVPair> SplitStringIntoKeyValuePairs(std::string_view str,
                                                 char kv_separator = '=',
                                                 char list_separator = '&');

}  // namespace shaka
