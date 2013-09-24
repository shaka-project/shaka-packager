# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import logging

class InspectorConsole(object):
  def __init__(self, inspector_backend):
    self._inspector_backend = inspector_backend
    self._inspector_backend.RegisterDomain(
        'Console',
        self._OnNotification,
        self._OnClose)
    self._message_output_stream = None
    self._last_message = None
    self._console_enabled = False

  def _OnNotification(self, msg):
    logging.debug('Notification: %s', json.dumps(msg, indent=2))
    if msg['method'] == 'Console.messageAdded':
      if msg['params']['message']['url'] == 'chrome://newtab/':
        return
      self._last_message = 'At %s:%i: %s' % (
        msg['params']['message']['url'],
        msg['params']['message']['line'],
        msg['params']['message']['text'])
      if self._message_output_stream:
        self._message_output_stream.write(
          '%s\n' % self._last_message)

    elif msg['method'] == 'Console.messageRepeatCountUpdated':
      if self._message_output_stream:
        self._message_output_stream.write(
          '%s\n' % self._last_message)

  def _OnClose(self):
    pass

  # False positive in PyLint 0.25.1: http://www.logilab.org/89092
  @property
  def message_output_stream(self):  # pylint: disable=E0202
    return self._message_output_stream

  @message_output_stream.setter
  def message_output_stream(self, stream):  # pylint: disable=E0202
    self._message_output_stream = stream
    self._UpdateConsoleEnabledState()

  def _UpdateConsoleEnabledState(self):
    enabled = self._message_output_stream != None
    if enabled == self._console_enabled:
      return

    if enabled:
      method_name = 'enable'
    else:
      method_name = 'disable'
    self._inspector_backend.SyncRequest({
        'method': 'Console.%s' % method_name
        })
    self._console_enabled = enabled
