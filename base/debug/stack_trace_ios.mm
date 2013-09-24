// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#include <mach/task.h>
#include <stdio.h>

#include "base/logging.h"

// This is just enough of a shim to let the support needed by test_support
// link.

namespace base {
namespace debug {

namespace {

void StackDumpSignalHandler(int signal) {
  // TODO(phajdan.jr): Fix async-signal unsafety.
  LOG(ERROR) << "Received signal " << signal;
  NSArray *stack_symbols = [NSThread callStackSymbols];
  for (NSString* stack_symbol in stack_symbols) {
    fprintf(stderr, "\t%s\n", [stack_symbol UTF8String]);
  }
  _exit(1);
}

}  // namespace

// TODO(phajdan.jr): Deduplicate, see copy in stack_trace_posix.cc.
bool EnableInProcessStackDumping() {
  // When running in an application, our code typically expects SIGPIPE
  // to be ignored.  Therefore, when testing that same code, it should run
  // with SIGPIPE ignored as well.
  struct sigaction action;
  action.sa_handler = SIG_IGN;
  action.sa_flags = 0;
  sigemptyset(&action.sa_mask);
  bool success = (sigaction(SIGPIPE, &action, NULL) == 0);

  success &= (signal(SIGILL, &StackDumpSignalHandler) != SIG_ERR);
  success &= (signal(SIGABRT, &StackDumpSignalHandler) != SIG_ERR);
  success &= (signal(SIGFPE, &StackDumpSignalHandler) != SIG_ERR);
  success &= (signal(SIGBUS, &StackDumpSignalHandler) != SIG_ERR);
  success &= (signal(SIGSEGV, &StackDumpSignalHandler) != SIG_ERR);
  success &= (signal(SIGSYS, &StackDumpSignalHandler) != SIG_ERR);

  return success;
}

}  // namespace debug
}  // namespace base
