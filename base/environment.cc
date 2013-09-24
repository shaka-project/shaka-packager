// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"

#if defined(OS_POSIX)
#include <stdlib.h>
#elif defined(OS_WIN)
#include <windows.h>
#endif

#include "base/strings/string_util.h"

#if defined(OS_WIN)
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#endif

namespace {

class EnvironmentImpl : public base::Environment {
 public:
  virtual bool GetVar(const char* variable_name,
                      std::string* result) OVERRIDE {
    if (GetVarImpl(variable_name, result))
      return true;

    // Some commonly used variable names are uppercase while others
    // are lowercase, which is inconsistent. Let's try to be helpful
    // and look for a variable name with the reverse case.
    // I.e. HTTP_PROXY may be http_proxy for some users/systems.
    char first_char = variable_name[0];
    std::string alternate_case_var;
    if (first_char >= 'a' && first_char <= 'z')
      alternate_case_var = StringToUpperASCII(std::string(variable_name));
    else if (first_char >= 'A' && first_char <= 'Z')
      alternate_case_var = StringToLowerASCII(std::string(variable_name));
    else
      return false;
    return GetVarImpl(alternate_case_var.c_str(), result);
  }

  virtual bool SetVar(const char* variable_name,
                      const std::string& new_value) OVERRIDE {
    return SetVarImpl(variable_name, new_value);
  }

  virtual bool UnSetVar(const char* variable_name) OVERRIDE {
    return UnSetVarImpl(variable_name);
  }

 private:
  bool GetVarImpl(const char* variable_name, std::string* result) {
#if defined(OS_POSIX)
    const char* env_value = getenv(variable_name);
    if (!env_value)
      return false;
    // Note that the variable may be defined but empty.
    if (result)
      *result = env_value;
    return true;
#elif defined(OS_WIN)
    DWORD value_length = ::GetEnvironmentVariable(
        UTF8ToWide(variable_name).c_str(), NULL, 0);
    if (value_length == 0)
      return false;
    if (result) {
      scoped_ptr<wchar_t[]> value(new wchar_t[value_length]);
      ::GetEnvironmentVariable(UTF8ToWide(variable_name).c_str(), value.get(),
                               value_length);
      *result = WideToUTF8(value.get());
    }
    return true;
#else
#error need to port
#endif
  }

  bool SetVarImpl(const char* variable_name, const std::string& new_value) {
#if defined(OS_POSIX)
    // On success, zero is returned.
    return !setenv(variable_name, new_value.c_str(), 1);
#elif defined(OS_WIN)
    // On success, a nonzero value is returned.
    return !!SetEnvironmentVariable(UTF8ToWide(variable_name).c_str(),
                                    UTF8ToWide(new_value).c_str());
#endif
  }

  bool UnSetVarImpl(const char* variable_name) {
#if defined(OS_POSIX)
    // On success, zero is returned.
    return !unsetenv(variable_name);
#elif defined(OS_WIN)
    // On success, a nonzero value is returned.
    return !!SetEnvironmentVariable(UTF8ToWide(variable_name).c_str(), NULL);
#endif
  }
};

}  // namespace

namespace base {

namespace env_vars {

#if defined(OS_POSIX)
// On Posix systems, this variable contains the location of the user's home
// directory. (e.g, /home/username/).
const char kHome[] = "HOME";
#endif

}  // namespace env_vars

Environment::~Environment() {}

// static
Environment* Environment::Create() {
  return new EnvironmentImpl();
}

bool Environment::HasVar(const char* variable_name) {
  return GetVar(variable_name, NULL);
}

}  // namespace base
