// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/value_map_pref_store.h"

#include <algorithm>

#include "base/stl_util.h"
#include "base/values.h"

ValueMapPrefStore::ValueMapPrefStore() {}

bool ValueMapPrefStore::GetValue(const std::string& key,
                                 const base::Value** value) const {
  return prefs_.GetValue(key, value);
}

void ValueMapPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void ValueMapPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

size_t ValueMapPrefStore::NumberOfObservers() const {
  return observers_.size();
}

ValueMapPrefStore::~ValueMapPrefStore() {}

void ValueMapPrefStore::SetValue(const std::string& key, base::Value* value) {
  if (prefs_.SetValue(key, value))
    FOR_EACH_OBSERVER(Observer, observers_, OnPrefValueChanged(key));
}

void ValueMapPrefStore::RemoveValue(const std::string& key) {
  if (prefs_.RemoveValue(key))
    FOR_EACH_OBSERVER(Observer, observers_, OnPrefValueChanged(key));
}

void ValueMapPrefStore::NotifyInitializationCompleted() {
  FOR_EACH_OBSERVER(Observer, observers_, OnInitializationCompleted(true));
}
