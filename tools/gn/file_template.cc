// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/file_template.h"

#include "tools/gn/filesystem_utils.h"

const char FileTemplate::kSource[] = "{{source}}";
const char FileTemplate::kSourceNamePart[] = "{{source_name_part}}";

FileTemplate::FileTemplate(const Value& t, Err* err) {
  ParseInput(t, err);
}

FileTemplate::FileTemplate(const std::vector<std::string>& t) {
  for (size_t i = 0; i < t.size(); i++)
    ParseOneTemplateString(t[i]);
}

FileTemplate::~FileTemplate() {
}

void FileTemplate::Apply(const Value& sources,
                         const ParseNode* origin,
                         std::vector<Value>* dest,
                         Err* err) const {
  if (!sources.VerifyTypeIs(Value::LIST, err))
    return;
  dest->reserve(sources.list_value().size() * templates_.container().size());

  // Temporary holding place, allocate outside to re-use- buffer.
  std::vector<std::string> string_output;

  const std::vector<Value>& sources_list = sources.list_value();
  for (size_t i = 0; i < sources_list.size(); i++) {
    if (!sources_list[i].VerifyTypeIs(Value::STRING, err))
      return;

    ApplyString(sources_list[i].string_value(), &string_output);
    for (size_t out_i = 0; out_i < string_output.size(); out_i++)
      dest->push_back(Value(origin, string_output[i]));
  }
}

void FileTemplate::ApplyString(const std::string& str,
                               std::vector<std::string>* output) const {
  // Compute all substitutions needed so we can just do substitutions below.
  // We skip the LITERAL one since that varies each time.
  std::string subst[Subrange::NUM_TYPES];
  if (types_required_[Subrange::SOURCE])
    subst[Subrange::SOURCE] = str;
  if (types_required_[Subrange::NAME_PART])
    subst[Subrange::NAME_PART] = FindFilenameNoExtension(&str).as_string();

  output->resize(templates_.container().size());
  for (size_t template_i = 0;
       template_i < templates_.container().size(); template_i++) {
    const Template& t = templates_[template_i];
    (*output)[template_i].clear();
    for (size_t subrange_i = 0; subrange_i < t.container().size();
         subrange_i++) {
      if (t[subrange_i].type == Subrange::LITERAL)
        (*output)[template_i].append(t[subrange_i].literal);
      else
        (*output)[template_i].append(subst[t[subrange_i].type]);
    }
  }
}

void FileTemplate::ParseInput(const Value& value, Err* err) {
  switch (value.type()) {
    case Value::STRING:
      ParseOneTemplateString(value.string_value());
      break;
    case Value::LIST:
      for (size_t i = 0; i < value.list_value().size(); i++) {
        if (!value.list_value()[i].VerifyTypeIs(Value::STRING, err))
          return;
        ParseOneTemplateString(value.list_value()[i].string_value());
      }
      break;
    default:
      *err = Err(value, "File template must be a string or list.",
                 "A sarcastic comment about your skills goes here.");
  }
}

void FileTemplate::ParseOneTemplateString(const std::string& str) {
  templates_.container().resize(templates_.container().size() + 1);
  Template& t = templates_[templates_.container().size() - 1];

  size_t cur = 0;
  while (true) {
    size_t next = str.find("{{", cur);

    // Pick up everything from the previous spot to here as a literal.
    if (next == std::string::npos) {
      if (cur != str.size())
        t.container().push_back(Subrange(Subrange::LITERAL, str.substr(cur)));
      break;
    } else if (next > cur) {
      t.container().push_back(
          Subrange(Subrange::LITERAL, str.substr(cur, next - cur)));
    }

    // Decode the template param.
    if (str.compare(next, arraysize(kSource) - 1, kSource) == 0) {
      t.container().push_back(Subrange(Subrange::SOURCE));
      types_required_[Subrange::SOURCE] = true;
      cur = next + arraysize(kSource) - 1;
    } else if (str.compare(next, arraysize(kSourceNamePart) - 1,
                           kSourceNamePart) == 0) {
      t.container().push_back(Subrange(Subrange::NAME_PART));
      types_required_[Subrange::NAME_PART] = true;
      cur = next + arraysize(kSourceNamePart) - 1;
    } else {
      // If it's not a match, treat it like a one-char literal (this will be
      // rare, so it's not worth the bother to add to the previous literal) so
      // we can keep going.
      t.container().push_back(Subrange(Subrange::LITERAL, "{"));
      cur = next + 1;
    }
  }
}
