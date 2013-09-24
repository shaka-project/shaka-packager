// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_registry_simple.h"

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

PrefRegistrySimple::PrefRegistrySimple() {
}

PrefRegistrySimple::~PrefRegistrySimple() {
}

void PrefRegistrySimple::RegisterBooleanPref(const char* path,
                                             bool default_value) {
  RegisterPreference(path, base::Value::CreateBooleanValue(default_value));
}

void PrefRegistrySimple::RegisterIntegerPref(const char* path,
                                             int default_value) {
  RegisterPreference(path, base::Value::CreateIntegerValue(default_value));
}

void PrefRegistrySimple::RegisterDoublePref(const char* path,
                                            double default_value) {
  RegisterPreference(path, base::Value::CreateDoubleValue(default_value));
}

void PrefRegistrySimple::RegisterStringPref(const char* path,
                                            const std::string& default_value) {
  RegisterPreference(path, base::Value::CreateStringValue(default_value));
}

void PrefRegistrySimple::RegisterFilePathPref(
    const char* path,
    const base::FilePath& default_value) {
  RegisterPreference(path,
                     base::Value::CreateStringValue(default_value.value()));
}

void PrefRegistrySimple::RegisterListPref(const char* path) {
  RegisterPreference(path, new base::ListValue());
}

void PrefRegistrySimple::RegisterListPref(const char* path,
                                          base::ListValue* default_value) {
  RegisterPreference(path, default_value);
}

void PrefRegistrySimple::RegisterDictionaryPref(const char* path) {
  RegisterPreference(path, new base::DictionaryValue());
}

void PrefRegistrySimple::RegisterDictionaryPref(
    const char* path,
    base::DictionaryValue* default_value) {
  RegisterPreference(path, default_value);
}

void PrefRegistrySimple::RegisterInt64Pref(const char* path,
                                           int64 default_value) {
  RegisterPreference(
      path, base::Value::CreateStringValue(base::Int64ToString(default_value)));
}
