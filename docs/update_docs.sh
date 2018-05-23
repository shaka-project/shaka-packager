#!/bin/bash
#
# Copyright 2018 Google LLC. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd
#
# This script is expected to run in src/ directory.
#   ./docs/update_docs.sh {SHOULD_RUN_DOXYGEN:default true} \
#                         {SITE_DIRECTORY: default unset}

set -ev

SHOULD_RUN_DOXYGEN=${1:-true}
SITE_DIRECTORY=${2}

rev=$(git rev-parse HEAD)

if [ "${SHOULD_RUN_DOXYGEN}" = true ] ; then
  doxygen docs/Doxyfile
fi
cd docs
make html

cd ../out

# If SITE_DIRECTORY is specified, it is assumed to be local evaluation, so we
# will not try to update Git repository.
if [[ -z ${SITE_DIRECTORY} ]] ; then
  rm -rf gh-pages
  git clone --depth 1 https://github.com/google/shaka-packager -b gh-pages gh-pages
  cd gh-pages
  git rm -rf *
  mv ../sphinx/html html
  if [ "${SHOULD_RUN_DOXYGEN}" = true ] ; then
    mv ../doxygen/html docs
  fi
  git add *
  git commit -m "Generate documents for commit ${rev}"
else
  rm -rf ${SITE_DIRECTORY}
  mkdir ${SITE_DIRECTORY}
  mv sphinx/html ${SITE_DIRECTORY}/html
  if [ "${SHOULD_RUN_DOXYGEN}" = true ] ; then
    mv doxygen/html ${SITE_DIRECTORY}/docs
  fi
  chmod -R 755 ${SITE_DIRECTORY}
fi
