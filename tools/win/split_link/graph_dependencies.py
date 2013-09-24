# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import subprocess
import sys


BASE_DIR = os.path.dirname(os.path.abspath(__file__))


def main():
  if len(sys.argv) != 2:
    print 'usage: %s <output.html>' % sys.argv[0]
    return 1
  env = os.environ.copy()
  env['GYP_GENERATORS'] = 'dump_dependency_json'
  print 'Dumping dependencies...'
  popen = subprocess.Popen(
      ['python', 'build/gyp_chromium'],
      shell=True, env=env)
  popen.communicate()
  if popen.returncode != 0:
    return popen.returncode
  print 'Finding problems...'
  popen = subprocess.Popen(
      ['python', 'tools/gyp-explain.py', '--dot',
       'chrome.gyp:browser#', 'core.gyp:webcore#'],
      stdout=subprocess.PIPE,
      shell=True)
  out, _ = popen.communicate()
  if popen.returncode != 0:
    return popen.returncode

  # Break into pairs to uniq to make graph less of a mess.
  print 'Simplifying...'
  deduplicated = set()
  lines = out.splitlines()[2:-1]
  for line in lines:
    line = line.strip('\r\n ;')
    pairs = line.split(' -> ')
    for i in range(len(pairs) - 1):
      deduplicated.add('%s -> %s;' % (pairs[i], pairs[i + 1]))
  graph = 'strict digraph {\n' + '\n'.join(sorted(deduplicated)) + '\n}'

  print 'Writing report to %s...' % sys.argv[1]
  path_count = len(out.splitlines())
  with open(os.path.join(BASE_DIR, 'viz.js', 'viz.js')) as f:
    viz_js = f.read()
  with open(sys.argv[1], 'w') as f:
    f.write(PREFIX % path_count)
    f.write(graph)
    f.write(SUFFIX % viz_js)
  print 'Done.'


PREFIX = r'''<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <title>Undesirable Dependencies</title>
  </head>
  <body>
    <h1>Undesirable Dependencies</h1>
<h2>browser &rarr; webcore</h2>
<h3>%d paths</h3>
    <script type="text/vnd.graphviz" id="graph">
'''


SUFFIX = r'''
    </script>
    <script>%s</script>
    <div id="output">Rendering...</div>
    <script>
      setTimeout(function() {
          document.getElementById("output").innerHTML =
              Viz(document.getElementById("graph").innerHTML, "svg");
        }, 1);
    </script>
  </body>
</html>
'''


if __name__ == '__main__':
  sys.exit(main())
