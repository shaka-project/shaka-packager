# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import logging
import os
import re
import shutil

from telemetry.page import cloud_storage


def _UpdateHashFile(file_path):
  with open(file_path + '.sha1', 'wb') as f:
    f.write(cloud_storage.GetHash(file_path))
    f.flush()


class PageSetArchiveInfo(object):
  def __init__(self, archive_data_file_path, page_set_file_path, data):
    self._archive_data_file_path = archive_data_file_path
    self._archive_data_file_dir = os.path.dirname(archive_data_file_path)
    # Back pointer to the page set file.
    self._page_set_file_path = page_set_file_path

    for archive_path in data['archives']:
      cloud_storage.GetIfChanged(cloud_storage.DEFAULT_BUCKET, archive_path)

    # Map from the relative path (as it appears in the metadata file) of the
    # .wpr file to a list of urls it supports.
    self._wpr_file_to_urls = data['archives']

    # Map from the page url to a relative path (as it appears in the metadata
    # file) of the .wpr file.
    self._url_to_wpr_file = dict()
    # Find out the wpr file names for each page.
    for wpr_file in data['archives']:
      page_urls = data['archives'][wpr_file]
      for url in page_urls:
        self._url_to_wpr_file[url] = wpr_file
    self.temp_target_wpr_file_path = None

  @classmethod
  def FromFile(cls, file_path, page_set_file_path):
    cloud_storage.GetIfChanged(cloud_storage.DEFAULT_BUCKET, file_path)

    if os.path.exists(file_path):
      with open(file_path, 'r') as f:
        data = json.load(f)
        return cls(file_path, page_set_file_path, data)
    return cls(file_path, page_set_file_path, {'archives': {}})

  def WprFilePathForPage(self, page):
    if self.temp_target_wpr_file_path:
      return self.temp_target_wpr_file_path
    wpr_file = self._url_to_wpr_file.get(page.url, None)
    if wpr_file:
      return self._WprFileNameToPath(wpr_file)
    return None

  def AddNewTemporaryRecording(self, temp_target_wpr_file_path):
    self.temp_target_wpr_file_path = temp_target_wpr_file_path

  def AddRecordedPages(self, urls):
    (target_wpr_file, target_wpr_file_path) = self._NextWprFileName()
    for url in urls:
      self._SetWprFileForPage(url, target_wpr_file)
    shutil.move(self.temp_target_wpr_file_path, target_wpr_file_path)
    _UpdateHashFile(target_wpr_file_path)
    self._WriteToFile()
    self._DeleteAbandonedWprFiles()

  def _DeleteAbandonedWprFiles(self):
    # Update the metadata so that the abandoned wpr files don't have empty url
    # arrays.
    abandoned_wpr_files = self._AbandonedWprFiles()
    for wpr_file in abandoned_wpr_files:
      del self._wpr_file_to_urls[wpr_file]
      # Don't fail if we're unable to delete some of the files.
      wpr_file_path = self._WprFileNameToPath(wpr_file)
      try:
        os.remove(wpr_file_path)
      except Exception:
        logging.warning('Failed to delete file: %s' % wpr_file_path)

  def _AbandonedWprFiles(self):
    abandoned_wpr_files = []
    for wpr_file, urls in self._wpr_file_to_urls.iteritems():
      if not urls:
        abandoned_wpr_files.append(wpr_file)
    return abandoned_wpr_files

  def _WriteToFile(self):
    """Writes the metadata into the file passed as constructor parameter."""
    metadata = dict()
    metadata['description'] = (
        'Describes the Web Page Replay archives for a page set. Don\'t edit by '
        'hand! Use record_wpr for updating.')
    # Pointer from the metadata to the page set .json file.
    metadata['page_set'] = os.path.relpath(self._page_set_file_path,
                                           self._archive_data_file_dir)
    metadata['archives'] = self._wpr_file_to_urls.copy()
    # Don't write data for abandoned archives.
    abandoned_wpr_files = self._AbandonedWprFiles()
    for wpr_file in abandoned_wpr_files:
      del metadata['archives'][wpr_file]

    with open(self._archive_data_file_path, 'w') as f:
      json.dump(metadata, f, indent=4)
      f.flush()
    _UpdateHashFile(self._archive_data_file_path)

  def _WprFileNameToPath(self, wpr_file):
    return os.path.abspath(os.path.join(self._archive_data_file_dir, wpr_file))

  def _NextWprFileName(self):
    """Creates a new file name for a wpr archive file."""
    # The names are of the format "some_thing_number.wpr". Read the numbers.
    highest_number = -1
    base = None
    for wpr_file in self._wpr_file_to_urls:
      match = re.match(r'(?P<BASE>.*)_(?P<NUMBER>[0-9]+)\.wpr', wpr_file)
      if not match:
        raise Exception('Illegal wpr file name ' + wpr_file)
      highest_number = max(int(match.groupdict()['NUMBER']), highest_number)
      if base and match.groupdict()['BASE'] != base:
        raise Exception('Illegal wpr file name ' + wpr_file +
                        ', doesn\'t begin with ' + base)
      base = match.groupdict()['BASE']
    if not base:
      # If we're creating a completely new info file, use the base name of the
      # page set file.
      base = os.path.splitext(os.path.basename(self._page_set_file_path))[0]
    new_filename = '%s_%03d.wpr' % (base, highest_number + 1)
    return new_filename, self._WprFileNameToPath(new_filename)

  def _SetWprFileForPage(self, url, wpr_file):
    """For modifying the metadata when we're going to record a new archive."""
    old_wpr_file = self._url_to_wpr_file.get(url, None)
    if old_wpr_file:
      self._wpr_file_to_urls[old_wpr_file].remove(url)
    self._url_to_wpr_file[url] = wpr_file
    if wpr_file not in self._wpr_file_to_urls:
      self._wpr_file_to_urls[wpr_file] = []
    self._wpr_file_to_urls[wpr_file].append(url)
