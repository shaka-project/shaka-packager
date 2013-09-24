// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/toolchain.h"

#include "base/logging.h"

const char* Toolchain::kToolCc = "cc";
const char* Toolchain::kToolCxx = "cxx";
const char* Toolchain::kToolObjC = "objc";
const char* Toolchain::kToolObjCxx = "objcxx";
const char* Toolchain::kToolAsm = "asm";
const char* Toolchain::kToolAlink = "alink";
const char* Toolchain::kToolSolink = "solink";
const char* Toolchain::kToolLink = "link";
const char* Toolchain::kToolStamp = "stamp";
const char* Toolchain::kToolCopy = "copy";

Toolchain::Tool::Tool() {
}

Toolchain::Tool::~Tool() {
}

Toolchain::Toolchain(const Label& label) : Item(label) {
}

Toolchain::~Toolchain() {
}

Toolchain* Toolchain::AsToolchain() {
  return this;
}

const Toolchain* Toolchain::AsToolchain() const {
  return this;
}

// static
Toolchain::ToolType Toolchain::ToolNameToType(const base::StringPiece& str) {
  if (str == kToolCc) return TYPE_CC;
  if (str == kToolCxx) return TYPE_CXX;
  if (str == kToolObjC) return TYPE_OBJC;
  if (str == kToolObjCxx) return TYPE_OBJCXX;
  if (str == kToolAsm) return TYPE_ASM;
  if (str == kToolAlink) return TYPE_ALINK;
  if (str == kToolSolink) return TYPE_SOLINK;
  if (str == kToolLink) return TYPE_LINK;
  if (str == kToolStamp) return TYPE_STAMP;
  if (str == kToolCopy) return TYPE_COPY;
  return TYPE_NONE;
}

// static
std::string Toolchain::ToolTypeToName(ToolType type) {
  switch (type) {
    case TYPE_CC: return kToolCc;
    case TYPE_CXX: return kToolCxx;
    case TYPE_OBJC: return kToolObjC;
    case TYPE_OBJCXX: return kToolObjCxx;
    case TYPE_ASM: return kToolAsm;
    case TYPE_ALINK: return kToolAlink;
    case TYPE_SOLINK: return kToolSolink;
    case TYPE_LINK: return kToolLink;
    case TYPE_STAMP: return kToolStamp;
    case TYPE_COPY: return kToolCopy;
    default:
      NOTREACHED();
      return std::string();
  }
}

const Toolchain::Tool& Toolchain::GetTool(ToolType type) const {
  DCHECK(type != TYPE_NONE);
  return tools_[static_cast<size_t>(type)];
}

void Toolchain::SetTool(ToolType type, const Tool& t) {
  DCHECK(type != TYPE_NONE);
  tools_[static_cast<size_t>(type)] = t;
}
