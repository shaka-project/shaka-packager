# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrappers for gsutil, for basic interaction with Google Cloud Storage."""

import cStringIO
import hashlib
import logging
import os
import subprocess
import sys
import tarfile
import urllib2

from telemetry.core import util


DEFAULT_BUCKET = 'chromium-wpr'


_GSUTIL_URL = 'http://storage.googleapis.com/pub/gsutil.tar.gz'
_DOWNLOAD_PATH = os.path.join(util.GetTelemetryDir(), 'third_party', 'gsutil')


class CloudStorageError(Exception):
  pass


class CredentialsError(CloudStorageError):
  def __init__(self, gsutil_path):
    super(CredentialsError, self).__init__(
        'Attempted to download a file from Cloud Storage but you have no '
        'configured credentials. Run "%s config" to configure your '
        'credentials. The project-id field can be left blank.' % gsutil_path)


def _DownloadGsutil():
  logging.info('Downloading gsutil')
  response = urllib2.urlopen(_GSUTIL_URL)
  with tarfile.open(fileobj=cStringIO.StringIO(response.read())) as tar_file:
    tar_file.extractall(os.path.dirname(_DOWNLOAD_PATH))
  logging.info('Downloaded gsutil to %s' % _DOWNLOAD_PATH)

  return os.path.join(_DOWNLOAD_PATH, 'gsutil')


def _FindGsutil():
  """Return the gsutil executable path. If we can't find it, download it."""
  search_paths = [_DOWNLOAD_PATH] + os.environ['PATH'].split(os.pathsep)

  # Look for a depot_tools installation.
  for path in search_paths:
    gsutil_path = os.path.join(path, 'third_party', 'gsutil', 'gsutil')
    if os.path.isfile(gsutil_path):
      return gsutil_path

  # Look for a gsutil installation.
  for path in search_paths:
    gsutil_path = os.path.join(path, 'gsutil')
    if os.path.isfile(gsutil_path):
      return gsutil_path

  # Failed to find it. Download it!
  return _DownloadGsutil()


def _RunCommand(args):
  gsutil_path = _FindGsutil()
  gsutil = subprocess.Popen([sys.executable, gsutil_path] + args,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  stdout, stderr = gsutil.communicate()

  if gsutil.returncode:
    if ('You are attempting to access protected data with '
        'no configured credentials.' in stderr):
      raise CredentialsError(gsutil_path)
    raise CloudStorageError(stderr)

  return stdout


def List(bucket):
  stdout = _RunCommand(['ls', 'gs://%s' % bucket])
  return [url.split('/')[-1] for url in stdout.splitlines()]


def Delete(bucket, remote_path):
  url = 'gs://%s/%s' % (bucket, remote_path)
  logging.info('Deleting %s' % url)
  _RunCommand(['rm', url])


def Get(bucket, remote_path, local_path):
  url = 'gs://%s/%s' % (bucket, remote_path)
  logging.info('Downloading %s to %s' % (url, local_path))
  _RunCommand(['cp', url, local_path])


def Insert(bucket, remote_path, local_path):
  url = 'gs://%s/%s' % (bucket, remote_path)
  logging.info('Uploading %s to %s' % (local_path, url))
  _RunCommand(['cp', local_path, url])


def GetIfChanged(bucket, file_path):
  """Gets the file at file_path if it has a hash file that doesn't match."""
  hash_path = file_path + '.sha1'
  if not os.path.exists(hash_path):
    return

  with open(hash_path, 'rb') as f:
    expected_hash = f.read(1024).rstrip()
  if not os.path.exists(file_path) or GetHash(file_path) != expected_hash:
    Get(bucket, expected_hash, file_path)


def GetHash(file_path):
  """Calculates and returns the hash of the file at file_path."""
  sha1 = hashlib.sha1()
  with open(file_path, 'rb') as f:
    while True:
      # Read in 1mb chunks, so it doesn't all have to be loaded into memory.
      chunk = f.read(1024*1024)
      if not chunk:
        break
      sha1.update(chunk)
  return sha1.hexdigest()
