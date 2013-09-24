# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import re
import sys


def _SyncFilesToCloud(input_api, output_api):
  """Searches for .sha1 files and uploads them to Cloud Storage.

  It validates all the hashes and skips upload if not necessary.
  """
  # Because this script will be called from a magic PRESUBMIT demon,
  # avoid angering it; don't pollute its sys.path.
  old_sys_path = sys.path
  try:
    sys.path = [os.path.join(os.pardir, os.pardir, 'telemetry')] + sys.path
    from telemetry.page import cloud_storage
  finally:
    sys.path = old_sys_path

  hashes_in_cloud_storage = cloud_storage.List(cloud_storage.DEFAULT_BUCKET)

  results = []
  for hash_path in input_api.AbsoluteLocalPaths():
    file_path, extension = os.path.splitext(hash_path)
    if extension != '.sha1':
      continue

    with open(hash_path, 'rb') as f:
      file_hash = f.read(1024).rstrip()
    if file_hash in hashes_in_cloud_storage:
      results.append(output_api.PresubmitNotifyResult(
          'File already in Cloud Storage, skipping upload: %s' % hash_path))
      continue

    if not re.match('^([A-Za-z0-9]{40})$', file_hash):
      results.append(output_api.PresubmitError(
          'Hash file does not contain a valid SHA-1 hash: %s' % hash_path))
      continue
    if not os.path.exists(file_path):
      results.append(output_api.PresubmitError(
          'Hash file exists, but file not found: %s' % hash_path))
      continue
    if cloud_storage.GetHash(file_path) != file_hash:
      results.append(output_api.PresubmitError(
          'Hash file does not match file\'s actual hash: %s' % hash_path))
      continue

    try:
      cloud_storage.Insert(cloud_storage.DEFAULT_BUCKET, file_hash, file_path)
    except cloud_storage.CloudStorageError:
      results.append(output_api.PresubmitError(
          'Unable to upload to Cloud Storage: %s' % hash_path))

  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results += _SyncFilesToCloud(input_api, output_api)
  return results
