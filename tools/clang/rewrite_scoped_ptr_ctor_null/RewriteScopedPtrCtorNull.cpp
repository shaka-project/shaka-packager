// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This implements a Clang tool to convert all instances of std::string("") to
// std::string(). The latter is more efficient (as std::string doesn't have to
// take a copy of an empty string) and generates fewer instructions as well. It
// should be run using the tools/clang/scripts/run_tool.py helper.

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

using clang::ast_matchers::MatchFinder;
using clang::ast_matchers::argumentCountIs;
using clang::ast_matchers::bindTemporaryExpr;
using clang::ast_matchers::constructorDecl;
using clang::ast_matchers::constructExpr;
using clang::ast_matchers::defaultArgExpr;
using clang::ast_matchers::expr;
using clang::ast_matchers::forEach;
using clang::ast_matchers::has;
using clang::ast_matchers::hasArgument;
using clang::ast_matchers::hasDeclaration;
using clang::ast_matchers::matchesName;
using clang::ast_matchers::id;
using clang::ast_matchers::methodDecl;
using clang::ast_matchers::newExpr;
using clang::ast_matchers::ofClass;
using clang::ast_matchers::unless;
using clang::ast_matchers::varDecl;
using clang::tooling::CommonOptionsParser;
using clang::tooling::Replacement;
using clang::tooling::Replacements;

namespace {

bool IsNullConstant(const clang::Expr& expr, clang::ASTContext* context) {
  return expr.isNullPointerConstant(*context,
                                    clang::Expr::NPC_ValueDependentIsNotNull) !=
         clang::Expr::NPCK_NotNull;
}

// Handles replacements for stack and heap-allocated instances, e.g.:
// scoped_ptr<T> a(NULL);
// scoped_ptr<T>* b = new scoped_ptr<T>(NULL);
// ...though the latter should be pretty rare.
class ConstructorCallback : public MatchFinder::MatchCallback {
 public:
  ConstructorCallback(Replacements* replacements)
      : replacements_(replacements) {}

  virtual void run(const MatchFinder::MatchResult& result) LLVM_OVERRIDE;

 private:
  Replacements* const replacements_;
};

// Handles replacements for invocations of scoped_ptr<T>(NULL) in an initializer
// list.
class InitializerCallback : public MatchFinder::MatchCallback {
 public:
  InitializerCallback(Replacements* replacements)
      : replacements_(replacements) {}

  virtual void run(const MatchFinder::MatchResult& result) LLVM_OVERRIDE;

 private:
  Replacements* const replacements_;
};

// Handles replacements for invocations of scoped_ptr<T>(NULL) in a temporary
// context, e.g. return scoped_ptr<T>(NULL).
class TemporaryCallback : public MatchFinder::MatchCallback {
 public:
  TemporaryCallback(Replacements* replacements) : replacements_(replacements) {}

  virtual void run(const MatchFinder::MatchResult& result) LLVM_OVERRIDE;

 private:
  Replacements* const replacements_;
};

class EmptyStringConverter {
 public:
  explicit EmptyStringConverter(Replacements* replacements)
      : constructor_callback_(replacements),
        initializer_callback_(replacements),
        temporary_callback_(replacements) {}

  void SetupMatchers(MatchFinder* match_finder);

 private:
  ConstructorCallback constructor_callback_;
  InitializerCallback initializer_callback_;
  TemporaryCallback temporary_callback_;
};

void EmptyStringConverter::SetupMatchers(MatchFinder* match_finder) {
  const char kPattern[] = "^::(scoped_ptr|scoped_ptr_malloc)$";
  const clang::ast_matchers::StatementMatcher& constructor_call = id(
      "call",
      constructExpr(hasDeclaration(methodDecl(ofClass(matchesName(kPattern)))),
                    argumentCountIs(1),
                    hasArgument(0, id("arg", expr())),
                    unless(hasArgument(0, defaultArgExpr()))));

  match_finder->addMatcher(varDecl(forEach(constructor_call)),
                           &constructor_callback_);
  match_finder->addMatcher(newExpr(has(constructor_call)),
                           &constructor_callback_);
  match_finder->addMatcher(bindTemporaryExpr(has(constructor_call)),
                           &temporary_callback_);
  match_finder->addMatcher(constructorDecl(forEach(constructor_call)),
                           &initializer_callback_);
}

void ConstructorCallback::run(const MatchFinder::MatchResult& result) {
  const clang::Expr* arg = result.Nodes.getNodeAs<clang::Expr>("arg");
  if (!IsNullConstant(*arg, result.Context))
    return;

  const clang::CXXConstructExpr* call =
      result.Nodes.getNodeAs<clang::CXXConstructExpr>("call");
  clang::CharSourceRange range =
      clang::CharSourceRange::getTokenRange(call->getParenRange());
  replacements_->insert(Replacement(*result.SourceManager, range, ""));
}

void InitializerCallback::run(const MatchFinder::MatchResult& result) {
  const clang::Expr* arg = result.Nodes.getNodeAs<clang::Expr>("arg");
  if (!IsNullConstant(*arg, result.Context))
    return;

  const clang::CXXConstructExpr* call =
      result.Nodes.getNodeAs<clang::CXXConstructExpr>("call");
  replacements_->insert(Replacement(*result.SourceManager, call, ""));
}

void TemporaryCallback::run(const MatchFinder::MatchResult& result) {
  const clang::Expr* arg = result.Nodes.getNodeAs<clang::Expr>("arg");
  if (!IsNullConstant(*arg, result.Context))
    return;

  // TODO(dcheng): File a bug with clang. There should be an easier way to do
  // this replacement, but getTokenRange(call->getParenRange()) and the obvious
  // (but incorrect) arg both don't work. The former is presumably just buggy,
  // while the latter probably has to do with the fact that NULL is actually a
  // macro which expands to a built-in.
  clang::SourceRange range = arg->getSourceRange();
  clang::SourceRange expansion_range(
      result.SourceManager->getExpansionLoc(range.getBegin()),
      result.SourceManager->getExpansionLoc(range.getEnd()));
  replacements_->insert(
      Replacement(*result.SourceManager,
                  clang::CharSourceRange::getTokenRange(expansion_range),
                  ""));
}

}  // namespace

static llvm::cl::extrahelp common_help(CommonOptionsParser::HelpMessage);

int main(int argc, const char* argv[]) {
  CommonOptionsParser options(argc, argv);
  clang::tooling::ClangTool tool(options.getCompilations(),
                                 options.getSourcePathList());

  Replacements replacements;
  EmptyStringConverter converter(&replacements);
  MatchFinder match_finder;
  converter.SetupMatchers(&match_finder);

  int result =
      tool.run(clang::tooling::newFrontendActionFactory(&match_finder));
  if (result != 0)
    return result;

  // Each replacement line should have the following format:
  // r:<file path>:<offset>:<length>:<replacement text>
  // Only the <replacement text> field can contain embedded ":" characters.
  // TODO(dcheng): Use a more clever serialization.
  llvm::outs() << "==== BEGIN EDITS ====\n";
  for (Replacements::const_iterator it = replacements.begin();
       it != replacements.end();
       ++it) {
    llvm::outs() << "r:" << it->getFilePath() << ":" << it->getOffset() << ":"
                 << it->getLength() << ":" << it->getReplacementText() << "\n";
  }
  llvm::outs() << "==== END EDITS ====\n";

  return 0;
}
