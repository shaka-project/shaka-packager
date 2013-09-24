# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This Makefile requires the LLVM build system. In order to build this tool,
# please run tools/clang/scripts/build_tool.py.

CLANG_LEVEL := ../..

TOOLNAME = rewrite_scoped_ptr_ctor_null

NO_INSTALL = 1

include $(CLANG_LEVEL)/../../Makefile.config

LINK_COMPONENTS := $(TARGETS_TO_BUILD) asmparser bitreader support mc
USEDLIBS = clangTooling.a clangFrontend.a clangSerialization.a clangDriver.a \
           clangRewriteFrontend.a clangRewriteCore.a clangParse.a clangSema.a \
           clangAnalysis.a clangAST.a clangASTMatchers.a clangEdit.a \
           clangLex.a clangBasic.a

include $(CLANG_LEVEL)/Makefile

