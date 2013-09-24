// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_aurax11.h"

#include <glib.h>
#include <X11/X.h>
#include <X11/extensions/XInput2.h>
#include <X11/XKBlib.h>

#include "base/basictypes.h"
#include "base/message_loop/message_loop.h"

namespace base {

namespace {

gboolean XSourcePrepare(GSource* source, gint* timeout_ms) {
  if (XPending(MessagePumpAuraX11::GetDefaultXDisplay()))
    *timeout_ms = 0;
  else
    *timeout_ms = -1;
  return FALSE;
}

gboolean XSourceCheck(GSource* source) {
  return XPending(MessagePumpAuraX11::GetDefaultXDisplay());
}

gboolean XSourceDispatch(GSource* source,
                         GSourceFunc unused_func,
                         gpointer data) {
  MessagePumpAuraX11* pump = static_cast<MessagePumpAuraX11*>(data);
  return pump->DispatchXEvents();
}

GSourceFuncs XSourceFuncs = {
  XSourcePrepare,
  XSourceCheck,
  XSourceDispatch,
  NULL
};

// The connection is essentially a global that's accessed through a static
// method and destroyed whenever ~MessagePumpAuraX11() is called. We do this
// for historical reasons so user code can call
// MessagePumpForUI::GetDefaultXDisplay() where MessagePumpForUI is a typedef
// to whatever type in the current build.
//
// TODO(erg): This can be changed to something more sane like
// MessagePumpAuraX11::Current()->display() once MessagePumpGtk goes away.
Display* g_xdisplay = NULL;
int g_xinput_opcode = -1;

bool InitializeXInput2Internal() {
  Display* display = MessagePumpAuraX11::GetDefaultXDisplay();
  if (!display)
    return false;

  int event, err;

  int xiopcode;
  if (!XQueryExtension(display, "XInputExtension", &xiopcode, &event, &err)) {
    DVLOG(1) << "X Input extension not available.";
    return false;
  }
  g_xinput_opcode = xiopcode;

#if defined(USE_XI2_MT)
  // USE_XI2_MT also defines the required XI2 minor minimum version.
  int major = 2, minor = USE_XI2_MT;
#else
  int major = 2, minor = 0;
#endif
  if (XIQueryVersion(display, &major, &minor) == BadRequest) {
    DVLOG(1) << "XInput2 not supported in the server.";
    return false;
  }
#if defined(USE_XI2_MT)
  if (major < 2 || (major == 2 && minor < USE_XI2_MT)) {
    DVLOG(1) << "XI version on server is " << major << "." << minor << ". "
            << "But 2." << USE_XI2_MT << " is required.";
    return false;
  }
#endif

  return true;
}

Window FindEventTarget(const NativeEvent& xev) {
  Window target = xev->xany.window;
  if (xev->type == GenericEvent &&
      static_cast<XIEvent*>(xev->xcookie.data)->extension == g_xinput_opcode) {
    target = static_cast<XIDeviceEvent*>(xev->xcookie.data)->event;
  }
  return target;
}

bool InitializeXInput2() {
  static bool xinput2_supported = InitializeXInput2Internal();
  return xinput2_supported;
}

bool InitializeXkb() {
  Display* display = MessagePumpAuraX11::GetDefaultXDisplay();
  if (!display)
    return false;

  int opcode, event, error;
  int major = XkbMajorVersion;
  int minor = XkbMinorVersion;
  if (!XkbQueryExtension(display, &opcode, &event, &error, &major, &minor)) {
    DVLOG(1) << "Xkb extension not available.";
    return false;
  }

  // Ask the server not to send KeyRelease event when the user holds down a key.
  // crbug.com/138092
  Bool supported_return;
  if (!XkbSetDetectableAutoRepeat(display, True, &supported_return)) {
    DVLOG(1) << "XKB not supported in the server.";
    return false;
  }

  return true;
}

}  // namespace

MessagePumpAuraX11::MessagePumpAuraX11() : MessagePumpGlib(),
    x_source_(NULL) {
  InitializeXInput2();
  InitializeXkb();
  InitXSource();

  // Can't put this in the initializer list because g_xdisplay may not exist
  // until after InitXSource().
  x_root_window_ = DefaultRootWindow(g_xdisplay);
}

MessagePumpAuraX11::~MessagePumpAuraX11() {
  g_source_destroy(x_source_);
  g_source_unref(x_source_);
  XCloseDisplay(g_xdisplay);
  g_xdisplay = NULL;
}

// static
Display* MessagePumpAuraX11::GetDefaultXDisplay() {
  if (!g_xdisplay)
    g_xdisplay = XOpenDisplay(NULL);
  return g_xdisplay;
}

// static
bool MessagePumpAuraX11::HasXInput2() {
  return InitializeXInput2();
}

// static
MessagePumpAuraX11* MessagePumpAuraX11::Current() {
  MessageLoopForUI* loop = MessageLoopForUI::current();
  return static_cast<MessagePumpAuraX11*>(loop->pump_ui());
}

void MessagePumpAuraX11::AddDispatcherForWindow(
    MessagePumpDispatcher* dispatcher,
    unsigned long xid) {
  dispatchers_.insert(std::make_pair(xid, dispatcher));
}

void MessagePumpAuraX11::RemoveDispatcherForWindow(unsigned long xid) {
  dispatchers_.erase(xid);
}

void MessagePumpAuraX11::AddDispatcherForRootWindow(
    MessagePumpDispatcher* dispatcher) {
  root_window_dispatchers_.AddObserver(dispatcher);
}

void MessagePumpAuraX11::RemoveDispatcherForRootWindow(
    MessagePumpDispatcher* dispatcher) {
  root_window_dispatchers_.RemoveObserver(dispatcher);
}

bool MessagePumpAuraX11::DispatchXEvents() {
  Display* display = GetDefaultXDisplay();
  DCHECK(display);
  MessagePumpDispatcher* dispatcher =
      GetDispatcher() ? GetDispatcher() : this;

  // In the general case, we want to handle all pending events before running
  // the tasks. This is what happens in the message_pump_glib case.
  while (XPending(display)) {
    XEvent xev;
    XNextEvent(display, &xev);
    if (dispatcher && ProcessXEvent(dispatcher, &xev))
      return TRUE;
  }
  return TRUE;
}

void MessagePumpAuraX11::BlockUntilWindowMapped(unsigned long xid) {
  XEvent event;

  Display* display = GetDefaultXDisplay();
  DCHECK(display);

  MessagePumpDispatcher* dispatcher =
      GetDispatcher() ? GetDispatcher() : this;

  do {
    // Block until there's a message of |event_mask| type on |w|. Then remove
    // it from the queue and stuff it in |event|.
    XWindowEvent(display, xid, StructureNotifyMask, &event);
    ProcessXEvent(dispatcher, &event);
  } while (event.type != MapNotify);
}

void MessagePumpAuraX11::InitXSource() {
  // CHECKs are to help track down crbug.com/113106.
  CHECK(!x_source_);
  Display* display = GetDefaultXDisplay();
  CHECK(display) << "Unable to get connection to X server";
  x_poll_.reset(new GPollFD());
  CHECK(x_poll_.get());
  x_poll_->fd = ConnectionNumber(display);
  x_poll_->events = G_IO_IN;

  x_source_ = g_source_new(&XSourceFuncs, sizeof(GSource));
  g_source_add_poll(x_source_, x_poll_.get());
  g_source_set_can_recurse(x_source_, TRUE);
  g_source_set_callback(x_source_, NULL, this, NULL);
  g_source_attach(x_source_, g_main_context_default());
}

bool MessagePumpAuraX11::ProcessXEvent(MessagePumpDispatcher* dispatcher,
                                       XEvent* xev) {
  bool should_quit = false;

  bool have_cookie = false;
  if (xev->type == GenericEvent &&
      XGetEventData(xev->xgeneric.display, &xev->xcookie)) {
    have_cookie = true;
  }

  if (!WillProcessXEvent(xev)) {
    if (!dispatcher->Dispatch(xev)) {
      should_quit = true;
      Quit();
    }
    DidProcessXEvent(xev);
  }

  if (have_cookie) {
    XFreeEventData(xev->xgeneric.display, &xev->xcookie);
  }

  return should_quit;
}

bool MessagePumpAuraX11::WillProcessXEvent(XEvent* xevent) {
  if (!observers().might_have_observers())
    return false;
  ObserverListBase<MessagePumpObserver>::Iterator it(observers());
  MessagePumpObserver* obs;
  while ((obs = it.GetNext()) != NULL) {
    if (obs->WillProcessEvent(xevent))
      return true;
  }
  return false;
}

void MessagePumpAuraX11::DidProcessXEvent(XEvent* xevent) {
  FOR_EACH_OBSERVER(MessagePumpObserver, observers(), DidProcessEvent(xevent));
}

MessagePumpDispatcher* MessagePumpAuraX11::GetDispatcherForXEvent(
    const NativeEvent& xev) const {
  ::Window x_window = FindEventTarget(xev);
  DispatchersMap::const_iterator it = dispatchers_.find(x_window);
  return it != dispatchers_.end() ? it->second : NULL;
}

bool MessagePumpAuraX11::Dispatch(const NativeEvent& xev) {
  // MappingNotify events (meaning that the keyboard or pointer buttons have
  // been remapped) aren't associated with a window; send them to all
  // dispatchers.
  if (xev->type == MappingNotify) {
    for (DispatchersMap::const_iterator it = dispatchers_.begin();
         it != dispatchers_.end(); ++it) {
      it->second->Dispatch(xev);
    }
    return true;
  }

  if (FindEventTarget(xev) == x_root_window_) {
    FOR_EACH_OBSERVER(MessagePumpDispatcher, root_window_dispatchers_,
                      Dispatch(xev));
    return true;
  }
  MessagePumpDispatcher* dispatcher = GetDispatcherForXEvent(xev);
  return dispatcher ? dispatcher->Dispatch(xev) : true;
}

}  // namespace base
