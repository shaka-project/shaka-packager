#!/usr/bin/python3
"""Lints all Python sources in the repo, excluding third-party code."""

import os
import subprocess

import pylint.lint

def ShouldLintFile(path):
  """Returns True if this path should be linted."""
  excluded_folders = [
      'third_party',
      'protoc_wrapper',
      'ycm_extra_conf',
  ]

  if (not path.endswith('.py') or
      any(f in path for f in excluded_folders)):
    return False

  return True

def GetPyFileList():
  """Yield the paths of Python source files that should be linted."""
  output = subprocess.check_output(['git', 'ls-files'], text=True)

  for path in output.split('\n'):
    if ShouldLintFile(path):
      yield path

def main():
  """Lint Python source files.

  Pylint will exit with a non-zero status if there are any failures."""
  dir_name = os.path.dirname(__file__)
  rc_path = os.path.join(dir_name, 'pylintrc')

  py_files = list(GetPyFileList())
  # Run will call sys.exit, so no explicit call to sys.exit is used here.
  pylint.lint.Run(['--rcfile={}'.format(rc_path)] + py_files)

if __name__ == '__main__':
  main()
