# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import re
import time
import urlparse

from telemetry.core import util

def _UrlPathJoin(*args):
  """Joins each path in |args| for insertion into a URL path.

  This is distinct from os.path.join in that:
  1. Forward slashes are always used.
  2. Paths beginning with '/' are not treated as absolute.

  For example:
    _UrlPathJoin('a', 'b') => 'a/b'
    _UrlPathJoin('a/', 'b') => 'a/b'
    _UrlPathJoin('a', '/b') => 'a/b'
    _UrlPathJoin('a/', '/b') => 'a/b'
  """
  if not args:
    return ''
  if len(args) == 1:
    return str(args[0])
  else:
    args = [str(arg).replace('\\', '/') for arg in args]
    work = [args[0]]
    for arg in args[1:]:
      if not arg:
        continue
      if arg.startswith('/'):
        work.append(arg[1:])
      else:
        work.append(arg)
    joined = reduce(os.path.join, work)
  return joined.replace('\\', '/')

class Page(object):
  def __init__(self, url, page_set, attributes=None, base_dir=None):
    parsed_url = urlparse.urlparse(url)
    if not parsed_url.scheme:
      abspath = os.path.abspath(os.path.join(base_dir, parsed_url.path))
      if os.path.exists(abspath):
        url = 'file://%s' % os.path.abspath(os.path.join(base_dir, url))
      else:
        raise Exception('URLs must be fully qualified: %s' % url)
    self.url = url
    self.page_set = page_set
    self.base_dir = base_dir

    # These attributes can be set dynamically by the page.
    self.credentials = None
    self.disabled = False
    self.script_to_evaluate_on_commit = None

    if attributes:
      for k, v in attributes.iteritems():
        setattr(self, k, v)

  def __getattr__(self, name):
    if self.page_set and hasattr(self.page_set, name):
      return getattr(self.page_set, name)

    raise AttributeError()

  @property
  def is_file(self):
    parsed_url = urlparse.urlparse(self.url)
    return parsed_url.scheme == 'file'

  @property
  def is_local(self):
    parsed_url = urlparse.urlparse(self.url)
    return parsed_url.scheme == 'file' or parsed_url.scheme == 'chrome'

  @property
  def serving_dirs_and_file(self):
    parsed_url = urlparse.urlparse(self.url)
    path = _UrlPathJoin(self.base_dir, parsed_url.netloc, parsed_url.path)

    if hasattr(self.page_set, 'serving_dirs'):
      url_base_dir = os.path.commonprefix(self.page_set.serving_dirs)
      base_path = _UrlPathJoin(self.base_dir, url_base_dir)
      return ([_UrlPathJoin(self.base_dir, d)
               for d in self.page_set.serving_dirs],
              path.replace(base_path, ''))

    return os.path.split(path)

  # A version of this page's URL that's safe to use as a filename.
  @property
  def url_as_file_safe_name(self):
    # Just replace all special characters in the url with underscore.
    return re.sub('[^a-zA-Z0-9]', '_', self.display_url)

  @property
  def display_url(self):
    if not self.is_local:
      return self.url
    url_paths = ['/'.join(p.url.strip('/').split('/')[:-1])
                 for p in self.page_set if p.is_file]
    common_prefix = os.path.commonprefix(url_paths)
    return self.url[len(common_prefix):].strip('/')

  @property
  def archive_path(self):
    return self.page_set.WprFilePathForPage(self)

  def __str__(self):
    return self.url

  def WaitToLoad(self, tab, timeout, poll_interval=0.1):
    Page.WaitForPageToLoad(self, tab, timeout, poll_interval)

  # TODO(dtu): Remove this method when no page sets use a click interaction
  # with a wait condition. crbug.com/168431
  @staticmethod
  def WaitForPageToLoad(obj, tab, timeout, poll_interval=0.1):
    """Waits for various wait conditions present in obj."""
    if hasattr(obj, 'wait_seconds'):
      time.sleep(obj.wait_seconds)
    if hasattr(obj, 'wait_for_element_with_text'):
      callback_code = 'function(element) { return element != null; }'
      util.WaitFor(
          lambda: util.FindElementAndPerformAction(
              tab, obj.wait_for_element_with_text, callback_code),
          timeout, poll_interval)
    if hasattr(obj, 'wait_for_element_with_selector'):
      util.WaitFor(lambda: tab.EvaluateJavaScript(
           'document.querySelector(\'' + obj.wait_for_element_with_selector +
           '\') != null'), timeout, poll_interval)
    if hasattr(obj, 'post_navigate_javascript_to_execute'):
      tab.EvaluateJavaScript(obj.post_navigate_javascript_to_execute)
    if hasattr(obj, 'wait_for_javascript_expression'):
      util.WaitFor(
          lambda: tab.EvaluateJavaScript(obj.wait_for_javascript_expression),
          timeout, poll_interval)
