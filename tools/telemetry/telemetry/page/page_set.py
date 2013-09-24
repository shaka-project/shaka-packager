# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import csv
import json
import os

from telemetry.page import cloud_storage
from telemetry.page import page as page_module
from telemetry.page import page_set_archive_info

class PageSet(object):
  def __init__(self, file_path='', attributes=None):
    self.description = ''
    self.archive_data_file = ''
    self.file_path = file_path
    self.credentials_path = None
    self.user_agent_type = None
    self.make_javascript_deterministic = True

    if attributes:
      for k, v in attributes.iteritems():
        setattr(self, k, v)

    self.pages = []

    if self.archive_data_file:
      self.wpr_archive_info = page_set_archive_info.PageSetArchiveInfo.FromFile(
          os.path.join(self._base_dir, self.archive_data_file), file_path)
    else:
      self.wpr_archive_info = None

  @classmethod
  def FromFile(cls, file_path):
    with open(file_path, 'r') as f:
      contents = f.read()
    data = json.loads(contents)
    return cls.FromDict(data, file_path)

  @classmethod
  def FromDict(cls, data, file_path):
    page_set = cls(file_path, data)

    for page_attributes in data['pages']:
      url = page_attributes.pop('url')

      page = page_module.Page(url, page_set, attributes=page_attributes,
          base_dir=page_set._base_dir)  # pylint: disable=W0212
      page_set.pages.append(page)

    all_serving_dirs = set()
    for page in page_set:
      if page.is_file:
        serving_dirs, _ = page.serving_dirs_and_file
        if isinstance(serving_dirs, list):
          all_serving_dirs |= set(serving_dirs)
        else:
          all_serving_dirs.add(serving_dirs)
    for serving_dir in all_serving_dirs:
      for dirpath, _, filenames in os.walk(serving_dir):
        for filename in filenames:
          file_path, extension = os.path.splitext(
              os.path.join(dirpath, filename))
          if extension != '.sha1':
            continue
          cloud_storage.GetIfChanged(cloud_storage.DEFAULT_BUCKET, file_path)

    return page_set

  @property
  def _base_dir(self):
    if os.path.isdir(self.file_path):
      return self.file_path
    else:
      return os.path.dirname(self.file_path)

  def ContainsOnlyFileURLs(self):
    for page in self.pages:
      if not page.is_file:
        return False
    return True

  def ReorderPageSet(self, results_file):
    """Reorders this page set based on the results of a past run."""
    page_set_dict = {}
    for page in self.pages:
      page_set_dict[page.url] = page

    pages = []
    with open(results_file, 'rb') as csv_file:
      csv_reader = csv.reader(csv_file)
      csv_header = csv_reader.next()

      if 'url' not in csv_header:
        raise Exception('Unusable results_file.')

      url_index = csv_header.index('url')

      for csv_row in csv_reader:
        if csv_row[url_index] in page_set_dict:
          pages.append(page_set_dict[csv_row[url_index]])
        else:
          raise Exception('Unusable results_file.')

    return pages

  def WprFilePathForPage(self, page):
    if not self.wpr_archive_info:
      return None
    return self.wpr_archive_info.WprFilePathForPage(page)

  def __iter__(self):
    return self.pages.__iter__()

  def __len__(self):
    return len(self.pages)

  def __getitem__(self, key):
    return self.pages[key]

  def __setitem__(self, key, value):
    self.pages[key] = value
