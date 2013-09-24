# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import string
import subprocess
import sys


BASE_DIR = os.path.dirname(os.path.abspath(__file__))


def Run(*args):
  with open(os.devnull, 'w') as null:
    subprocess.check_call(args, stdout=null, stderr=null)


def FindNode(node, component):
  for child in node['children']:
    if child['name'] == component:
      return child
  return None


def InsertIntoTree(tree, source_name, size):
  components = source_name.replace(':', '').split('\\')
  node = tree
  for index, component in enumerate(components):
    data = FindNode(node, component)
    if not data:
      data = { 'name': component }
      if index == len(components) - 1:
        data['size'] = size
      else:
        data['children'] = []
      node['children'].append(data)
    node = data


def main():
  out_dir = os.path.join(BASE_DIR, '..', '..', '..', 'out', 'Release')
  jsons = []
  for dll in ('chrome.dll', 'chrome_child.dll'):
    dll_path = os.path.normpath(os.path.join(out_dir, dll))
    if os.path.exists(dll_path):
      print 'Tallying %s...' % dll_path
      json_path = dll_path + '.json'
      Run(os.path.join(BASE_DIR, 'code_tally.exe'),
          '--input-image=' + dll_path,
          '--input-pdb=' + dll_path + '.pdb',
          '--output-file=' + json_path)
      jsons.append(json_path)
  if not jsons:
    print 'Couldn\'t find binaries, looking in', out_dir
    return 1

  for json_name in jsons:
    with open(json_name, 'r') as jsonf:
      all_data = json.load(jsonf)
    html_path = os.path.splitext(json_name)[0] + '.html'
    print 'Generating %s...' % html_path
    by_source = {}
    for obj_name, obj_data in all_data['objects'].iteritems():
      for symbol, symbol_data in obj_data.iteritems():
        size = int(symbol_data['size'])
        # Sometimes there's symbols with no source file, we just ignore those.
        if 'contribs' in symbol_data:
          # There may be more than one file in the list, we just assign to the
          # first source file that contains the symbol, rather than try to
          # split or duplicate info.
          src_index = symbol_data['contribs'][0]
          source = all_data['sources'][int(src_index)]
          if source not in by_source:
            by_source[source] = []
          by_source[source].append(size)
    binary_name = all_data['executable']['name']
    data = {}
    data['name'] = binary_name
    data['children'] = []
    for source, sizes in by_source.iteritems():
      InsertIntoTree(data, source, sum(sizes))
    with open(html_path, 'w') as f:
      with open(os.path.join(BASE_DIR, 'template.html'), 'r') as templatef:
        template = templatef.read()
      f.write(string.Template(template).substitute(
          {'data': json.dumps(data, indent=2),
           'dllname': binary_name + ' ' + all_data['executable']['version']}))

  return 0


if __name__ == '__main__':
  sys.exit(main())
