// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_value_map.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/values.h"

PrefValueMap::PrefValueMap() {}

PrefValueMap::~PrefValueMap() {
  Clear();
}

bool PrefValueMap::GetValue(const std::string& key,
                            const base::Value** value) const {
  const Map::const_iterator entry = prefs_.find(key);
  if (entry != prefs_.end()) {
    if (value)
      *value = entry->second;
    return true;
  }

  return false;
}

bool PrefValueMap::GetValue(const std::string& key, base::Value** value) {
  const Map::const_iterator entry = prefs_.find(key);
  if (entry != prefs_.end()) {
    if (value)
      *value = entry->second;
    return true;
  }

  return false;
}

bool PrefValueMap::SetValue(const std::string& key, base::Value* value) {
  DCHECK(value);
  scoped_ptr<base::Value> value_ptr(value);
  const Map::iterator entry = prefs_.find(key);
  if (entry != prefs_.end()) {
    if (base::Value::Equals(entry->second, value))
      return false;
    delete entry->second;
    entry->second = value_ptr.release();
  } else {
    prefs_[key] = value_ptr.release();
  }

  return true;
}

bool PrefValueMap::RemoveValue(const std::string& key) {
  const Map::iterator entry = prefs_.find(key);
  if (entry != prefs_.end()) {
    delete entry->second;
    prefs_.erase(entry);
    return true;
  }

  return false;
}

void PrefValueMap::Clear() {
  STLDeleteValues(&prefs_);
  prefs_.clear();
}

void PrefValueMap::Swap(PrefValueMap* other) {
  prefs_.swap(other->prefs_);
}

PrefValueMap::iterator PrefValueMap::begin() {
  return prefs_.begin();
}

PrefValueMap::iterator PrefValueMap::end() {
  return prefs_.end();
}

PrefValueMap::const_iterator PrefValueMap::begin() const {
  return prefs_.begin();
}

PrefValueMap::const_iterator PrefValueMap::end() const {
  return prefs_.end();
}

bool PrefValueMap::GetBoolean(const std::string& key,
                              bool* value) const {
  const base::Value* stored_value = NULL;
  return GetValue(key, &stored_value) && stored_value->GetAsBoolean(value);
}

void PrefValueMap::SetBoolean(const std::string& key, bool value) {
  SetValue(key, new base::FundamentalValue(value));
}

bool PrefValueMap::GetString(const std::string& key,
                             std::string* value) const {
  const base::Value* stored_value = NULL;
  return GetValue(key, &stored_value) && stored_value->GetAsString(value);
}

void PrefValueMap::SetString(const std::string& key,
                             const std::string& value) {
  SetValue(key, new base::StringValue(value));
}

bool PrefValueMap::GetInteger(const std::string& key, int* value) const {
  const base::Value* stored_value = NULL;
  return GetValue(key, &stored_value) && stored_value->GetAsInteger(value);
}

void PrefValueMap::SetInteger(const std::string& key, const int value) {
  SetValue(key, new base::FundamentalValue(value));
}

void PrefValueMap::GetDifferingKeys(
    const PrefValueMap* other,
    std::vector<std::string>* differing_keys) const {
  differing_keys->clear();

  // Walk over the maps in lockstep, adding everything that is different.
  Map::const_iterator this_pref(prefs_.begin());
  Map::const_iterator other_pref(other->prefs_.begin());
  while (this_pref != prefs_.end() && other_pref != other->prefs_.end()) {
    const int diff = this_pref->first.compare(other_pref->first);
    if (diff == 0) {
      if (!this_pref->second->Equals(other_pref->second))
        differing_keys->push_back(this_pref->first);
      ++this_pref;
      ++other_pref;
    } else if (diff < 0) {
      differing_keys->push_back(this_pref->first);
      ++this_pref;
    } else if (diff > 0) {
      differing_keys->push_back(other_pref->first);
      ++other_pref;
    }
  }

  // Add the remaining entries.
  for ( ; this_pref != prefs_.end(); ++this_pref)
      differing_keys->push_back(this_pref->first);
  for ( ; other_pref != other->prefs_.end(); ++other_pref)
      differing_keys->push_back(other_pref->first);
}
