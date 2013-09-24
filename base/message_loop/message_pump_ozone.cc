// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_ozone.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"

namespace base {

MessagePumpOzone::MessagePumpOzone()
    : MessagePumpLibevent() {
}

MessagePumpOzone::~MessagePumpOzone() {
}

void MessagePumpOzone::AddObserver(MessagePumpObserver* /* observer */) {
  NOTIMPLEMENTED();
}

void MessagePumpOzone::RemoveObserver(MessagePumpObserver* /* observer */) {
  NOTIMPLEMENTED();
}

// static
MessagePumpOzone* MessagePumpOzone::Current() {
  MessageLoopForUI* loop = MessageLoopForUI::current();
  return static_cast<MessagePumpOzone*>(loop->pump_ui());
}

void MessagePumpOzone::AddDispatcherForRootWindow(
    MessagePumpDispatcher* dispatcher) {
  // Only one root window is supported.
  DCHECK(dispatcher_.size() == 0);
  dispatcher_.insert(dispatcher_.begin(),dispatcher);
}

void MessagePumpOzone::RemoveDispatcherForRootWindow(
      MessagePumpDispatcher* dispatcher) {
  DCHECK(dispatcher_.size() == 1);
  dispatcher_.pop_back();
}

bool MessagePumpOzone::Dispatch(const NativeEvent& dev) {
  if (dispatcher_.size() > 0)
    return dispatcher_[0]->Dispatch(dev);
  else
    return true;
}

// This code assumes that the caller tracks the lifetime of the |dispatcher|.
void MessagePumpOzone::RunWithDispatcher(
    Delegate* delegate, MessagePumpDispatcher* dispatcher) {
  dispatcher_.push_back(dispatcher);
  Run(delegate);
  dispatcher_.pop_back();
}

}  // namespace base
