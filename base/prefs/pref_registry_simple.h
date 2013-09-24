// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PREFS_PREF_REGISTRY_SIMPLE_H_
#define BASE_PREFS_PREF_REGISTRY_SIMPLE_H_

#include <string>

#include "base/prefs/base_prefs_export.h"
#include "base/prefs/pref_registry.h"

namespace base {
class DictionaryValue;
class FilePath;
class ListValue;
}

// A simple implementation of PrefRegistry.
class BASE_PREFS_EXPORT PrefRegistrySimple : public PrefRegistry {
 public:
  PrefRegistrySimple();

  void RegisterBooleanPref(const char* path, bool default_value);
  void RegisterIntegerPref(const char* path, int default_value);
  void RegisterDoublePref(const char* path, double default_value);
  void RegisterStringPref(const char* path, const std::string& default_value);
  void RegisterFilePathPref(const char* path,
                            const base::FilePath& default_value);
  void RegisterListPref(const char* path);
  void RegisterDictionaryPref(const char* path);
  void RegisterListPref(const char* path, base::ListValue* default_value);
  void RegisterDictionaryPref(const char* path,
                              base::DictionaryValue* default_value);
  void RegisterInt64Pref(const char* path,
                         int64 default_value);

 private:
  virtual ~PrefRegistrySimple();

  DISALLOW_COPY_AND_ASSIGN(PrefRegistrySimple);
};

#endif  // BASE_PREFS_PREF_REGISTRY_SIMPLE_H_
