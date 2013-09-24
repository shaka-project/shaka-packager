// Copyright (c) 2013, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

// Generated from namespace: functions

part of chrome;

/**
 * Types
 */

class FunctionsDictType extends ChromeObject {
  /*
   * Private constructor
   */
  FunctionsDictType._proxy(_jsObject) : super._proxy(_jsObject);

  /*
   * Public accessors
   */
  /// A field.
  int get a => JS('int', '#.a', this._jsObject);

  void set a(int a) {
    JS('void', '#.a = #', this._jsObject, a);
  }


  /*
   * Methods
   */
  /// A parameter.
  void voidFunc() => JS('void', '#.voidFunc()', this._jsObject);

}

/**
 * Functions
 */

class API_functions {
  /*
   * API connection
   */
  Object _jsObject;

  /*
   * Functions
   */
  /// Simple function.
  void voidFunc() => JS('void', '#.voidFunc()', this._jsObject);

  /// Function taking a non-optional argument.
  void argFunc(String s) => JS('void', '#.argFunc(#)', this._jsObject, s);

  /// Function taking an optional argument.
  void optionalArgFunc([String s]) => JS('void', '#.optionalArgFunc(#)', this._jsObject, s);

  /// Function taking a non-optional dictionary argument.
  void dictArgFunc(FunctionsDictType d) => JS('void', '#.dictArgFunc(#)', this._jsObject, convertArgument(d));

  /// Function taking an optional dictionary argument.
  void optionalDictArgFunc([FunctionsDictType d]) => JS('void', '#.optionalDictArgFunc(#)', this._jsObject, convertArgument(d));

  /// Function taking an entry argument.
  void entryArgFunc(Object entry) => JS('void', '#.entryArgFunc(#)', this._jsObject, convertArgument(entry));

  /// Function taking a simple callback.
  void callbackFunc(void c()) => JS('void', '#.callbackFunc(#)', this._jsObject, convertDartClosureToJS(c, 0));

  /// Function taking an optional simple callback.
  void optionalCallbackFunc([void c()]) => JS('void', '#.optionalCallbackFunc(#)', this._jsObject, convertDartClosureToJS(c, 0));

  /// Function taking a primitive callback.
  void primitiveCallbackFunc(void c(int i)) => JS('void', '#.primitiveCallbackFunc(#)', this._jsObject, convertDartClosureToJS(c, 1));

  /// Function taking a dictionary callback.
  void dictCallbackFunc(void c(DictType dict)) {
    void __proxy_callback(dict) {
      if (c != null) {
        c(new DictType._proxy(dict));
      }
    }
    JS('void', '#.dictCallbackFunc(#)', this._jsObject, convertDartClosureToJS(__proxy_callback, 1));
  }

  /// Function returning a dictionary.
  FunctionsDictType dictRetFunc() => new FunctionsDictType._proxy(JS('', '#.dictRetFunc()', this._jsObject));

  API_functions(this._jsObject) {
  }
}
