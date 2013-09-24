// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_PARSER_H_
#define TOOLS_GN_PARSER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "tools/gn/err.h"
#include "tools/gn/parse_tree.h"

// Parses a series of tokens. The resulting AST will refer to the tokens passed
// to the input, so the tokens an the file data they refer to must outlive your
// use of the ParseNode.
class Parser {
 public:
  // Will return a null pointer and set the err on error.
  static scoped_ptr<ParseNode> Parse(const std::vector<Token>& tokens,
                                     Err* err);

  // Alternative to parsing that assumes the input is an expression.
  static scoped_ptr<ParseNode> ParseExpression(const std::vector<Token>& tokens,
                                               Err* err);

 private:
  // Vector must be valid for lifetime of call.
  Parser(const std::vector<Token>& tokens, Err* err);
  ~Parser();

  scoped_ptr<AccessorNode> ParseAccessor();
  scoped_ptr<BlockNode> ParseBlock(bool need_braces);
  scoped_ptr<ConditionNode> ParseCondition();
  scoped_ptr<ParseNode> ParseExpression();
  scoped_ptr<ParseNode> ParseExpressionExceptBinaryOperators();
  scoped_ptr<FunctionCallNode> ParseFunctionCall();
  scoped_ptr<ListNode> ParseList(const Token& expected_begin,
                                 const Token& expected_end);
  scoped_ptr<ParseNode> ParseParenExpression();
  scoped_ptr<UnaryOpNode> ParseUnaryOp();

  bool IsToken(Token::Type type, char* str) const;

  // Gets an error corresponding to the last token. When we hit an EOF
  // usually we've already gone beyond the end (or maybe there are no tokens)
  // so there is some tricky logic to report this.
  Err MakeEOFError(const std::string& message,
                   const std::string& help = std::string()) const;

  const Token& cur_token() const { return tokens_[cur_]; }

  bool done() const { return at_end() || has_error(); }
  bool at_end() const { return cur_ >= tokens_.size(); }
  bool has_error() const { return err_->has_error(); }

  const Token& next_token() const { return tokens_[cur_ + 1]; }
  bool has_next_token() const { return cur_ + 1 < tokens_.size(); }

  const std::vector<Token>& tokens_;

  Err* err_;

  // Current index into the tokens.
  size_t cur_;

  FRIEND_TEST_ALL_PREFIXES(Parser, BinaryOp);
  FRIEND_TEST_ALL_PREFIXES(Parser, Block);
  FRIEND_TEST_ALL_PREFIXES(Parser, Condition);
  FRIEND_TEST_ALL_PREFIXES(Parser, Expression);
  FRIEND_TEST_ALL_PREFIXES(Parser, FunctionCall);
  FRIEND_TEST_ALL_PREFIXES(Parser, List);
  FRIEND_TEST_ALL_PREFIXES(Parser, ParenExpression);
  FRIEND_TEST_ALL_PREFIXES(Parser, UnaryOp);

  DISALLOW_COPY_AND_ASSIGN(Parser);
};

#endif  // TOOLS_GN_PARSER_H_
