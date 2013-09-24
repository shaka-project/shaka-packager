// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_INI_PARSER_H_
#define BASE_INI_PARSER_H_

#include <string>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/values.h"

namespace base {

// Parses INI files in a string. Users should in inherit from this class.
// This is a very basic INI parser with these characteristics:
//  - Ignores blank lines.
//  - Ignores comment lines beginning with '#' or ';'.
//  - Duplicate key names in the same section will simply cause repeated calls
//    to HandleTriplet with the same |section| and |key| parameters.
//  - No escape characters supported.
//  - Global properties result in calls to HandleTriplet with an empty string in
//    the |section| argument.
//  - Section headers begin with a '[' character. It is recommended, but
//    not required to close the header bracket with a ']' character. All
//    characters after a closing ']' character is ignored.
//  - Key value pairs are indicated with an '=' character. Whitespace is not
//    ignored. Quoting is not supported. Everything before the first '='
//    is considered the |key|, and everything after is the |value|.
class BASE_EXPORT INIParser {
 public:
  INIParser();
  virtual ~INIParser();

  // May only be called once per instance.
  void Parse(const std::string& content);

 private:
  virtual void HandleTriplet(const std::string& section,
                             const std::string& key,
                             const std::string& value) = 0;

  bool used_;
};

// Parsed values are stored as strings at the "section.key" path. Triplets with
// |section| or |key| parameters containing '.' are ignored.
class BASE_EXPORT DictionaryValueINIParser : public INIParser {
 public:
  DictionaryValueINIParser();
  virtual ~DictionaryValueINIParser();

  const DictionaryValue& root() const { return root_; }

 private:
  // INIParser implementation.
  virtual void HandleTriplet(const std::string& section,
                             const std::string& key,
                             const std::string& value) OVERRIDE;

  DictionaryValue root_;

  DISALLOW_COPY_AND_ASSIGN(DictionaryValueINIParser);
};

}  // namespace base

#endif  // BASE_INI_PARSER_H_
