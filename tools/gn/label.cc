// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/label.h"

#include "base/logging.h"
#include "tools/gn/err.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/value.h"

namespace {

// We print user visible label names with no trailing slash after the
// directory name.
std::string DirWithNoTrailingSlash(const SourceDir& dir) {
  // Be careful not to trim if the input is just "/" or "//".
  if (dir.value().size() > 2)
    return dir.value().substr(0, dir.value().size() - 1);
  return dir.value();
}

// Given the separate-out input (everything before the colon) in the dep rule,
// computes the final build rule. Sets err on failure. On success,
// |*used_implicit| will be set to whether the implicit current directory was
// used. The value is used only for generating error messages.
bool ComputeBuildLocationFromDep(const Value& input_value,
                                 const SourceDir& current_dir,
                                 const base::StringPiece& input,
                                 SourceDir* result,
                                 Err* err) {
  // No rule, use the current locaton.
  if (input.empty()) {
    *result = current_dir;
    return true;
  }

  // Don't allow directories to start with a single slash. All labels must be
  // in the source root.
  if (input[0] == '/' && (input.size() == 1 || input[1] != '/')) {
    *err = Err(input_value, "Label can't start with a single slash",
        "Labels must be either relative (no slash at the beginning) or be "
        "absolute\ninside the source root (two slashes at the beginning).");
    return false;
  }

  *result = current_dir.ResolveRelativeDir(input);
  return true;
}

// Given the separated-out target name (after the colon) computes the final
// name, using the implicit name from the previously-generated
// computed_location if necessary. The input_value is used only for generating
// error messages.
bool ComputeTargetNameFromDep(const Value& input_value,
                              const SourceDir& computed_location,
                              const base::StringPiece& input,
                              std::string* result,
                              Err* err) {
  if (!input.empty()) {
    // Easy case: input is specified, just use it.
    result->assign(input.data(), input.size());
    return true;
  }

  const std::string& loc = computed_location.value();

  // Use implicit name. The path will be "//", "//base/", "//base/i18n/", etc.
  if (loc.size() <= 1) {
    *err = Err(input_value, "This dependency name is empty");
    return false;
  }

  size_t next_to_last_slash = loc.rfind('/', loc.size() - 2);
  DCHECK(next_to_last_slash != std::string::npos);
  result->assign(&loc[next_to_last_slash + 1],
                 loc.size() - next_to_last_slash - 2);
  return true;
}

// The original value is used only for error reporting, use the |input| as the
// input to this function (which may be a substring of the original value when
// we're parsing toolchains.
//
// If the output toolchain vars are NULL, then we'll report an error if we
// find a toolchain specified (this is used when recursively parsing toolchain
// labels which themselves can't have toolchain specs).
//
// We assume that the output variables are initialized to empty so we don't
// write them unless we need them to contain something.
//
// Returns true on success. On failure, the out* variables might be written to
// but shouldn't be used.
bool Resolve(const SourceDir& current_dir,
             const Label& current_toolchain,
             const Value& original_value,
             const base::StringPiece& input,
             SourceDir* out_dir,
             std::string* out_name,
             SourceDir* out_toolchain_dir,
             std::string* out_toolchain_name,
             Err* err) {
  // To workaround the problem that StringPiece operator[] doesn't return a ref.
  const char* input_str = input.data();

  size_t path_separator = input.find_first_of(":(");
  base::StringPiece location_piece;
  base::StringPiece name_piece;
  base::StringPiece toolchain_piece;
  if (path_separator == std::string::npos) {
    location_piece = input;
    // Leave name & toolchain piece null.
  } else {
    location_piece = base::StringPiece(&input_str[0], path_separator);

    size_t toolchain_separator = input.find('(', path_separator);
    if (toolchain_separator == std::string::npos) {
      name_piece = base::StringPiece(&input_str[path_separator + 1],
                                     input.size() - path_separator - 1);
      // Leave location piece null.
    } else if (!out_toolchain_dir) {
      // Toolchain specified but not allows in this context.
      *err = Err(original_value, "Toolchain has a toolchain.",
          "Your toolchain definition (inside the parens) seems to itself "
          "have a\ntoolchain. Don't do this.");
      return false;
    } else {
      // Name piece is everything between the two separators. Note that the
      // separators may be the same (e.g. "//foo(bar)" which means empty name.
      if (toolchain_separator > path_separator) {
        name_piece = base::StringPiece(
            &input_str[path_separator + 1],
            toolchain_separator - path_separator - 1);
      }

      // Toolchain name should end in a ) and this should be the end of the
      // string.
      if (input[input.size() - 1] != ')') {
        *err = Err(original_value, "Bad toolchain name.",
            "Toolchain name must end in a \")\" at the end of the label.");
        return false;
      }

      // Subtract off the two parens to just get the toolchain name.
      toolchain_piece = base::StringPiece(
          &input_str[toolchain_separator + 1],
          input.size() - toolchain_separator - 2);
    }
  }

  // Everything before the separator is the filename.
  // We allow three cases:
  //   Absolute:                "//foo:bar" -> /foo:bar
  //   Target in current file:  ":foo"     -> <currentdir>:foo
  //   Path with implicit name: "/foo"     -> /foo:foo
  if (location_piece.empty() && name_piece.empty()) {
    // Can't use both implicit filename and name (":").
    *err = Err(original_value, "This doesn't specify a dependency.");
    return false;
  }

  if (!ComputeBuildLocationFromDep(original_value, current_dir, location_piece,
                                   out_dir, err))
    return false;

  if (!ComputeTargetNameFromDep(original_value, *out_dir, name_piece,
                                out_name, err))
    return false;

  // Last, do the toolchains.
  if (out_toolchain_dir) {
    // Handle empty toolchain strings. We don't allow normal labels to be
    // empty so we can't allow the recursive call of this function to do this
    // check.
    if (toolchain_piece.empty()) {
      *out_toolchain_dir = current_toolchain.dir();
      *out_toolchain_name = current_toolchain.name();
      return true;
    } else {
      return Resolve(current_dir, current_toolchain,
                     original_value, toolchain_piece,
                     out_toolchain_dir, out_toolchain_name, NULL, NULL, err);
    }
  }
  return true;
}

}  // namespace

Label::Label() {
}

Label::Label(const SourceDir& dir,
             const base::StringPiece& name,
             const SourceDir& toolchain_dir,
             const base::StringPiece& toolchain_name)
    : dir_(dir),
      toolchain_dir_(toolchain_dir) {
  name_.assign(name.data(), name.size());
  toolchain_name_.assign(toolchain_name.data(), toolchain_name.size());
}

Label::~Label() {
}

// static
Label Label::Resolve(const SourceDir& current_dir,
                     const Label& current_toolchain,
                     const Value& input,
                     Err* err) {
  Label ret;
  if (input.type() != Value::STRING) {
    *err = Err(input, "Dependency is not a string.");
    return ret;
  }
  const std::string& input_string = input.string_value();
  if (input_string.empty()) {
    *err = Err(input, "Dependency string is empty.");
    return ret;
  }

  if (!::Resolve(current_dir, current_toolchain, input, input_string,
                 &ret.dir_, &ret.name_,
                 &ret.toolchain_dir_, &ret.toolchain_name_,
                 err))
    return Label();
  return ret;
}

Label Label::GetToolchainLabel() const {
  return Label(toolchain_dir_, toolchain_name_,
               SourceDir(), base::StringPiece());
}

std::string Label::GetUserVisibleName(bool include_toolchain) const {
  std::string ret;
  ret.reserve(dir_.value().size() + name_.size() + 1);

  if (dir_.is_null())
    return ret;

  ret = DirWithNoTrailingSlash(dir_);
  ret.push_back(':');
  ret.append(name_);

  if (include_toolchain) {
    ret.push_back('(');
    if (!toolchain_dir_.is_null() && !toolchain_name_.empty()) {
      ret.append(DirWithNoTrailingSlash(toolchain_dir_));
      ret.push_back(':');
      ret.append(toolchain_name_);
    }
    ret.push_back(')');
  }
  return ret;
}

std::string Label::GetUserVisibleName(const Label& default_toolchain) const {
  bool include_toolchain =
      default_toolchain.dir() != toolchain_dir_ ||
      default_toolchain.name() != toolchain_name_;
  return GetUserVisibleName(include_toolchain);
}
