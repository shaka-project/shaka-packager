// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/parse_tree.h"

#include <string>

#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "tools/gn/functions.h"
#include "tools/gn/operators.h"
#include "tools/gn/scope.h"
#include "tools/gn/string_utils.h"

namespace {

std::string IndentFor(int value) {
  std::string ret;
  for (int i = 0; i < value; i++)
    ret.append(" ");
  return ret;
}

}  // namespace

ParseNode::ParseNode() {
}

ParseNode::~ParseNode() {
}

const AccessorNode* ParseNode::AsAccessor() const { return NULL; }
const BinaryOpNode* ParseNode::AsBinaryOp() const { return NULL; }
const BlockNode* ParseNode::AsBlock() const { return NULL; }
const ConditionNode* ParseNode::AsConditionNode() const { return NULL; }
const FunctionCallNode* ParseNode::AsFunctionCall() const { return NULL; }
const IdentifierNode* ParseNode::AsIdentifier() const { return NULL; }
const ListNode* ParseNode::AsList() const { return NULL; }
const LiteralNode* ParseNode::AsLiteral() const { return NULL; }
const UnaryOpNode* ParseNode::AsUnaryOp() const { return NULL; }

// AccessorNode ---------------------------------------------------------------

AccessorNode::AccessorNode() {
}

AccessorNode::~AccessorNode() {
}

const AccessorNode* AccessorNode::AsAccessor() const {
  return this;
}

Value AccessorNode::Execute(Scope* scope, Err* err) const {
  Value index_value = index_->Execute(scope, err);
  if (err->has_error())
    return Value();
  if (!index_value.VerifyTypeIs(Value::INTEGER, err))
    return Value();

  const Value* base_value = scope->GetValue(base_.value(), true);
  if (!base_value) {
    *err = MakeErrorDescribing("Undefined identifier.");
    return Value();
  }
  if (!base_value->VerifyTypeIs(Value::LIST, err))
    return Value();

  int64 index_int = index_value.int_value();
  if (index_int < 0) {
    *err = Err(index_->GetRange(), "Negative array subscript.",
        "You gave me " + base::Int64ToString(index_int) + ".");
    return Value();
  }
  size_t index_sizet = static_cast<size_t>(index_int);
  if (index_sizet >= base_value->list_value().size()) {
    *err = Err(index_->GetRange(), "Array subscript out of range.",
        "You gave me " + base::Int64ToString(index_int) +
        " but I was expecting something from 0 to " +
        base::Int64ToString(
            static_cast<int64>(base_value->list_value().size()) - 1) +
        ", inclusive.");
    return Value();
  }

  // Doing this assumes that there's no way in the language to do anything
  // between the time the reference is created and the time that the reference
  // is used. If there is, this will crash! Currently, this is just used for
  // array accesses where this "shouldn't" happen.
  return base_value->list_value()[index_sizet];
}

LocationRange AccessorNode::GetRange() const {
  return LocationRange(base_.location(), index_->GetRange().end());
}

Err AccessorNode::MakeErrorDescribing(const std::string& msg,
                                      const std::string& help) const {
  return Err(GetRange(), msg, help);
}

void AccessorNode::Print(std::ostream& out, int indent) const {
  out << IndentFor(indent) << "ACCESSOR\n";
  out << IndentFor(indent + 1) << base_.value() << "\n";
  index_->Print(out, indent + 1);
}

// BinaryOpNode ---------------------------------------------------------------

BinaryOpNode::BinaryOpNode() {
}

BinaryOpNode::~BinaryOpNode() {
}

const BinaryOpNode* BinaryOpNode::AsBinaryOp() const {
  return this;
}

Value BinaryOpNode::Execute(Scope* scope, Err* err) const {
  return ExecuteBinaryOperator(scope, this, left_.get(), right_.get(), err);
}

LocationRange BinaryOpNode::GetRange() const {
  return left_->GetRange().Union(right_->GetRange());
}

Err BinaryOpNode::MakeErrorDescribing(const std::string& msg,
                                      const std::string& help) const {
  return Err(op_, msg, help);
}

void BinaryOpNode::Print(std::ostream& out, int indent) const {
  out << IndentFor(indent) << "BINARY(" << op_.value() << ")\n";
  left_->Print(out, indent + 1);
  right_->Print(out, indent + 1);
}

// BlockNode ------------------------------------------------------------------

BlockNode::BlockNode(bool has_scope)
    : has_scope_(has_scope),
      begin_token_(NULL),
      end_token_(NULL) {
}

BlockNode::~BlockNode() {
  STLDeleteContainerPointers(statements_.begin(), statements_.end());
}

const BlockNode* BlockNode::AsBlock() const {
  return this;
}

Value BlockNode::Execute(Scope* containing_scope, Err* err) const {
  if (has_scope_) {
    Scope our_scope(containing_scope);
    Value ret = ExecuteBlockInScope(&our_scope, err);
    if (err->has_error())
      return Value();

    // Check for unused vars in the scope.
    //our_scope.CheckForUnusedVars(err);
    return ret;
  }
  return ExecuteBlockInScope(containing_scope, err);
}

LocationRange BlockNode::GetRange() const {
  if (begin_token_ && end_token_) {
    return begin_token_->range().Union(end_token_->range());
  }
  return LocationRange();  // TODO(brettw) indicate the entire file somehow.
}

Err BlockNode::MakeErrorDescribing(const std::string& msg,
                                   const std::string& help) const {
  if (begin_token_)
    return Err(*begin_token_, msg, help);
  // TODO(brettw) this should have the beginning of the file in it or something.
  return Err(Location(NULL, 1, 1), msg, help);
}

void BlockNode::Print(std::ostream& out, int indent) const {
  out << IndentFor(indent) << "BLOCK\n";
  for (size_t i = 0; i < statements_.size(); i++)
    statements_[i]->Print(out, indent + 1);
}

Value BlockNode::ExecuteBlockInScope(Scope* our_scope, Err* err) const {
  for (size_t i = 0; i < statements_.size() && !err->has_error(); i++) {
    // Check for trying to execute things with no side effects in a block.
    const ParseNode* cur = statements_[i];
    if (cur->AsList() || cur->AsLiteral() || cur->AsUnaryOp() ||
        cur->AsIdentifier()) {
      *err = cur->MakeErrorDescribing(
          "This statment has no effect.",
          "Either delete it or do something with the result.");
      return Value();
    }
    cur->Execute(our_scope, err);
  }
  return Value();
}

// ConditionNode --------------------------------------------------------------

ConditionNode::ConditionNode() {
}

ConditionNode::~ConditionNode() {
}

const ConditionNode* ConditionNode::AsConditionNode() const {
  return this;
}

Value ConditionNode::Execute(Scope* scope, Err* err) const {
  Value condition_result = condition_->Execute(scope, err);
  if (err->has_error())
    return Value();
  if (condition_result.type() == Value::NONE) {
    *err = condition_->MakeErrorDescribing(
        "This does not evaluate to a value.",
        "Please give me something to work with for the if statement.");
    err->AppendRange(if_token_.range());
    return Value();
  }

  if (condition_result.InterpretAsInt()) {
    if_true_->ExecuteBlockInScope(scope, err);
  } else if (if_false_) {
    // The else block is optional. It's either another condition (for an
    // "else if" and we can just Execute it and the condition will handle
    // the scoping) or it's a block indicating an "else" in which ase we
    // need to be sure it inherits our scope.
    const BlockNode* if_false_block = if_false_->AsBlock();
    if (if_false_block)
      if_false_block->ExecuteBlockInScope(scope, err);
    else
      if_false_->Execute(scope, err);
  }

  return Value();
}

LocationRange ConditionNode::GetRange() const {
  if (if_false_)
    return if_token_.range().Union(if_false_->GetRange());
  return if_token_.range().Union(if_true_->GetRange());
}

Err ConditionNode::MakeErrorDescribing(const std::string& msg,
                                       const std::string& help) const {
  return Err(if_token_, msg, help);
}

void ConditionNode::Print(std::ostream& out, int indent) const {
  out << IndentFor(indent) << "CONDITION\n";
  condition_->Print(out, indent + 1);
  if_true_->Print(out, indent + 1);
  if (if_false_)
    if_false_->Print(out, indent + 1);
}

// FunctionCallNode -----------------------------------------------------------

FunctionCallNode::FunctionCallNode() {
}

FunctionCallNode::~FunctionCallNode() {
}

const FunctionCallNode* FunctionCallNode::AsFunctionCall() const {
  return this;
}

Value FunctionCallNode::Execute(Scope* scope, Err* err) const {
  Value args = args_->Execute(scope, err);
  if (err->has_error())
    return Value();
  return functions::RunFunction(scope, this, args.list_value(), block_.get(),
                                err);
}

LocationRange FunctionCallNode::GetRange() const {
  if (block_)
    return function_.range().Union(block_->GetRange());
  return function_.range().Union(args_->GetRange());
}

Err FunctionCallNode::MakeErrorDescribing(const std::string& msg,
                                          const std::string& help) const {
  return Err(function_, msg, help);
}

void FunctionCallNode::Print(std::ostream& out, int indent) const {
  out << IndentFor(indent) << "FUNCTION(" << function_.value() << ")\n";
  args_->Print(out, indent + 1);
  if (block_)
    block_->Print(out, indent + 1);
}

// IdentifierNode --------------------------------------------------------------

IdentifierNode::IdentifierNode() {
}

IdentifierNode::IdentifierNode(const Token& token) : value_(token) {
}

IdentifierNode::~IdentifierNode() {
}

const IdentifierNode* IdentifierNode::AsIdentifier() const {
  return this;
}

Value IdentifierNode::Execute(Scope* scope, Err* err) const {
  const Value* result = scope->GetValue(value_.value(), true);
  if (!result) {
    *err = MakeErrorDescribing("Undefined identifier");
    return Value();
  }
  return *result;
}

LocationRange IdentifierNode::GetRange() const {
  return value_.range();
}

Err IdentifierNode::MakeErrorDescribing(const std::string& msg,
                                        const std::string& help) const {
  return Err(value_, msg, help);
}

void IdentifierNode::Print(std::ostream& out, int indent) const {
  out << IndentFor(indent) << "IDENTIFIER(" << value_.value() << ")\n";
}

// ListNode -------------------------------------------------------------------

ListNode::ListNode() {
}

ListNode::~ListNode() {
  STLDeleteContainerPointers(contents_.begin(), contents_.end());
}

const ListNode* ListNode::AsList() const {
  return this;
}

Value ListNode::Execute(Scope* scope, Err* err) const {
  Value result_value(this, Value::LIST);
  std::vector<Value>& results = result_value.list_value();
  results.resize(contents_.size());

  for (size_t i = 0; i < contents_.size(); i++) {
    const ParseNode* cur = contents_[i];
    results[i] = cur->Execute(scope, err);
    if (err->has_error())
      return Value();
    if (results[i].type() == Value::NONE) {
      *err = cur->MakeErrorDescribing(
          "This does not evaluate to a value.",
          "I can't do something with nothing.");
      return Value();
    }
  }
  return result_value;
}

LocationRange ListNode::GetRange() const {
  return LocationRange(begin_token_.location(), end_token_.location());
}

Err ListNode::MakeErrorDescribing(const std::string& msg,
                                  const std::string& help) const {
  return Err(begin_token_, msg, help);
}

void ListNode::Print(std::ostream& out, int indent) const {
  out << IndentFor(indent) << "LIST\n";
  for (size_t i = 0; i < contents_.size(); i++)
    contents_[i]->Print(out, indent + 1);
}

// LiteralNode -----------------------------------------------------------------

LiteralNode::LiteralNode() {
}

LiteralNode::LiteralNode(const Token& token) : value_(token) {
}

LiteralNode::~LiteralNode() {
}

const LiteralNode* LiteralNode::AsLiteral() const {
  return this;
}

Value LiteralNode::Execute(Scope* scope, Err* err) const {
  switch (value_.type()) {
    case Token::INTEGER: {
      int64 result_int;
      if (!base::StringToInt64(value_.value(), &result_int)) {
        *err = MakeErrorDescribing("This does not look like an integer");
        return Value();
      }
      return Value(this, result_int);
    }
    case Token::STRING: {
      // TODO(brettw) Unescaping probably needs to be moved & improved.
      // The input value includes the quotes around the string, strip those
      // off and unescape.
      Value v(this, Value::STRING);
      ExpandStringLiteral(scope, value_, &v, err);
      return v;
    }
    default:
      NOTREACHED();
      return Value();
  }
}

LocationRange LiteralNode::GetRange() const {
  return value_.range();
}

Err LiteralNode::MakeErrorDescribing(const std::string& msg,
                                     const std::string& help) const {
  return Err(value_, msg, help);
}

void LiteralNode::Print(std::ostream& out, int indent) const {
  out << IndentFor(indent) << "LITERAL(" << value_.value() << ")\n";
}

// UnaryOpNode ----------------------------------------------------------------

UnaryOpNode::UnaryOpNode() {
}

UnaryOpNode::~UnaryOpNode() {
}

const UnaryOpNode* UnaryOpNode::AsUnaryOp() const {
  return this;
}

Value UnaryOpNode::Execute(Scope* scope, Err* err) const {
  Value operand_value = operand_->Execute(scope, err);
  if (err->has_error())
    return Value();
  return ExecuteUnaryOperator(scope, this, operand_value, err);
}

LocationRange UnaryOpNode::GetRange() const {
  return op_.range().Union(operand_->GetRange());
}

Err UnaryOpNode::MakeErrorDescribing(const std::string& msg,
                                     const std::string& help) const {
  return Err(op_, msg, help);
}

void UnaryOpNode::Print(std::ostream& out, int indent) const {
  out << IndentFor(indent) << "UNARY(" << op_.value() << ")\n";
  operand_->Print(out, indent + 1);
}
