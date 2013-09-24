// Copyright (c) 2013, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

// Generated from namespace: comments

part of chrome;
/**
 * Functions
 */

class API_comments {
  /*
   * API connection
   */
  Object _jsObject;

  /*
   * Functions
   */
  /// There's a blank line at the start of this comment.  Documentation for
  /// basicFunction. BasicFunction() is a great function. There is a newline
  /// after this.<br/><br/> It works like so:        +-----+        |     |
  /// +--+        |     |     |  |        +-----+ --> +--+<br/><br/> Some other
  /// stuff here.    This paragraph starts with whitespace.    Overall, its a
  /// great function. There's also a blank line at the end of this comment.
  void basicFunction() => JS('void', '#.basicFunction()', this._jsObject);

  API_comments(this._jsObject) {
  }
}
