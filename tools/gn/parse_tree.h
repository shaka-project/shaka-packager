// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_PARSE_TREE_H_
#define TOOLS_GN_PARSE_TREE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "tools/gn/err.h"
#include "tools/gn/token.h"
#include "tools/gn/value.h"

class AccessorNode;
class BinaryOpNode;
class BlockNode;
class ConditionNode;
class FunctionCallNode;
class IdentifierNode;
class ListNode;
class LiteralNode;
class Scope;
class UnaryOpNode;

// ParseNode -------------------------------------------------------------------

// A node in the AST.
class ParseNode {
 public:
  ParseNode();
  virtual ~ParseNode();

  virtual const AccessorNode* AsAccessor() const;
  virtual const BinaryOpNode* AsBinaryOp() const;
  virtual const BlockNode* AsBlock() const;
  virtual const ConditionNode* AsConditionNode() const;
  virtual const FunctionCallNode* AsFunctionCall() const;
  virtual const IdentifierNode* AsIdentifier() const;
  virtual const ListNode* AsList() const;
  virtual const LiteralNode* AsLiteral() const;
  virtual const UnaryOpNode* AsUnaryOp() const;

  virtual Value Execute(Scope* scope, Err* err) const = 0;

  virtual LocationRange GetRange() const = 0;

  // Returns an error with the given messages and the range set to something
  // that indicates this node.
  virtual Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const = 0;

  // Prints a representation of this node to the given string, indenting
  // by the given number of spaces.
  virtual void Print(std::ostream& out, int indent) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ParseNode);
};

// AccessorNode ----------------------------------------------------------------

// Access an array element.
//
// If we need to add support for member variables like "variable.len" I was
// thinking this would also handle that case.
class AccessorNode : public ParseNode {
 public:
  AccessorNode();
  virtual ~AccessorNode();

  virtual const AccessorNode* AsAccessor() const OVERRIDE;
  virtual Value Execute(Scope* scope, Err* err) const OVERRIDE;
  virtual LocationRange GetRange() const OVERRIDE;
  virtual Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const OVERRIDE;
  virtual void Print(std::ostream& out, int indent) const OVERRIDE;

  // Base is the thing on the left of the [], currently always required to be
  // an identifier token.
  const Token& base() const { return base_; }
  void set_base(const Token& b) { base_ = b; }

  // Index is the expression inside the [].
  const ParseNode* index() const { return index_.get(); }
  void set_index(scoped_ptr<ParseNode> i) { index_ = i.Pass(); }

 private:
  Token base_;
  scoped_ptr<ParseNode> index_;

  DISALLOW_COPY_AND_ASSIGN(AccessorNode);
};

// BinaryOpNode ----------------------------------------------------------------

class BinaryOpNode : public ParseNode {
 public:
  BinaryOpNode();
  virtual ~BinaryOpNode();

  virtual const BinaryOpNode* AsBinaryOp() const OVERRIDE;
  virtual Value Execute(Scope* scope, Err* err) const OVERRIDE;
  virtual LocationRange GetRange() const OVERRIDE;
  virtual Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const OVERRIDE;
  virtual void Print(std::ostream& out, int indent) const OVERRIDE;

  const Token& op() const { return op_; }
  void set_op(const Token& t) { op_ = t; }

  const ParseNode* left() const { return left_.get(); }
  void set_left(scoped_ptr<ParseNode> left) {
    left_ = left.Pass();
  }

  const ParseNode* right() const { return right_.get(); }
  void set_right(scoped_ptr<ParseNode> right) {
    right_ = right.Pass();
  }

 private:
  scoped_ptr<ParseNode> left_;
  Token op_;
  scoped_ptr<ParseNode> right_;

  DISALLOW_COPY_AND_ASSIGN(BinaryOpNode);
};

// BlockNode -------------------------------------------------------------------

class BlockNode : public ParseNode {
 public:
  // Set has_scope if this block introduces a nested scope.
  BlockNode(bool has_scope);
  virtual ~BlockNode();

  virtual const BlockNode* AsBlock() const OVERRIDE;
  virtual Value Execute(Scope* scope, Err* err) const OVERRIDE;
  virtual LocationRange GetRange() const OVERRIDE;
  virtual Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const OVERRIDE;
  virtual void Print(std::ostream& out, int indent) const OVERRIDE;

  void set_begin_token(const Token* t) { begin_token_ = t; }
  void set_end_token(const Token* t) { end_token_ = t; }

  const std::vector<ParseNode*>& statements() const { return statements_; }
  void append_statement(scoped_ptr<ParseNode> s) {
    statements_.push_back(s.release());
  }

  // Doesn't create a nested scope.
  Value ExecuteBlockInScope(Scope* our_scope, Err* err) const;

 private:
  bool has_scope_;

  // Tokens corresponding to { and }, if any (may be NULL).
  const Token* begin_token_;
  const Token* end_token_;

  // Owning pointers, use unique_ptr when we can use C++11.
  std::vector<ParseNode*> statements_;

  DISALLOW_COPY_AND_ASSIGN(BlockNode);
};

// ConditionNode ---------------------------------------------------------------

class ConditionNode : public ParseNode {
 public:
  ConditionNode();
  virtual ~ConditionNode();

  virtual const ConditionNode* AsConditionNode() const OVERRIDE;
  virtual Value Execute(Scope* scope, Err* err) const OVERRIDE;
  virtual LocationRange GetRange() const OVERRIDE;
  virtual Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const OVERRIDE;
  virtual void Print(std::ostream& out, int indent) const OVERRIDE;

  void set_if_token(const Token& token) { if_token_ = token; }

  const ParseNode* condition() const { return condition_.get(); }
  void set_condition(scoped_ptr<ParseNode> c) {
    condition_ = c.Pass();
  }

  const BlockNode* if_true() const { return if_true_.get(); }
  void set_if_true(scoped_ptr<BlockNode> t) {
    if_true_ = t.Pass();
  }

  // This is either empty, a block (for the else clause), or another
  // condition.
  const ParseNode* if_false() const { return if_false_.get(); }
  void set_if_false(scoped_ptr<ParseNode> f) {
    if_false_ = f.Pass();
  }

 private:
  // Token corresponding to the "if" string.
  Token if_token_;

  scoped_ptr<ParseNode> condition_;  // Always non-null.
  scoped_ptr<BlockNode> if_true_;  // Always non-null.
  scoped_ptr<ParseNode> if_false_;  // May be null.

  DISALLOW_COPY_AND_ASSIGN(ConditionNode);
};

// FunctionCallNode ------------------------------------------------------------

class FunctionCallNode : public ParseNode {
 public:
  FunctionCallNode();
  virtual ~FunctionCallNode();

  virtual const FunctionCallNode* AsFunctionCall() const OVERRIDE;
  virtual Value Execute(Scope* scope, Err* err) const OVERRIDE;
  virtual LocationRange GetRange() const OVERRIDE;
  virtual Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const OVERRIDE;
  virtual void Print(std::ostream& out, int indent) const OVERRIDE;

  const Token& function() const { return function_; }
  void set_function(Token t) { function_ = t; }

  const ListNode* args() const { return args_.get(); }
  void set_args(scoped_ptr<ListNode> a) { args_ = a.Pass(); }

  const BlockNode* block() const { return block_.get(); }
  void set_block(scoped_ptr<BlockNode> b) { block_ = b.Pass(); }

 private:
  Token function_;
  scoped_ptr<ListNode> args_;
  scoped_ptr<BlockNode> block_;  // May be null.

  DISALLOW_COPY_AND_ASSIGN(FunctionCallNode);
};

// IdentifierNode --------------------------------------------------------------

class IdentifierNode : public ParseNode {
 public:
  IdentifierNode();
  IdentifierNode(const Token& token);
  virtual ~IdentifierNode();

  virtual const IdentifierNode* AsIdentifier() const OVERRIDE;
  virtual Value Execute(Scope* scope, Err* err) const OVERRIDE;
  virtual LocationRange GetRange() const OVERRIDE;
  virtual Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const OVERRIDE;
  virtual void Print(std::ostream& out, int indent) const OVERRIDE;

  const Token& value() const { return value_; }
  void set_value(const Token& t) { value_ = t; }

 private:
  Token value_;

  DISALLOW_COPY_AND_ASSIGN(IdentifierNode);
};

// ListNode --------------------------------------------------------------------

class ListNode : public ParseNode {
 public:
  ListNode();
  virtual ~ListNode();

  virtual const ListNode* AsList() const OVERRIDE;
  virtual Value Execute(Scope* scope, Err* err) const OVERRIDE;
  virtual LocationRange GetRange() const OVERRIDE;
  virtual Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const OVERRIDE;
  virtual void Print(std::ostream& out, int indent) const OVERRIDE;

  void set_begin_token(const Token& t) { begin_token_ = t; }
  void set_end_token(const Token& t) { end_token_ = t; }

  void append_item(scoped_ptr<ParseNode> s) {
    contents_.push_back(s.release());
  }
  const std::vector<ParseNode*>& contents() const { return contents_; }

 private:
  // Tokens corresponding to the [ and ].
  Token begin_token_;
  Token end_token_;

  // Owning pointers, use unique_ptr when we can use C++11.
  std::vector<ParseNode*> contents_;

  DISALLOW_COPY_AND_ASSIGN(ListNode);
};

// LiteralNode -----------------------------------------------------------------

class LiteralNode : public ParseNode {
 public:
  LiteralNode();
  LiteralNode(const Token& token);
  virtual ~LiteralNode();

  virtual const LiteralNode* AsLiteral() const OVERRIDE;
  virtual Value Execute(Scope* scope, Err* err) const OVERRIDE;
  virtual LocationRange GetRange() const OVERRIDE;
  virtual Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const OVERRIDE;
  virtual void Print(std::ostream& out, int indent) const OVERRIDE;

  const Token& value() const { return value_; }
  void set_value(const Token& t) { value_ = t; }

 private:
  Token value_;

  DISALLOW_COPY_AND_ASSIGN(LiteralNode);
};

// UnaryOpNode -----------------------------------------------------------------

class UnaryOpNode : public ParseNode {
 public:
  UnaryOpNode();
  virtual ~UnaryOpNode();

  virtual const UnaryOpNode* AsUnaryOp() const OVERRIDE;
  virtual Value Execute(Scope* scope, Err* err) const OVERRIDE;
  virtual LocationRange GetRange() const OVERRIDE;
  virtual Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const OVERRIDE;
  virtual void Print(std::ostream& out, int indent) const OVERRIDE;

  const Token& op() const { return op_; }
  void set_op(const Token& t) { op_ = t; }

  const ParseNode* operand() const { return operand_.get(); }
  void set_operand(scoped_ptr<ParseNode> operand) {
    operand_ = operand.Pass();
  }

 private:
  Token op_;
  scoped_ptr<ParseNode> operand_;

  DISALLOW_COPY_AND_ASSIGN(UnaryOpNode);
};

#endif  // TOOLS_GN_PARSE_TREE_H_
