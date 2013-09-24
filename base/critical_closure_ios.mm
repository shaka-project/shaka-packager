// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/critical_closure.h"

#import <UIKit/UIKit.h>

#include "base/bind.h"
#include "base/ios/scoped_critical_action.h"
#include "base/memory/ref_counted.h"

namespace {

// This class wraps a closure so it can continue to run for a period of time
// when the application goes to the background by using
// |base::ios::ScopedCriticalAction|.
class CriticalClosure : public base::RefCountedThreadSafe<CriticalClosure> {
 public:
  explicit CriticalClosure(base::Closure* closure) : closure_(closure) {
  }

  void Run() {
    closure_->Run();
  }

 private:
  friend class base::RefCountedThreadSafe<CriticalClosure>;

  virtual ~CriticalClosure() {}

  base::ios::ScopedCriticalAction criticial_action_;
  scoped_ptr<base::Closure> closure_;

  DISALLOW_COPY_AND_ASSIGN(CriticalClosure);
};

}  // namespace

namespace base {

base::Closure MakeCriticalClosure(const base::Closure& closure) {
  DCHECK([[UIDevice currentDevice] isMultitaskingSupported]);
  scoped_refptr<CriticalClosure> critical_closure(
      new CriticalClosure(new base::Closure(closure)));
  return base::Bind(&CriticalClosure::Run, critical_closure.get());
}

}  // namespace base
