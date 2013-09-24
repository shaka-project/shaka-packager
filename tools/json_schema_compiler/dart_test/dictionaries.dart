// Copyright (c) 2013, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

// Generated from namespace: dictionaries

part of chrome;

/**
 * Types
 */

class DictionariesInnerType extends ChromeObject {
  /*
   * Public constructor
   */
  DictionariesInnerType({String s, int b, int i, int l, double d, FileEntry f, String os, int ob, int oi, int ol, double od, FileEntry of}) {
    if (s != null)
      this.s = s;
    if (b != null)
      this.b = b;
    if (i != null)
      this.i = i;
    if (l != null)
      this.l = l;
    if (d != null)
      this.d = d;
    if (f != null)
      this.f = f;
    if (os != null)
      this.os = os;
    if (ob != null)
      this.ob = ob;
    if (oi != null)
      this.oi = oi;
    if (ol != null)
      this.ol = ol;
    if (od != null)
      this.od = od;
    if (of != null)
      this.of = of;
  }

  /*
   * Private constructor
   */
  DictionariesInnerType._proxy(_jsObject) : super._proxy(_jsObject);

  /*
   * Public accessors
   */
  /// Documentation for the String s.
  String get s => JS('String', '#.s', this._jsObject);

  void set s(String s) {
    JS('void', '#.s = #', this._jsObject, s);
  }

  /// Documentation for the boolean b.
  int get b => JS('int', '#.b', this._jsObject);

  void set b(int b) {
    JS('void', '#.b = #', this._jsObject, b);
  }

  /// Documentation for the int i.
  int get i => JS('int', '#.i', this._jsObject);

  void set i(int i) {
    JS('void', '#.i = #', this._jsObject, i);
  }

  /// Documentation for the long l.
  int get l => JS('int', '#.l', this._jsObject);

  void set l(int l) {
    JS('void', '#.l = #', this._jsObject, l);
  }

  /// Documentation for the double d.
  double get d => JS('double', '#.d', this._jsObject);

  void set d(double d) {
    JS('void', '#.d = #', this._jsObject, d);
  }

  /// Documentation for the file entry f.
  FileEntry get f => JS('FileEntry', '#.f', this._jsObject);

  void set f(FileEntry f) {
    JS('void', '#.f = #', this._jsObject, convertArgument(f));
  }

  /// Documentation for the optional String s.
  String get os => JS('String', '#.os', this._jsObject);

  void set os(String os) {
    JS('void', '#.os = #', this._jsObject, os);
  }

  /// Documentation for the optional boolean ob.
  int get ob => JS('int', '#.ob', this._jsObject);

  void set ob(int ob) {
    JS('void', '#.ob = #', this._jsObject, ob);
  }

  /// Documentation for the optional int i.
  int get oi => JS('int', '#.oi', this._jsObject);

  void set oi(int oi) {
    JS('void', '#.oi = #', this._jsObject, oi);
  }

  /// Documentation for the optional long l.
  int get ol => JS('int', '#.ol', this._jsObject);

  void set ol(int ol) {
    JS('void', '#.ol = #', this._jsObject, ol);
  }

  /// Documentation for the optional double d.
  double get od => JS('double', '#.od', this._jsObject);

  void set od(double od) {
    JS('void', '#.od = #', this._jsObject, od);
  }

  /// Documentation for the optional file entry f.
  FileEntry get of => JS('FileEntry', '#.of', this._jsObject);

  void set of(FileEntry of) {
    JS('void', '#.of = #', this._jsObject, convertArgument(of));
  }

}

class DictionariesOuterType extends ChromeObject {
  /*
   * Public constructor
   */
  DictionariesOuterType({List<DictionariesInnerType> items, List<DictionariesInnerType> oitems}) {
    if (items != null)
      this.items = items;
    if (oitems != null)
      this.oitems = oitems;
  }

  /*
   * Private constructor
   */
  DictionariesOuterType._proxy(_jsObject) : super._proxy(_jsObject);

  /*
   * Public accessors
   */
  /// Documentation for the array of InnerTypes items.
  List<DictionariesInnerType> get items {
    List<DictionariesInnerType> __proxy_items = new List<DictionariesInnerType>();
    int count = JS('int', '#.items.length', this._jsObject);
    for (int i = 0; i < count; i++) {
      var item = JS('', '#.items[#]', this._jsObject, i);
      __proxy_items.add(new DictionariesInnerType._proxy(item));
    }
    return __proxy_items;
  }

  void set items(List<DictionariesInnerType> items) {
    JS('void', '#.items = #', this._jsObject, convertArgument(items));
  }

  /// Documentation for the optional array of Inner Types oitems.
  List<DictionariesInnerType> get oitems {
    List<DictionariesInnerType> __proxy_oitems = new List<DictionariesInnerType>();
    int count = JS('int', '#.oitems.length', this._jsObject);
    for (int i = 0; i < count; i++) {
      var item = JS('', '#.oitems[#]', this._jsObject, i);
      __proxy_oitems.add(new DictionariesInnerType._proxy(item));
    }
    return __proxy_oitems;
  }

  void set oitems(List<DictionariesInnerType> oitems) {
    JS('void', '#.oitems = #', this._jsObject, convertArgument(oitems));
  }

}

class DictionariesComplexType extends ChromeObject {
  /*
   * Public constructor
   */
  DictionariesComplexType({int i, DictionariesComplexType c}) {
    if (i != null)
      this.i = i;
    if (c != null)
      this.c = c;
  }

  /*
   * Private constructor
   */
  DictionariesComplexType._proxy(_jsObject) : super._proxy(_jsObject);

  /*
   * Public accessors
   */
  /// Documentation for the int i.
  int get i => JS('int', '#.i', this._jsObject);

  void set i(int i) {
    JS('void', '#.i = #', this._jsObject, i);
  }

  /// Documentation for the ComplexType c.
  DictionariesComplexType get c => new DictionariesComplexType._proxy(JS('', '#.c', this._jsObject));

  void set c(DictionariesComplexType c) {
    JS('void', '#.c = #', this._jsObject, convertArgument(c));
  }

}

/**
 * Functions
 */

class API_dictionaries {
  /*
   * API connection
   */
  Object _jsObject;
  API_dictionaries(this._jsObject) {
  }
}
