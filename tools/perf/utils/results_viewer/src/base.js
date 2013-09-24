// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';


/**
 * The global object.
 * @type {!Object}
 * @const
 */
var global = this;


/** Platform, package, object property, and Event support. */
this.base = (function() {

  function mLog(text, opt_indentLevel) {
    if (true)
      return;

    var spacing = '';
    var indentLevel = opt_indentLevel || 0;
    for (var i = 0; i < indentLevel; i++)
      spacing += ' ';
    console.log(spacing + text);
  }

  /**
   * Builds an object structure for the provided namespace path,
   * ensuring that names that already exist are not overwritten. For
   * example:
   * 'a.b.c' -> a = {};a.b={};a.b.c={};
   * @param {string} name Name of the object that this file defines.
   * @param {*=} opt_object The object to expose at the end of the path.
   * @param {Object=} opt_objectToExportTo The object to add the path to;
   *     default is {@code global}.
   * @private
   */
  function exportPath(name, opt_object, opt_objectToExportTo) {
    var parts = name.split('.');
    var cur = opt_objectToExportTo || global;

    for (var part; parts.length && (part = parts.shift());) {
      if (!parts.length && opt_object !== undefined) {
        // last part and we have an object; use it
        cur[part] = opt_object;
      } else if (part in cur) {
        cur = cur[part];
      } else {
        cur = cur[part] = {};
      }
    }
    return cur;
  }

  function exportTo(namespace, fn) {
    var obj = exportPath(namespace);
    try {
      var exports = fn();
    } catch (e) {
      console.log('While running exports for ', name, ':');
      console.log(e.stack || e);
      return;
    }

    for (var propertyName in exports) {
      // Maybe we should check the prototype chain here? The current usage
      // pattern is always using an object literal so we only care about own
      // properties.
      var propertyDescriptor = Object.getOwnPropertyDescriptor(exports,
                                                               propertyName);
      if (propertyDescriptor) {
        Object.defineProperty(obj, propertyName, propertyDescriptor);
        mLog('  +' + propertyName);
      }
    }
  }

  return {
    exportTo: exportTo
  };
})();