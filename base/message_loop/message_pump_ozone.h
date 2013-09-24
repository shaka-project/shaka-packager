// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_OZONE_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_OZONE_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_pump_dispatcher.h"
#include "base/message_loop/message_pump_libevent.h"
#include "base/message_loop/message_pump_observer.h"
#include "base/observer_list.h"

namespace base {

// This class implements a message-pump for processing events from input devices
// Refer to MessagePump for further documentation.
class BASE_EXPORT MessagePumpOzone : public MessagePumpLibevent,
                                     public MessagePumpDispatcher {
 public:
  MessagePumpOzone();
  virtual ~MessagePumpOzone();

  // Returns the UI message pump.
  static MessagePumpOzone* Current();

  // Add/Remove the root window dispatcher.
  void AddDispatcherForRootWindow(MessagePumpDispatcher* dispatcher);
  void RemoveDispatcherForRootWindow(MessagePumpDispatcher* dispatcher);

  void RunWithDispatcher(Delegate* delegate, MessagePumpDispatcher* dispatcher);

  // Add / remove an Observer, which will start receiving notifications
  // immediately.
  void AddObserver(MessagePumpObserver* observer);
  void RemoveObserver(MessagePumpObserver* observer);

  // Overridden from MessagePumpDispatcher.
  virtual bool Dispatch(const NativeEvent& event) OVERRIDE;

 private:
  std::vector<MessagePumpDispatcher*> dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpOzone);
};

typedef MessagePumpOzone MessagePumpForUI;

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_OZONE_H_
