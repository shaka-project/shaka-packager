// Copyright (c) 2013, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

// Generated from namespace: tags

part of chrome;

/**
 * Types
 */

class TagsInlineDoc extends ChromeObject {
  /*
   * Public constructor
   */
  TagsInlineDoc({}) {
  }

  /*
   * Private constructor
   */
  TagsInlineDoc._proxy(_jsObject) : super._proxy(_jsObject);
}

class TagsNodoc extends ChromeObject {
  /*
   * Public constructor
   */
  TagsNodoc({}) {
  }

  /*
   * Private constructor
   */
  TagsNodoc._proxy(_jsObject) : super._proxy(_jsObject);
}

class TagsNocompile extends ChromeObject {
  /*
   * Public constructor
   */
  TagsNocompile({}) {
  }

  /*
   * Private constructor
   */
  TagsNocompile._proxy(_jsObject) : super._proxy(_jsObject);
}

class TagsPlainDict extends ChromeObject {
  /*
   * Public constructor
   */
  TagsPlainDict({int inline_doc, String nodoc, double nocompile, fileEntry instance_of_tag}) {
    if (inline_doc != null)
      this.inline_doc = inline_doc;
    if (nodoc != null)
      this.nodoc = nodoc;
    if (nocompile != null)
      this.nocompile = nocompile;
    if (instance_of_tag != null)
      this.instance_of_tag = instance_of_tag;
  }

  /*
   * Private constructor
   */
  TagsPlainDict._proxy(_jsObject) : super._proxy(_jsObject);

  /*
   * Public accessors
   */
  /// This int has the property [inline_doc].
  int get inline_doc => JS('int', '#.inline_doc', this._jsObject);

  void set inline_doc(int inline_doc) {
    JS('void', '#.inline_doc = #', this._jsObject, inline_doc);
  }

  /// This String has the property [nodoc].
  String get nodoc => JS('String', '#.nodoc', this._jsObject);

  void set nodoc(String nodoc) {
    JS('void', '#.nodoc = #', this._jsObject, nodoc);
  }

  /// This double has the property [nocompile].
  double get nocompile => JS('double', '#.nocompile', this._jsObject);

  void set nocompile(double nocompile) {
    JS('void', '#.nocompile = #', this._jsObject, nocompile);
  }

  /// This object has the property [instanceOf=fileEntry].
  fileEntry get instance_of_tag => JS('fileEntry', '#.instance_of_tag', this._jsObject);

  void set instance_of_tag(fileEntry instance_of_tag) {
    JS('void', '#.instance_of_tag = #', this._jsObject, convertArgument(instance_of_tag));
  }

}

/**
 * Functions
 */

class API_tags {
  /*
   * API connection
   */
  Object _jsObject;
  API_tags(this._jsObject) {
  }
}
