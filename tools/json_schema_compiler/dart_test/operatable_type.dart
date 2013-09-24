// Copyright (c) 2013, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

// Generated from namespace: operatable_type

part of chrome;

/**
 * Types
 */

class Operatable_typeDictType extends ChromeObject {
  /*
   * Public constructor
   */
  Operatable_typeDictType({int x, int y}) {
    if (x != null)
      this.x = x;
    if (y != null)
      this.y = y;
  }

  /*
   * Private constructor
   */
  Operatable_typeDictType._proxy(_jsObject) : super._proxy(_jsObject);

  /*
   * Public accessors
   */
  int get x => JS('int', '#.x', this._jsObject);

  void set x(int x) {
    JS('void', '#.x = #', this._jsObject, x);
  }

  int get y => JS('int', '#.y', this._jsObject);

  void set y(int y) {
    JS('void', '#.y = #', this._jsObject, y);
  }

}

class Operatable_typeOperatableType extends ChromeObject {
  /*
   * Private constructor
   */
  Operatable_typeOperatableType._proxy(_jsObject) : super._proxy(_jsObject);

  /*
   * Public accessors
   */
  /// Documentation for the String t.
  String get t => JS('String', '#.t', this._jsObject);

  void set t(String t) {
    JS('void', '#.t = #', this._jsObject, t);
  }


  /*
   * Methods
   */
  /// Function returning nothing, taking nothing.
  void voidFunc() => JS('void', '#.voidFunc()', this._jsObject);

  /// Function returning primitive type.
  int intRetFunc() => new int._proxy(JS('', '#.intRetFunc()', this._jsObject));

  /// Function returning dictionary.
  Operatable_typeDictType dictRetFunc() => new Operatable_typeDictType._proxy(JS('', '#.dictRetFunc()', this._jsObject));

  /// Function taking primitive type.
  void intArgFunc(int i) => JS('void', '#.intArgFunc(#)', this._jsObject, i);

  /// Function taking dict type.
  void dictArgFunc(Operatable_typeDictType d) => JS('void', '#.dictArgFunc(#)', this._jsObject, convertArgument(d));

}

/**
 * Functions
 */

class API_operatable_type {
  /*
   * API connection
   */
  Object _jsObject;
  API_operatable_type(this._jsObject) {
  }
}
