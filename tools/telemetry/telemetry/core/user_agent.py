# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

UA_TYPE_MAPPING = {
  'desktop':
      'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_7_3) '
      'AppleWebKit/537.22 (KHTML, like Gecko) '
      'Chrome/27.0.1453.111 Safari/537.22',
  'mobile':
      'Mozilla/5.0 (Linux; Android 4.0.4; Galaxy Nexus Build/IMM76B) '
      'AppleWebKit/535.19 (KHTML, like Gecko) Chrome/27.0.1453.111 Mobile '
      'Safari/535.19',
  'tablet':
      'Mozilla/5.0 (Linux; Android 4.0.4; Galaxy Nexus 7 Build/IMM76B) '
      'AppleWebKit/535.19 (KHTML, like Gecko) Chrome/27.0.1453.111 '
      'Safari/535.19',
}


def GetChromeUserAgentArgumentFromType(user_agent_type):
  """Returns a chrome user agent based on a user agent type.
  This is derived from:
  https://developers.google.com/chrome/mobile/docs/user-agent
  """
  if user_agent_type:
    return ['--user-agent=%s' % UA_TYPE_MAPPING[user_agent_type]]
  return []
