// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate enum and a java class for the values.

#ifndef DEFINE_MEMORY_PRESSURE_LEVEL
#error "DEFINE_MEMORY_PRESSURE_LEVEL should be defined."
#endif

// Modules are advised to free buffers that are cheap to re-allocate and not
// immediately needed.
DEFINE_MEMORY_PRESSURE_LEVEL(MEMORY_PRESSURE_MODERATE, 0)

// At this level, modules are advised to free all possible memory.
// The alternative is to be killed by the system, which means all memory will
// have to be re-created, plus the cost of a cold start.
DEFINE_MEMORY_PRESSURE_LEVEL(MEMORY_PRESSURE_CRITICAL, 2)
