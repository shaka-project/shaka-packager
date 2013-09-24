// Copyright (c) 2013, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

// Generated from namespace: events

part of chrome;

/**
 * Types
 */

class EventsEventArgumentElement extends ChromeObject {
  /*
   * Public constructor
   */
  EventsEventArgumentElement({String elementStringArg}) {
    if (elementStringArg != null)
      this.elementStringArg = elementStringArg;
  }

  /*
   * Private constructor
   */
  EventsEventArgumentElement._proxy(_jsObject) : super._proxy(_jsObject);

  /*
   * Public accessors
   */
  String get elementStringArg => JS('String', '#.elementStringArg', this._jsObject);

  void set elementStringArg(String elementStringArg) {
    JS('void', '#.elementStringArg = #', this._jsObject, elementStringArg);
  }

}

class EventsEventArgument extends ChromeObject {
  /*
   * Public constructor
   */
  EventsEventArgument({FileEntry entryArg, String stringArg, int intArg, List<EventsEventArgumentElement> elements, FileEntry optionalEntryArg, String optionalStringArg, int optionalIntArg, List<EventsEventArgumentElement> optionalElements}) {
    if (entryArg != null)
      this.entryArg = entryArg;
    if (stringArg != null)
      this.stringArg = stringArg;
    if (intArg != null)
      this.intArg = intArg;
    if (elements != null)
      this.elements = elements;
    if (optionalEntryArg != null)
      this.optionalEntryArg = optionalEntryArg;
    if (optionalStringArg != null)
      this.optionalStringArg = optionalStringArg;
    if (optionalIntArg != null)
      this.optionalIntArg = optionalIntArg;
    if (optionalElements != null)
      this.optionalElements = optionalElements;
  }

  /*
   * Private constructor
   */
  EventsEventArgument._proxy(_jsObject) : super._proxy(_jsObject);

  /*
   * Public accessors
   */
  /// A file entry
  FileEntry get entryArg => JS('FileEntry', '#.entryArg', this._jsObject);

  void set entryArg(FileEntry entryArg) {
    JS('void', '#.entryArg = #', this._jsObject, convertArgument(entryArg));
  }

  /// A string
  String get stringArg => JS('String', '#.stringArg', this._jsObject);

  void set stringArg(String stringArg) {
    JS('void', '#.stringArg = #', this._jsObject, stringArg);
  }

  /// A primitive
  int get intArg => JS('int', '#.intArg', this._jsObject);

  void set intArg(int intArg) {
    JS('void', '#.intArg = #', this._jsObject, intArg);
  }

  /// An array
  List<EventsEventArgumentElement> get elements {
    List<EventsEventArgumentElement> __proxy_elements = new List<EventsEventArgumentElement>();
    int count = JS('int', '#.elements.length', this._jsObject);
    for (int i = 0; i < count; i++) {
      var item = JS('', '#.elements[#]', this._jsObject, i);
      __proxy_elements.add(new EventsEventArgumentElement._proxy(item));
    }
    return __proxy_elements;
  }

  void set elements(List<EventsEventArgumentElement> elements) {
    JS('void', '#.elements = #', this._jsObject, convertArgument(elements));
  }

  /// Optional file entry
  FileEntry get optionalEntryArg => JS('FileEntry', '#.optionalEntryArg', this._jsObject);

  void set optionalEntryArg(FileEntry optionalEntryArg) {
    JS('void', '#.optionalEntryArg = #', this._jsObject, convertArgument(optionalEntryArg));
  }

  /// A string
  String get optionalStringArg => JS('String', '#.optionalStringArg', this._jsObject);

  void set optionalStringArg(String optionalStringArg) {
    JS('void', '#.optionalStringArg = #', this._jsObject, optionalStringArg);
  }

  /// A primitive
  int get optionalIntArg => JS('int', '#.optionalIntArg', this._jsObject);

  void set optionalIntArg(int optionalIntArg) {
    JS('void', '#.optionalIntArg = #', this._jsObject, optionalIntArg);
  }

  /// An array
  List<EventsEventArgumentElement> get optionalElements {
    List<EventsEventArgumentElement> __proxy_optionalElements = new List<EventsEventArgumentElement>();
    int count = JS('int', '#.optionalElements.length', this._jsObject);
    for (int i = 0; i < count; i++) {
      var item = JS('', '#.optionalElements[#]', this._jsObject, i);
      __proxy_optionalElements.add(new EventsEventArgumentElement._proxy(item));
    }
    return __proxy_optionalElements;
  }

  void set optionalElements(List<EventsEventArgumentElement> optionalElements) {
    JS('void', '#.optionalElements = #', this._jsObject, convertArgument(optionalElements));
  }

}

/**
 * Events
 */

/// Documentation for the first basic event.
class Event_events_firstBasicEvent extends Event {
  void addListener(void callback()) => super.addListener(callback);

  void removeListener(void callback()) => super.removeListener(callback);

  bool hasListener(void callback()) => super.hasListener(callback);

  Event_events_firstBasicEvent(jsObject) : super._(jsObject, 0);
}

/// Documentation for the second basic event.
class Event_events_secondBasicEvent extends Event {
  void addListener(void callback()) => super.addListener(callback);

  void removeListener(void callback()) => super.removeListener(callback);

  bool hasListener(void callback()) => super.hasListener(callback);

  Event_events_secondBasicEvent(jsObject) : super._(jsObject, 0);
}

/// Documentation for an event with a non-optional primitive argument.
class Event_events_nonOptionalPrimitiveArgEvent extends Event {
  void addListener(void callback(int argument)) => super.addListener(callback);

  void removeListener(void callback(int argument)) => super.removeListener(callback);

  bool hasListener(void callback(int argument)) => super.hasListener(callback);

  Event_events_nonOptionalPrimitiveArgEvent(jsObject) : super._(jsObject, 1);
}

/// Documentation for an event with an optional primitive argument.
class Event_events_optionalPrimitiveArgEvent extends Event {
  void addListener(void callback(int argument)) => super.addListener(callback);

  void removeListener(void callback(int argument)) => super.removeListener(callback);

  bool hasListener(void callback(int argument)) => super.hasListener(callback);

  Event_events_optionalPrimitiveArgEvent(jsObject) : super._(jsObject, 1);
}

/// Documentation for an event with a non-optional dictionary argument.
class Event_events_nonOptionalDictArgEvent extends Event {
  void addListener(void callback(EventsEventArgument argument)) {
    void __proxy_callback(argument) {
      if (callback != null) {
        callback(new EventsEventArgument._proxy(argument));
      }
    }
    super.addListener(__proxy_callback);
  }

  void removeListener(void callback(EventsEventArgument argument)) {
    void __proxy_callback(argument) {
      if (callback != null) {
        callback(new EventsEventArgument._proxy(argument));
      }
    }
    super.removeListener(__proxy_callback);
  }

  bool hasListener(void callback(EventsEventArgument argument)) {
    void __proxy_callback(argument) {
      if (callback != null) {
        callback(new EventsEventArgument._proxy(argument));
      }
    }
    super.hasListener(__proxy_callback);
  }

  Event_events_nonOptionalDictArgEvent(jsObject) : super._(jsObject, 1);
}

/// Documentation for an event with a optional dictionary argument.
class Event_events_optionalDictArgEvent extends Event {
  void addListener(void callback(EventsEventArgument argument)) {
    void __proxy_callback(argument) {
      if (callback != null) {
        callback(new EventsEventArgument._proxy(argument));
      }
    }
    super.addListener(__proxy_callback);
  }

  void removeListener(void callback(EventsEventArgument argument)) {
    void __proxy_callback(argument) {
      if (callback != null) {
        callback(new EventsEventArgument._proxy(argument));
      }
    }
    super.removeListener(__proxy_callback);
  }

  bool hasListener(void callback(EventsEventArgument argument)) {
    void __proxy_callback(argument) {
      if (callback != null) {
        callback(new EventsEventArgument._proxy(argument));
      }
    }
    super.hasListener(__proxy_callback);
  }

  Event_events_optionalDictArgEvent(jsObject) : super._(jsObject, 1);
}

/**
 * Functions
 */

class API_events {
  /*
   * API connection
   */
  Object _jsObject;

  /*
   * Events
   */
  Event_events_firstBasicEvent firstBasicEvent;
  Event_events_secondBasicEvent secondBasicEvent;
  Event_events_nonOptionalPrimitiveArgEvent nonOptionalPrimitiveArgEvent;
  Event_events_optionalPrimitiveArgEvent optionalPrimitiveArgEvent;
  Event_events_nonOptionalDictArgEvent nonOptionalDictArgEvent;
  Event_events_optionalDictArgEvent optionalDictArgEvent;
  API_events(this._jsObject) {
    firstBasicEvent = new Event_events_firstBasicEvent(JS('', '#.firstBasicEvent', this._jsObject));
    secondBasicEvent = new Event_events_secondBasicEvent(JS('', '#.secondBasicEvent', this._jsObject));
    nonOptionalPrimitiveArgEvent = new Event_events_nonOptionalPrimitiveArgEvent(JS('', '#.nonOptionalPrimitiveArgEvent', this._jsObject));
    optionalPrimitiveArgEvent = new Event_events_optionalPrimitiveArgEvent(JS('', '#.optionalPrimitiveArgEvent', this._jsObject));
    nonOptionalDictArgEvent = new Event_events_nonOptionalDictArgEvent(JS('', '#.nonOptionalDictArgEvent', this._jsObject));
    optionalDictArgEvent = new Event_events_optionalDictArgEvent(JS('', '#.optionalDictArgEvent', this._jsObject));
  }
}
