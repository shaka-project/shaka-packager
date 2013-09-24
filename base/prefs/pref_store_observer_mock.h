// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PREFS_PREF_STORE_OBSERVER_MOCK_H_
#define BASE_PREFS_PREF_STORE_OBSERVER_MOCK_H_

#include "base/basictypes.h"
#include "base/prefs/pref_store.h"
#include "testing/gmock/include/gmock/gmock.h"

// A gmock-ified implementation of PrefStore::Observer.
class PrefStoreObserverMock : public PrefStore::Observer {
 public:
  PrefStoreObserverMock();
  virtual ~PrefStoreObserverMock();

  MOCK_METHOD1(OnPrefValueChanged, void(const std::string&));
  MOCK_METHOD1(OnInitializationCompleted, void(bool));

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefStoreObserverMock);
};

#endif  // BASE_PREFS_PREF_STORE_OBSERVER_MOCK_H_
