// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_CONFIG_VALUES_EXTRACTORS_H_
#define TOOLS_GN_CONFIG_VALUES_EXTRACTORS_H_

#include <ostream>
#include <string>
#include <vector>

#include "tools/gn/config.h"
#include "tools/gn/config_values.h"
#include "tools/gn/target.h"

struct EscapeOptions;

template<typename T, class Writer>
inline void ConfigValuesToStream(
    const ConfigValues& values,
    const std::vector<T>& (ConfigValues::* getter)() const,
    const Writer& writer,
    std::ostream& out) {
  const std::vector<T>& v = (values.*getter)();
  for (size_t i = 0; i < v.size(); i++)
    writer(v[i], out);
};

// Writes a given config value that applies to a given target. This collects
// all values from the target itself and all configs that apply, and writes
// then in order.
template<typename T, class Writer>
inline void RecursiveTargetConfigToStream(
    const Target* target,
    const std::vector<T>& (ConfigValues::* getter)() const,
    const Writer& writer,
    std::ostream& out) {
  // Note: if you make any changes to this, also change the writer in the
  // implementation of the "desc" command.

  // First write the values from the config itself.
  ConfigValuesToStream(target->config_values(), getter, writer, out);

  // Then write the configs in order.
  for (size_t i = 0; i < target->configs().size(); i++) {
    ConfigValuesToStream(target->configs()[i]->config_values(), getter,
                         writer, out);
  }
}

// Writes the values out as strings with no transformation.
void RecursiveTargetConfigStringsToStream(
    const Target* target,
    const std::vector<std::string>& (ConfigValues::* getter)() const,
    const EscapeOptions& escape_options,
    std::ostream& out);

#endif  // TOOLS_GN_CONFIG_VALUES_EXTRACTORS_H_
