#!/bin/bash -e

# Copyright 2015 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# Scan ca bundle in its common appearing locations.
paths=('/opt/local/etc/openssl/cert.pem'  # macports
       '/opt/local/share/curl/curl-ca-bundle.crt' # macports
       '/usr/local/etc/openssl/cert.pem'  # homebrew
       '/etc/ssl/cert.pem')

for path in "${paths[@]}"; do
  if test -f "$path"; then
    echo "$path"
    exit 0
  fi
done

echo 'Failed to locate SSL CA cert.'
exit 1
