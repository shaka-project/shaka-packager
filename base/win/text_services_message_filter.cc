// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/text_services_message_filter.h"

namespace base {
namespace win {

TextServicesMessageFilter::TextServicesMessageFilter()
    : client_id_(TF_CLIENTID_NULL),
      is_initialized_(false) {
}

TextServicesMessageFilter::~TextServicesMessageFilter() {
  if (is_initialized_)
    thread_mgr_->Deactivate();
}

bool TextServicesMessageFilter::Init() {
  if (FAILED(thread_mgr_.CreateInstance(CLSID_TF_ThreadMgr)))
    return false;

  if (FAILED(message_pump_.QueryFrom(thread_mgr_)))
    return false;

  if (FAILED(keystroke_mgr_.QueryFrom(thread_mgr_)))
    return false;

  if (FAILED(thread_mgr_->Activate(&client_id_)))
    return false;

  is_initialized_ = true;
  return is_initialized_;
}

// Wraps for ITfMessagePump::PeekMessage with win32 PeekMessage signature.
// Obtains messages from application message queue.
BOOL TextServicesMessageFilter::DoPeekMessage(MSG* msg,
                                              HWND window_handle,
                                              UINT msg_filter_min,
                                              UINT msg_filter_max,
                                              UINT remove_msg) {
  BOOL result = FALSE;
  if (FAILED(message_pump_->PeekMessage(msg, window_handle,
                                        msg_filter_min, msg_filter_max,
                                        remove_msg, &result))) {
    result = FALSE;
  }

  return result;
}

// Sends message to Text Service Manager.
// The message will be used to input composition text.
// Returns true if |message| was consumed by text service manager.
bool TextServicesMessageFilter::ProcessMessage(const MSG& msg) {
  if (msg.message == WM_KEYDOWN) {
    BOOL eaten = FALSE;
    HRESULT hr = keystroke_mgr_->TestKeyDown(msg.wParam, msg.lParam, &eaten);
    if (FAILED(hr) && !eaten)
      return false;
    eaten = FALSE;
    hr = keystroke_mgr_->KeyDown(msg.wParam, msg.lParam, &eaten);
    return (SUCCEEDED(hr) && !!eaten);
  }

  if (msg.message == WM_KEYUP) {
    BOOL eaten = FALSE;
    HRESULT hr = keystroke_mgr_->TestKeyUp(msg.wParam, msg.lParam, &eaten);
    if (FAILED(hr) && !eaten)
      return false;
    eaten = FALSE;
    hr = keystroke_mgr_->KeyUp(msg.wParam, msg.lParam, &eaten);
    return (SUCCEEDED(hr) && !!eaten);
  }

  return false;
}

}  // namespace win
}  // namespace base
