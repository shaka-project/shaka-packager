# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import sys

sys.exit(subprocess.call(["pkg-config"] + sys.argv[1:]))
