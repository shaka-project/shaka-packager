// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_BIND_OBJC_BLOCK_H_
#define BASE_MAC_BIND_OBJC_BLOCK_H_

#include <Block.h>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/mac/scoped_block.h"

// BindBlock builds a callback from an Objective-C block. Example usages:
//
// Closure closure = BindBlock(^{DoSomething();});
// Callback<int(void)> callback = BindBlock(^{return 42;});

namespace base {

namespace internal {

// Helper functions to run the block contained in the parameter.
template<typename R>
R RunBlock(base::mac::ScopedBlock<R(^)()> block) {
  R(^extracted_block)() = block.get();
  return extracted_block();
}

template<typename R, typename A1>
R RunBlock(base::mac::ScopedBlock<R(^)(A1)> block, A1 a) {
  R(^extracted_block)(A1) = block.get();
  return extracted_block(a);
}

}  // namespace internal

// Construct a callback with no argument from an objective-C block.
template<typename R>
base::Callback<R(void)> BindBlock(R(^block)()) {
  return base::Bind(&base::internal::RunBlock<R>,
                    base::mac::ScopedBlock<R(^)()>(Block_copy(block)));
}

// Construct a callback with one argument from an objective-C block.
template<typename R, typename A1>
base::Callback<R(A1)> BindBlock(R(^block)(A1)) {
  return base::Bind(&base::internal::RunBlock<R, A1>,
                    base::mac::ScopedBlock<R(^)(A1)>(Block_copy(block)));
}

}  // namespace base

#endif  // BASE_MAC_BIND_OBJC_BLOCK_H_
