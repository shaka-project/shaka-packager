// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_gtk.h"

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "base/debug/trace_event.h"
#include "base/profiler/scoped_profile.h"

namespace base {

namespace {

const char* EventToTypeString(const GdkEvent* event) {
  switch (event->type) {
    case GDK_NOTHING:           return "GDK_NOTHING";
    case GDK_DELETE:            return "GDK_DELETE";
    case GDK_DESTROY:           return "GDK_DESTROY";
    case GDK_EXPOSE:            return "GDK_EXPOSE";
    case GDK_MOTION_NOTIFY:     return "GDK_MOTION_NOTIFY";
    case GDK_BUTTON_PRESS:      return "GDK_BUTTON_PRESS";
    case GDK_2BUTTON_PRESS:     return "GDK_2BUTTON_PRESS";
    case GDK_3BUTTON_PRESS:     return "GDK_3BUTTON_PRESS";
    case GDK_BUTTON_RELEASE:    return "GDK_BUTTON_RELEASE";
    case GDK_KEY_PRESS:         return "GDK_KEY_PRESS";
    case GDK_KEY_RELEASE:       return "GDK_KEY_RELEASE";
    case GDK_ENTER_NOTIFY:      return "GDK_ENTER_NOTIFY";
    case GDK_LEAVE_NOTIFY:      return "GDK_LEAVE_NOTIFY";
    case GDK_FOCUS_CHANGE:      return "GDK_FOCUS_CHANGE";
    case GDK_CONFIGURE:         return "GDK_CONFIGURE";
    case GDK_MAP:               return "GDK_MAP";
    case GDK_UNMAP:             return "GDK_UNMAP";
    case GDK_PROPERTY_NOTIFY:   return "GDK_PROPERTY_NOTIFY";
    case GDK_SELECTION_CLEAR:   return "GDK_SELECTION_CLEAR";
    case GDK_SELECTION_REQUEST: return "GDK_SELECTION_REQUEST";
    case GDK_SELECTION_NOTIFY:  return "GDK_SELECTION_NOTIFY";
    case GDK_PROXIMITY_IN:      return "GDK_PROXIMITY_IN";
    case GDK_PROXIMITY_OUT:     return "GDK_PROXIMITY_OUT";
    case GDK_DRAG_ENTER:        return "GDK_DRAG_ENTER";
    case GDK_DRAG_LEAVE:        return "GDK_DRAG_LEAVE";
    case GDK_DRAG_MOTION:       return "GDK_DRAG_MOTION";
    case GDK_DRAG_STATUS:       return "GDK_DRAG_STATUS";
    case GDK_DROP_START:        return "GDK_DROP_START";
    case GDK_DROP_FINISHED:     return "GDK_DROP_FINISHED";
    case GDK_CLIENT_EVENT:      return "GDK_CLIENT_EVENT";
    case GDK_VISIBILITY_NOTIFY: return "GDK_VISIBILITY_NOTIFY";
    case GDK_NO_EXPOSE:         return "GDK_NO_EXPOSE";
    case GDK_SCROLL:            return "GDK_SCROLL";
    case GDK_WINDOW_STATE:      return "GDK_WINDOW_STATE";
    case GDK_SETTING:           return "GDK_SETTING";
    case GDK_OWNER_CHANGE:      return "GDK_OWNER_CHANGE";
    case GDK_GRAB_BROKEN:       return "GDK_GRAB_BROKEN";
    case GDK_DAMAGE:            return "GDK_DAMAGE";
    default:
      return "Unknown Gdk Event";
  }
}

}  // namespace

MessagePumpGtk::MessagePumpGtk() : MessagePumpGlib() {
  gdk_event_handler_set(&EventDispatcher, this, NULL);
}

MessagePumpGtk::~MessagePumpGtk() {
  gdk_event_handler_set(reinterpret_cast<GdkEventFunc>(gtk_main_do_event),
                        this, NULL);
}

void MessagePumpGtk::DispatchEvents(GdkEvent* event) {
  UNSHIPPED_TRACE_EVENT1("task", "MessagePumpGtk::DispatchEvents",
                         "type", EventToTypeString(event));

  WillProcessEvent(event);

  MessagePumpDispatcher* dispatcher = GetDispatcher();
  if (!dispatcher)
    gtk_main_do_event(event);
  else if (!dispatcher->Dispatch(event))
    Quit();

  DidProcessEvent(event);
}

// static
Display* MessagePumpGtk::GetDefaultXDisplay() {
  static GdkDisplay* display = gdk_display_get_default();
  if (!display) {
    // GTK / GDK has not been initialized, which is a decision we wish to
    // support, for example for the GPU process.
    static Display* xdisplay = XOpenDisplay(NULL);
    return xdisplay;
  }
  return GDK_DISPLAY_XDISPLAY(display);
}

void MessagePumpGtk::WillProcessEvent(GdkEvent* event) {
  FOR_EACH_OBSERVER(MessagePumpObserver, observers(), WillProcessEvent(event));
}

void MessagePumpGtk::DidProcessEvent(GdkEvent* event) {
  FOR_EACH_OBSERVER(MessagePumpObserver, observers(), DidProcessEvent(event));
}

// static
void MessagePumpGtk::EventDispatcher(GdkEvent* event, gpointer data) {
  MessagePumpGtk* message_pump = reinterpret_cast<MessagePumpGtk*>(data);
  message_pump->DispatchEvents(event);
}

}  // namespace base
