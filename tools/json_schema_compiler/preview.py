#!/usr/bin/env python

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Server for viewing the compiled C++ code from tools/json_schema_compiler.
"""

import cc_generator
import code
import compiler
import cpp_type_generator
import cpp_util
import h_generator
import idl_schema
import json_schema
import model
import optparse
import os
import schema_loader
import sys
import urlparse
from highlighters import (
    pygments_highlighter, none_highlighter, hilite_me_highlighter)
from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer

class CompilerHandler(BaseHTTPRequestHandler):
  """A HTTPRequestHandler that outputs the result of tools/json_schema_compiler.
  """
  def do_GET(self):
    parsed_url = urlparse.urlparse(self.path)
    request_path = self._GetRequestPath(parsed_url)

    chromium_favicon = 'http://codereview.chromium.org/static/favicon.ico'

    head = code.Code()
    head.Append('<link rel="icon" href="%s">' % chromium_favicon)
    head.Append('<link rel="shortcut icon" href="%s">' % chromium_favicon)

    body = code.Code()

    try:
      if os.path.isdir(request_path):
        self._ShowPanels(parsed_url, head, body)
      else:
        self._ShowCompiledFile(parsed_url, head, body)
    finally:
      self.wfile.write('<html><head>')
      self.wfile.write(head.Render())
      self.wfile.write('</head><body>')
      self.wfile.write(body.Render())
      self.wfile.write('</body></html>')

  def _GetRequestPath(self, parsed_url, strip_nav=False):
    """Get the relative path from the current directory to the requested file.
    """
    path = parsed_url.path
    if strip_nav:
      path = parsed_url.path.replace('/nav', '')
    return os.path.normpath(os.curdir + path)

  def _ShowPanels(self, parsed_url, head, body):
    """Show the previewer frame structure.

    Code panes are populated via XHR after links in the nav pane are clicked.
    """
    (head.Append('<style>')
         .Append('body {')
         .Append('  margin: 0;')
         .Append('}')
         .Append('.pane {')
         .Append('  height: 100%;')
         .Append('  overflow-x: auto;')
         .Append('  overflow-y: scroll;')
         .Append('  display: inline-block;')
         .Append('}')
         .Append('#nav_pane {')
         .Append('  width: 20%;')
         .Append('}')
         .Append('#nav_pane ul {')
         .Append('  list-style-type: none;')
         .Append('  padding: 0 0 0 1em;')
         .Append('}')
         .Append('#cc_pane {')
         .Append('  width: 40%;')
         .Append('}')
         .Append('#h_pane {')
         .Append('  width: 40%;')
         .Append('}')
         .Append('</style>')
    )

    body.Append(
        '<div class="pane" id="nav_pane">%s</div>'
        '<div class="pane" id="h_pane"></div>'
        '<div class="pane" id="cc_pane"></div>' %
        self._RenderNavPane(parsed_url.path[1:])
    )

    # The Javascript that interacts with the nav pane and panes to show the
    # compiled files as the URL or highlighting options change.
    body.Append('''<script type="text/javascript">
// Calls a function for each highlighter style <select> element.
function forEachHighlighterStyle(callback) {
  var highlighterStyles =
      document.getElementsByClassName('highlighter_styles');
  for (var i = 0; i < highlighterStyles.length; ++i)
    callback(highlighterStyles[i]);
}

// Called when anything changes, such as the highlighter or hashtag.
function updateEverything() {
  var highlighters = document.getElementById('highlighters');
  var highlighterName = highlighters.value;

  // Cache in localStorage for when the page loads next.
  localStorage.highlightersValue = highlighterName;

  // Show/hide the highlighter styles.
  var highlighterStyleName = '';
  forEachHighlighterStyle(function(highlighterStyle) {
    if (highlighterStyle.id === highlighterName + '_styles') {
      highlighterStyle.removeAttribute('style')
      highlighterStyleName = highlighterStyle.value;
    } else {
      highlighterStyle.setAttribute('style', 'display:none')
    }

    // Cache in localStorage for when the page next loads.
    localStorage[highlighterStyle.id + 'Value'] = highlighterStyle.value;
  });

  // Populate the code panes.
  function populateViaXHR(elementId, requestPath) {
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
      if (xhr.readyState != 4)
        return;
      if (xhr.status != 200) {
        alert('XHR error to ' + requestPath);
        return;
      }
      document.getElementById(elementId).innerHTML = xhr.responseText;
    };
    xhr.open('GET', requestPath, true);
    xhr.send();
  }

  var targetName = window.location.hash;
  targetName = targetName.substring('#'.length);
  targetName = targetName.split('.', 1)[0]

  if (targetName !== '') {
    var basePath = window.location.pathname;
    var query = 'highlighter=' + highlighterName + '&' +
                'style=' + highlighterStyleName;
    populateViaXHR('h_pane',  basePath + '/' + targetName + '.h?'  + query);
    populateViaXHR('cc_pane', basePath + '/' + targetName + '.cc?' + query);
  }
}

// Initial load: set the values of highlighter and highlighterStyles from
// localStorage.
(function() {
var cachedValue = localStorage.highlightersValue;
if (cachedValue)
  document.getElementById('highlighters').value = cachedValue;

forEachHighlighterStyle(function(highlighterStyle) {
  var cachedValue = localStorage[highlighterStyle.id + 'Value'];
  if (cachedValue)
    highlighterStyle.value = cachedValue;
});
})();

window.addEventListener('hashchange', updateEverything, false);
updateEverything();
</script>''')

  def _LoadModel(self, basedir, name):
    """Loads and returns the model for the |name| API from either its JSON or
    IDL file, e.g.
        name=contextMenus will be loaded from |basedir|/context_menus.json,
        name=alarms will be loaded from |basedir|/alarms.idl.
    """
    loaders = {
      'json': json_schema.Load,
      'idl': idl_schema.Load
    }
    # APIs are referred to like "webRequest" but that's in a file
    # "web_request.json" so we need to unixify the name.
    unix_name = model.UnixName(name)
    for loader_ext, loader_fn in loaders.items():
      file_path = '%s/%s.%s' % (basedir, unix_name, loader_ext)
      if os.path.exists(file_path):
        # For historical reasons these files contain a singleton list with the
        # model, so just return that single object.
        return (loader_fn(file_path)[0], file_path)
    raise ValueError('File for model "%s" not found' % name)

  def _ShowCompiledFile(self, parsed_url, head, body):
    """Show the compiled version of a json or idl file given the path to the
    compiled file.
    """
    api_model = model.Model()

    request_path = self._GetRequestPath(parsed_url)
    (file_root, file_ext) = os.path.splitext(request_path)
    (filedir, filename) = os.path.split(file_root)

    try:
      # Get main file.
      (api_def, file_path) = self._LoadModel(filedir, filename)
      namespace = api_model.AddNamespace(api_def, file_path)
      type_generator = cpp_type_generator.CppTypeGenerator(
           api_model,
           schema_loader.SchemaLoader(filedir),
           namespace)

      # Get the model's dependencies.
      for dependency in api_def.get('dependencies', []):
        # Dependencies can contain : in which case they don't refer to APIs,
        # rather, permissions or manifest keys.
        if ':' in dependency:
          continue
        (api_def, file_path) = self._LoadModel(filedir, dependency)
        referenced_namespace = api_model.AddNamespace(api_def, file_path)
        if referenced_namespace:
          type_generator.AddNamespace(referenced_namespace,
              cpp_util.Classname(referenced_namespace.name).lower())

      # Generate code
      cpp_namespace = 'generated_api_schemas'
      if file_ext == '.h':
        cpp_code = (h_generator.HGenerator(type_generator, cpp_namespace)
            .Generate(namespace).Render())
      elif file_ext == '.cc':
        cpp_code = (cc_generator.CCGenerator(type_generator, cpp_namespace)
            .Generate(namespace).Render())
      else:
        self.send_error(404, "File not found: %s" % request_path)
        return

      # Do highlighting on the generated code
      (highlighter_param, style_param) = self._GetHighlighterParams(parsed_url)
      head.Append('<style>' +
          self.server.highlighters[highlighter_param].GetCSS(style_param) +
          '</style>')
      body.Append(self.server.highlighters[highlighter_param]
          .GetCodeElement(cpp_code, style_param))
    except IOError:
      self.send_error(404, "File not found: %s" % request_path)
      return
    except (TypeError, KeyError, AttributeError,
        AssertionError, NotImplementedError) as error:
      body.Append('<pre>')
      body.Append('compiler error: %s' % error)
      body.Append('Check server log for more details')
      body.Append('</pre>')
      raise

  def _GetHighlighterParams(self, parsed_url):
    """Get the highlighting parameters from a parsed url.
    """
    query_dict = urlparse.parse_qs(parsed_url.query)
    return (query_dict.get('highlighter', ['pygments'])[0],
        query_dict.get('style', ['colorful'])[0])

  def _RenderNavPane(self, path):
    """Renders an HTML nav pane.

    This consists of a select element to set highlight style, and a list of all
    files at |path| with the appropriate onclick handlers to open either
    subdirectories or JSON files.
    """
    html = code.Code()

    # Highlighter chooser.
    html.Append('<select id="highlighters" onChange="updateEverything()">')
    for name, highlighter in self.server.highlighters.items():
      html.Append('<option value="%s">%s</option>' %
          (name, highlighter.DisplayName()))
    html.Append('</select>')

    html.Append('<br/>')

    # Style for each highlighter.
    # The correct highlighting will be shown by Javascript.
    for name, highlighter in self.server.highlighters.items():
      styles = sorted(highlighter.GetStyles())
      if not styles:
        continue

      html.Append('<select class="highlighter_styles" id="%s_styles" '
                  'onChange="updateEverything()">' % name)
      for style in styles:
        html.Append('<option>%s</option>' % style)
      html.Append('</select>')

    html.Append('<br/>')

    # The files, with appropriate handlers.
    html.Append('<ul>')

    # Make path point to a non-empty directory. This can happen if a URL like
    # http://localhost:8000 is navigated to.
    if path == '':
      path = os.curdir

    # Firstly, a .. link if this isn't the root.
    if not os.path.samefile(os.curdir, path):
      normpath = os.path.normpath(os.path.join(path, os.pardir))
      html.Append('<li><a href="/%s">%s/</a>' % (normpath, os.pardir))

    # Each file under path/
    for filename in sorted(os.listdir(path)):
      full_path = os.path.join(path, filename)
      (file_root, file_ext) = os.path.splitext(full_path)
      if os.path.isdir(full_path) and not full_path.endswith('.xcodeproj'):
        html.Append('<li><a href="/%s/">%s/</a>' % (full_path, filename))
      elif file_ext in ['.json', '.idl']:
        # cc/h panes will automatically update via the hash change event.
        html.Append('<li><a href="#%s">%s</a>' %
            (filename, filename))

    html.Append('</ul>')

    return html.Render()

class PreviewHTTPServer(HTTPServer, object):
  def __init__(self, server_address, handler, highlighters):
    super(PreviewHTTPServer, self).__init__(server_address, handler)
    self.highlighters = highlighters


if __name__ == '__main__':
  parser = optparse.OptionParser(
      description='Runs a server to preview the json_schema_compiler output.',
      usage='usage: %prog [option]...')
  parser.add_option('-p', '--port', default='8000',
      help='port to run the server on')

  (opts, argv) = parser.parse_args()

  try:
    print('Starting previewserver on port %s' % opts.port)
    print('The extension documentation can be found at:')
    print('')
    print('  http://localhost:%s/chrome/common/extensions/api' % opts.port)
    print('')

    highlighters = {
      'hilite': hilite_me_highlighter.HiliteMeHighlighter(),
      'none': none_highlighter.NoneHighlighter()
    }
    try:
      highlighters['pygments'] = pygments_highlighter.PygmentsHighlighter()
    except ImportError as e:
      pass

    server = PreviewHTTPServer(('', int(opts.port)),
                               CompilerHandler,
                               highlighters)
    server.serve_forever()
  except KeyboardInterrupt:
    server.socket.close()
