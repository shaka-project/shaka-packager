// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This implements a Clang tool to rewrite all instances of scoped_array<T> to
// scoped_ptr<T[]>. The former is being deprecated in favor of the latter, to
// allow for an eventual transition from scoped_ptr to unique_ptr.

#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

using clang::ast_matchers::MatchFinder;
using clang::ast_matchers::hasDeclaration;
using clang::ast_matchers::hasName;
using clang::ast_matchers::id;
using clang::ast_matchers::loc;
using clang::ast_matchers::qualType;
using clang::ast_matchers::recordDecl;
using clang::tooling::CommonOptionsParser;
using clang::tooling::Replacement;
using clang::tooling::Replacements;
using llvm::StringRef;

namespace {

class RewriterCallback : public MatchFinder::MatchCallback {
 public:
  RewriterCallback(Replacements* replacements) : replacements_(replacements) {}
  virtual void run(const MatchFinder::MatchResult& result) LLVM_OVERRIDE;

 private:
  Replacements* const replacements_;
};

void RewriterCallback::run(const MatchFinder::MatchResult& result) {
  const clang::TypeLoc type_location =
      *result.Nodes.getNodeAs<clang::TypeLoc>("loc");
  clang::CharSourceRange range = clang::CharSourceRange::getTokenRange(
      result.SourceManager->getSpellingLoc(type_location.getLocStart()),
      result.SourceManager->getSpellingLoc(type_location.getLocEnd()));
  // TODO(dcheng): Log an error?
  if (!range.isValid())
    return;
  std::string replacement_text = clang::Lexer::getSourceText(
      range, *result.SourceManager, result.Context->getLangOpts());
  // TODO(dcheng): Log errors?
  if (!StringRef(replacement_text).startswith("scoped_array<") ||
      !StringRef(replacement_text).endswith(">"))
    return;
  replacement_text.replace(strlen("scoped_"), strlen("array"), "ptr");
  replacement_text.insert(replacement_text.size() - 1, "[]");
  replacements_->insert(
      Replacement(*result.SourceManager, range, replacement_text));
}

}  // namespace

static llvm::cl::extrahelp common_help(CommonOptionsParser::HelpMessage);

int main(int argc, const char* argv[]) {
  CommonOptionsParser options(argc, argv);
  clang::tooling::ClangTool tool(options.getCompilations(),
                                 options.getSourcePathList());

  Replacements replacements;
  RewriterCallback callback(&replacements);
  MatchFinder match_finder;
  match_finder.addMatcher(
      id("loc",
         loc(qualType(hasDeclaration(recordDecl(hasName("::scoped_array")))))),
      &callback);

  int result =
      tool.run(clang::tooling::newFrontendActionFactory(&match_finder));
  if (result != 0)
    return result;

  // Serialization format is documented in tools/clang/scripts/run_tool.py
  llvm::outs() << "==== BEGIN EDITS ====\n";
  for (Replacements::const_iterator it = replacements.begin();
       it != replacements.end(); ++it) {
    llvm::outs() << "r:" << it->getFilePath() << ":" << it->getOffset() << ":"
                 << it->getLength() << ":" << it->getReplacementText() << "\n";
  }
  llvm::outs() << "==== END EDITS ====\n";

  return 0;
}
