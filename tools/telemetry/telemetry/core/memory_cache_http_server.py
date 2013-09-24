# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import BaseHTTPServer
from collections import namedtuple
import gzip
import mimetypes
import os
import SimpleHTTPServer
import SocketServer
import StringIO
import sys


ByteRange = namedtuple('ByteRange', ['from_byte', 'to_byte'])
ResourceAndRange = namedtuple('ResourceAndRange', ['resource', 'byte_range'])


class MemoryCacheHTTPRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):

  def do_GET(self):
    """Serve a GET request."""
    resource_range = self.SendHead()

    if not resource_range or not resource_range.resource:
      return
    response = resource_range.resource['response']

    if not resource_range.byte_range:
      self.wfile.write(response)
      return

    start_index = resource_range.byte_range.from_byte
    end_index = resource_range.byte_range.to_byte
    self.wfile.write(response[start_index:end_index + 1])

  def do_HEAD(self):
    """Serve a HEAD request."""
    self.SendHead()

  def SendHead(self):
    path = self.translate_path(self.path)
    if path not in self.server.resource_map:
      self.send_error(404, 'File not found')
      return None

    resource = self.server.resource_map[path]
    total_num_of_bytes = resource['content-length']
    byte_range = self.GetByteRange(total_num_of_bytes)
    if byte_range:
      # request specified a range, so set response code to 206.
      self.send_response(206)
      self.send_header('Content-Range',
                       'bytes %d-%d/%d' % (byte_range.from_byte,
                                           byte_range.to_byte,
                                           total_num_of_bytes))
      total_num_of_bytes = byte_range.to_byte - byte_range.from_byte + 1
    else:
      self.send_response(200)

    self.send_header('Content-Length', str(total_num_of_bytes))
    self.send_header('Content-Type', resource['content-type'])
    self.send_header('Last-Modified',
                     self.date_time_string(resource['last-modified']))
    if resource['zipped']:
      self.send_header('Content-Encoding', 'gzip')
    self.end_headers()
    return ResourceAndRange(resource, byte_range)

  def GetByteRange(self, total_num_of_bytes):
    """Parse the header and get the range values specified.

    Args:
      total_num_of_bytes: Total # of bytes in requested resource,
      used to calculate upper range limit.
    Returns:
      A ByteRange namedtuple object with the requested byte-range values.
      If no Range is explicitly requested or there is a failure parsing,
      return None.
      Special case: If range specified is in the format "N-", return N-N.
      If upper range limit is greater than total # of bytes, return upper index.
    """

    range_header = self.headers.getheader('Range')
    if range_header is None:
      return None
    if not range_header.startswith('bytes='):
      return None

    # The range header is expected to be a string in this format:
    # bytes=0-1
    # Get the upper and lower limits of the specified byte-range.
    # We've already confirmed that range_header starts with 'bytes='.
    byte_range_values = range_header[len('bytes='):].split('-')
    from_byte = 0
    to_byte = 0

    if len(byte_range_values) == 2:
      from_byte = int(byte_range_values[0])
      if byte_range_values[1]:
        to_byte = int(byte_range_values[1])
    else:
      return None

    # Do some validation.
    if from_byte < 0:
      return None

    if to_byte < from_byte:
      to_byte = from_byte

    if to_byte >= total_num_of_bytes:
      # End of range requested is greater than length of requested resource.
      # Only return # of available bytes.
      to_byte = total_num_of_bytes - 1

    return ByteRange(from_byte, to_byte)


class MemoryCacheHTTPServer(SocketServer.ThreadingMixIn,
                            BaseHTTPServer.HTTPServer):
  # Increase the request queue size. The default value, 5, is set in
  # SocketServer.TCPServer (the parent of BaseHTTPServer.HTTPServer).
  # Since we're intercepting many domains through this single server,
  # it is quite possible to get more than 5 concurrent requests.
  request_queue_size = 128

  def __init__(self, host_port, handler, paths):
    BaseHTTPServer.HTTPServer.__init__(self, host_port, handler)
    self.resource_map = {}
    for path in paths:
      if os.path.isdir(path):
        self.AddDirectoryToResourceMap(path)
      else:
        self.AddFileToResourceMap(path)

  def AddDirectoryToResourceMap(self, directory_path):
    """Loads all files in directory_path into the in-memory resource map."""
    for root, dirs, files in os.walk(directory_path):
      # Skip hidden files and folders (like .svn and .git).
      files = [f for f in files if f[0] != '.']
      dirs[:] = [d for d in dirs if d[0] != '.']

      for f in files:
        file_path = os.path.join(root, f)
        if not os.path.exists(file_path):  # Allow for '.#' files
          continue
        self.AddFileToResourceMap(file_path)

  def AddFileToResourceMap(self, file_path):
    """Loads file_path into the in-memory resource map."""
    with open(file_path, 'rb') as fd:
      response = fd.read()
      fs = os.fstat(fd.fileno())
      content_type = mimetypes.guess_type(file_path)[0]
      zipped = False
      if content_type in ['text/html', 'text/css', 'application/javascript']:
        zipped = True
        sio = StringIO.StringIO()
        gzf = gzip.GzipFile(fileobj=sio, compresslevel=9, mode='wb')
        gzf.write(response)
        gzf.close()
        response = sio.getvalue()
        sio.close()
      self.resource_map[file_path] = {
          'content-type': content_type,
          'content-length': len(response),
          'last-modified': fs.st_mtime,
          'response': response,
          'zipped': zipped
          }

      index = os.path.sep + 'index.html'
      if file_path.endswith(index):
        self.resource_map[
            file_path[:-len(index)]] = self.resource_map[file_path]


def Main():
  assert len(sys.argv) > 2, 'usage: %prog <port> [<path1>, <path2>, ...]'

  port = int(sys.argv[1])
  paths = sys.argv[2:]
  server_address = ('127.0.0.1', port)
  MemoryCacheHTTPRequestHandler.protocol_version = 'HTTP/1.1'
  httpd = MemoryCacheHTTPServer(server_address, MemoryCacheHTTPRequestHandler,
                                paths)
  httpd.serve_forever()


if __name__ == '__main__':
  Main()
