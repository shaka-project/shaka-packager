# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""HTTP proxy request handler with SSL support.

  RequestHandler: Utility class for parsing HTTP requests.
  ProxyHandler: HTTP proxy handler.
"""

import BaseHTTPServer
import cgi
import OpenSSL
import os
import socket
import SocketServer
import sys
import traceback
import urlparse


class RequestHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  """Class for reading HTTP requests and writing HTTP responses"""

  protocol_version = "HTTP/1.1"
  request_version = protocol_version

  class HTTPRequestException(Exception): pass

  def __init__(self, rfile, wfile, server):
    self.rfile = rfile
    self.wfile = wfile
    self.server = server

  def ReadRequest(self):
    "Reads and parses single HTTP request from self.rfile"

    self.raw_requestline = self.rfile.readline()
    if not self.raw_requestline:
      self.close_connection = 1
      raise HTTPRequestException('failed to read request line')
    if not self.parse_request():
      raise HTTPRequestException('failed to parse request')
    self.headers = dict(self.headers)
    self.body = None
    if 'content-length' in self.headers:
      self.body = self.rfile.read(int(self.headers['content-length']))

  def log_message(self, format, *args):
    pass


class ProxyHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  "Request handler class for proxy server"

  server_version = "PlaybackProxy/0.0.1"
  protocol_version = "HTTP/1.1"

  def do_CONNECT(self):
    "Handles CONNECT HTTP request"

    server = self.path.split(':')[0]
    certificate_file = os.path.join(self.certificate_directory, server)
    if not os.path.isfile(certificate_file):
      sys.stderr.write('request to connect %s is ignored\n' % server)
      self.send_response(501)
      self.send_header('Proxy-agent', self.version_string())
      self.end_headers()
      return

    # Send confirmation to browser.
    self.send_response(200, 'Connection established')
    self.send_header('Proxy-agent', self.version_string())
    self.end_headers()

    # Create SSL context.
    context = OpenSSL.SSL.Context(OpenSSL.SSL.SSLv23_METHOD)
    context.use_privatekey_file(certificate_file)
    context.use_certificate_file(certificate_file)

    # Create and initialize SSL connection atop of tcp socket.
    ssl_connection = OpenSSL.SSL.Connection(context, self.connection)
    ssl_connection.set_accept_state()
    ssl_connection.do_handshake()
    ssl_rfile = socket._fileobject(ssl_connection, "rb", self.rbufsize)
    ssl_wfile = socket._fileobject(ssl_connection, "wb", self.wbufsize)

    # Handle http requests coming from ssl_connection.
    handler = RequestHandler(ssl_rfile, ssl_wfile, self.path)
    try:
      handler.close_connection = 1
      while True:
        handler.ReadRequest()
        self.driver.ProcessRequest(handler)
        if handler.close_connection: break
    except (OpenSSL.SSL.SysCallError, OpenSSL.SSL.ZeroReturnError):
      pass
    finally:
      self.close_connection = 1

  def do_GET(self):
    self.driver.ProcessRequest(self)

  def do_POST(self):
    if 'content-length' in self.headers:
      self.body = self.rfile.read(int(self.headers['content-length']))
    self.driver.ProcessRequest(self)

  def log_message(self, format, *args):
    sys.stdout.write((format % args) + '\n')


class ThreadingHTTPServer (SocketServer.ThreadingMixIn,
                           BaseHTTPServer.HTTPServer):
  pass


def CreateServer(driver, port, certificate_directory=None):
  if not certificate_directory:
    certificate_directory = os.path.join(os.getcwd(), 'certificates')
  ProxyHandler.driver = driver
  ProxyHandler.certificate_directory = certificate_directory
  return ThreadingHTTPServer(('', port), ProxyHandler)
