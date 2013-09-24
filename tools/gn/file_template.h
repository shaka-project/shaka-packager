// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_FILE_TEMPLATE_H_
#define TOOLS_GN_FILE_TEMPLATE_H_

#include "base/basictypes.h"
#include "base/containers/stack_container.h"
#include "tools/gn/err.h"
#include "tools/gn/value.h"

class ParseNode;

class FileTemplate {
 public:
  struct Subrange {
    enum Type {
      LITERAL = 0,
      SOURCE,
      NAME_PART,
      NUM_TYPES  // Must be last
    };
    Subrange(Type t, const std::string& l = std::string())
        : type(t),
          literal(l) {
    }

    Type type;

    // When type_ == LITERAL, this specifies the literal.
    std::string literal;
  };

  // Constructs a template from the given value. On error, the err will be
  // set. In this case you should not use this object.
  FileTemplate(const Value& t, Err* err);
  FileTemplate(const std::vector<std::string>& t);
  ~FileTemplate();

  // Applies this template to the given list of sources, appending all
  // results to the given dest list. The sources must be a list for the
  // one that takes a value as an input, otherwise the given error will be set.
  void Apply(const Value& sources,
             const ParseNode* origin,
             std::vector<Value>* dest,
             Err* err) const;
  void ApplyString(const std::string& input,
                   std::vector<std::string>* output) const;

  // Known template types.
  static const char kSource[];
  static const char kSourceNamePart[];

 private:
  typedef base::StackVector<Subrange, 8> Template;
  typedef base::StackVector<Template, 8> TemplateVector;

  void ParseInput(const Value& value, Err* err);

  // Parses a template string and adds it to the templates_ list.
  void ParseOneTemplateString(const std::string& str);

  TemplateVector templates_;

  // The corresponding value is set to true if the given subrange type is
  // required. This allows us to precompute these types whem applying them
  // to a given source file.
  bool types_required_[Subrange::NUM_TYPES];

  DISALLOW_COPY_AND_ASSIGN(FileTemplate);
};

#endif  // TOOLS_GN_FILE_TEMPLATE_H_
