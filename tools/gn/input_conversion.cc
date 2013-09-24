// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "input_conversion.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/err.h"
#include "tools/gn/input_file.h"
#include "tools/gn/label.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/parser.h"
#include "tools/gn/scope.h"
#include "tools/gn/settings.h"
#include "tools/gn/tokenizer.h"
#include "tools/gn/value.h"

namespace {

// Returns the "first bit" of some script output for writing to error messages.
std::string GetExampleOfBadInput(const std::string& input) {
  std::string result(input);

  // Maybe the result starts with a blank line or something, which we don't
  // want.
  TrimWhitespaceASCII(result, TRIM_ALL, &result);

  // Now take the first line, or the first set of chars, whichever is shorter.
  bool trimmed = false;
  size_t newline_offset = result.find('\n');
  if (newline_offset != std::string::npos) {
    trimmed = true;
    result.resize(newline_offset);
  }
  TrimWhitespaceASCII(result, TRIM_ALL, &result);

  const size_t kMaxSize = 50;
  if (result.size() > kMaxSize) {
    trimmed = true;
    result.resize(kMaxSize);
  }

  if (trimmed)
    result.append("...");
  return result;
}

// When parsing the result as a value, we may get various types of errors.
// This creates an error message for this case with an optional nested error
// message to reference. If there is no nested err, pass Err().
//
// This code also takes care to rewrite the original error which will reference
// the temporary InputFile which won't exist when the error is propogated
// out to a higher level.
Err MakeParseErr(const std::string& input,
                 const ParseNode* origin,
                 const Err& nested) {
  std::string help_text =
      "When parsing a result as a \"value\" it should look like a list:\n"
      "  [ \"a\", \"b\", 5 ]\n"
      "or a single literal:\n"
      "  \"my result\"\n"
      "but instead I got this, which I find very confusing:\n";
  help_text.append(input);
  if (nested.has_error())
    help_text.append("\nThe exact error was:");

  Err result(origin, "Script result wasn't a valid value.", help_text);
  if (nested.has_error()) {
    result.AppendSubErr(Err(LocationRange(), nested.message(),
                            nested.help_text()));
  }
  return result;
}

// Sets the origin of the value and any nested values with the given node.
void RecursivelySetOrigin(Value* value, const ParseNode* origin) {
  value->set_origin(origin);
  if (value->type() == Value::LIST) {
    std::vector<Value>& list_value = value->list_value();
    for (size_t i = 0; i < list_value.size(); i++)
      RecursivelySetOrigin(&list_value[i], origin);
  }
}

Value ParseString(const std::string& input,
                  const ParseNode* origin,
                  Err* err) {
  SourceFile empty_source_for_most_vexing_parse;
  InputFile input_file(empty_source_for_most_vexing_parse);
  input_file.SetContents(input);

  std::vector<Token> tokens = Tokenizer::Tokenize(&input_file, err);
  if (err->has_error()) {
    *err = MakeParseErr(input, origin, *err);
    return Value();
  }

  scoped_ptr<ParseNode> expression = Parser::ParseExpression(tokens, err);
  if (err->has_error()) {
    *err = MakeParseErr(input, origin, *err);
    return Value();
  }

  // It's valid for the result to be a null pointer, this just means that the
  // script returned nothing.
  if (!expression)
    return Value();

  // The result should either be a list or a literal, anything else is
  // invalid.
  if (!expression->AsList() && !expression->AsLiteral()) {
    *err = MakeParseErr(input, origin, Err());
    return Value();
  }

  BuildSettings build_settings;
  Label empty_label;
  Toolchain toolchain(empty_label);
  Settings settings(&build_settings, &toolchain, std::string());
  Scope scope(&settings);

  Err nested_err;
  Value result = expression->Execute(&scope, &nested_err);
  if (nested_err.has_error()) {
    *err = MakeParseErr(input, origin, nested_err);
    return Value();
  }

  // The returned value will have references to the temporary parse nodes we
  // made on the stack. If the values are used in an error message in the
  // future, this will crash. Reset the origin of all values to be our
  // containing origin.
  RecursivelySetOrigin(&result, origin);
  return result;
}

Value ParseList(const std::string& input,
                const ParseNode* origin,
                Err* err) {
  Value ret(origin, Value::LIST);
  std::vector<std::string> as_lines;
  base::SplitString(input, '\n', &as_lines);

  // Trim empty lines from the end.
  // Do we want to make this configurable?
  while (!as_lines.empty() && as_lines[as_lines.size() - 1].empty())
    as_lines.resize(as_lines.size() - 1);

  ret.list_value().reserve(as_lines.size());
  for (size_t i = 0; i < as_lines.size(); i++)
    ret.list_value().push_back(Value(origin, as_lines[i]));
  return ret;
}

}  // namespace

extern const char kInputConversion_Help[] =
    "input_conversion: Specifies how to transform input to a variable.\n"
    "\n"
    "  input_conversion is an argument to read_file and exec_script that\n"
    "  specifies how the result of the read operation should be converted\n"
    "  into a variable.\n"
    "\n"
    "  \"list lines\"\n"
    "      Return the file contents as a list, with a string for each line.\n"
    "      The newlines will not be present in the result. Empty newlines\n"
    "      will be trimmed from the trailing end of the returned list.\n"
    "\n"
    "  \"value\"\n"
    "      Parse the input as if it was a literal rvalue in a buildfile.\n"
    "      Examples of typical program output using this mode:\n"
    "        [ \"foo\", \"bar\" ]     (result will be a list)\n"
    "      or\n"
    "        \"foo bar\"            (result will be a string)\n"
    "      or\n"
    "        5                    (result will be an integer)\n"
    "\n"
    "      Note that if the input is empty, the result will be a null value\n"
    "      which will produce an error if assigned to a variable.\n"
    "\n"
    "  \"string\"\n"
    "      Return the file contents into a single string.\n";

Value ConvertInputToValue(const std::string& input,
                          const ParseNode* origin,
                          const Value& input_conversion_value,
                          Err* err) {
  if (!input_conversion_value.VerifyTypeIs(Value::STRING, err))
    return Value();
  const std::string& input_conversion = input_conversion_value.string_value();

  if (input_conversion == "value")
    return ParseString(input, origin, err);
  if (input_conversion == "string")
    return Value(origin, input);
  if (input_conversion == "list lines")
    return ParseList(input, origin, err);

  *err = Err(input_conversion_value, "Not a valid read file mode.",
             "Have you considered a career in retail?");
  return Value();
}
