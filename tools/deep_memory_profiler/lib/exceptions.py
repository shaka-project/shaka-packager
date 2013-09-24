# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class EmptyDumpException(Exception):
  def __str__(self):
    return repr(self.args[0])


class ParsingException(Exception):
  def __str__(self):
    return repr(self.args[0])


class InvalidDumpException(ParsingException):
  def __str__(self):
    return "invalid heap profile dump: %s" % repr(self.args[0])


class ObsoleteDumpVersionException(ParsingException):
  def __str__(self):
    return "obsolete heap profile dump version: %s" % repr(self.args[0])
