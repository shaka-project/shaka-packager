#!/bin/bash -e

# Copyright 2015 Google Inc. All rights reserved.
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://developers.google.com/open-source/licenses/bsd

# Scan ca bundle in its common appearing locations.
paths=('/etc/pki/tls/certs/ca-bundle.crt'
       '/etc/ssl/ca-bundle.pem'
       '/etc/ssl/cert.pem'
       '/etc/ssl/certs/ca-bundle.crt'
       '/etc/ssl/certs/ca-certificates.crt'
       '/usr/local/share/certs/ca-root.crt'
       '/usr/share/ssl/certs/ca-bundle.crt')

for path in "${paths[@]}"; do
  if test -f "$path"; then
    echo "$path"
    exit 0
  fi
done

echo 'Failed to locate SSL CA cert.'
exit 1
