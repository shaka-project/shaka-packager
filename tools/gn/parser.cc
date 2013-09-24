// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/parser.h"

#include "base/logging.h"
#include "tools/gn/functions.h"
#include "tools/gn/operators.h"
#include "tools/gn/token.h"

namespace {

// Returns true if the two tokens are on the same line. We assume they're in
// the same file.
bool IsSameLine(const Token& a, const Token& b) {
  DCHECK(a.location().file() == b.location().file());
  return a.location().line_number() == b.location().line_number();
}

}  // namespace

Parser::Parser(const std::vector<Token>& tokens, Err* err)
    : tokens_(tokens),
      err_(err),
      cur_(0) {
}

Parser::~Parser() {
}

// static
scoped_ptr<ParseNode> Parser::Parse(const std::vector<Token>& tokens,
                                    Err* err) {
  Parser p(tokens, err);
  return p.ParseBlock(false).PassAs<ParseNode>();
}

// static
scoped_ptr<ParseNode> Parser::ParseExpression(const std::vector<Token>& tokens,
                                              Err* err) {
  Parser p(tokens, err);
  return p.ParseExpression().Pass();
}

bool Parser::IsToken(Token::Type type, char* str) const {
  if (at_end())
    return false;
  return cur_token().type() == type || cur_token().value() == str;
}

scoped_ptr<AccessorNode> Parser::ParseAccessor() {
  scoped_ptr<AccessorNode> accessor(new AccessorNode);

  DCHECK(cur_token().type() == Token::IDENTIFIER);
  accessor->set_base(cur_token());
  cur_++;  // Skip identifier.
  cur_++;  // Skip "[" (we know this exists because the existance of this
           // token is how the caller knows it's an accessor.

  if (at_end()) {
    *err_ = MakeEOFError("Got EOF when looking for list index.");
    return scoped_ptr<AccessorNode>();
  }

  // Get the expression.
  scoped_ptr<ParseNode> expr = ParseExpression().Pass();
  if (has_error())
    return scoped_ptr<AccessorNode>();
  if (at_end()) {
    *err_ = MakeEOFError("Got EOF when looking for list accessor ]");
    return scoped_ptr<AccessorNode>();
  }
  accessor->set_index(expr.Pass());

  // Skip over "]"
  if (!cur_token().IsScoperEqualTo("]")) {
    *err_ = Err(cur_token(), "Expecting ]",
               "You started a list access but didn't terminate it, and instead "
               "I fould this\nstupid thing.");
    return scoped_ptr<AccessorNode>();
  }
  cur_++;

  return accessor.Pass();
}

// Blocks at the file scope don't need {} so we have the option to ignore
// them. When need_braces is set, we'll expect a begin an end brace.
//
// block := "{" block_contents "}"
// block_contents := (expression | conditional | block)*
scoped_ptr<BlockNode> Parser::ParseBlock(bool need_braces) {
  scoped_ptr<BlockNode> block(new BlockNode(true));

  // Eat initial { if necessary.
  const Token* opening_curly_brace;
  if (need_braces) {
    if (at_end()) {
      *err_ = MakeEOFError("Got EOF when looking for { for block.",
                           "It should have been after here.");
      return scoped_ptr<BlockNode>();
    } else if(!IsScopeBeginScoper(cur_token())) {
      *err_ = Err(cur_token(), "Expecting { instead of this thing.",
                  "THOU SHALT USE CURLY BRACES FOR ALL BLOCKS.");
      return scoped_ptr<BlockNode>();
    }
    opening_curly_brace = &cur_token();
    block->set_begin_token(opening_curly_brace);
    cur_++;
  }

  // Loop until EOF or end brace found.
  while (!at_end() && !IsScopeEndScoper(cur_token())) {
    if (cur_token().IsIdentifierEqualTo("if")) {
      // Conditional.
      block->append_statement(ParseCondition().PassAs<ParseNode>());
    } else if (IsScopeBeginScoper(cur_token())) {
      // Nested block.
      block->append_statement(ParseBlock(true).PassAs<ParseNode>());
    } else {
      // Everything else is an expression.
      block->append_statement(ParseExpression().PassAs<ParseNode>());
    }
    if (has_error())
      return scoped_ptr<BlockNode>();
  }

  // Eat the ending "}" if necessary.
  if (need_braces) {
    if (at_end() || !IsScopeEndScoper(cur_token())) {
      *err_ = Err(*opening_curly_brace, "Expecting }",
                  "I ran headlong into the end of the file looking for the "
                  "closing brace\ncorresponding to this one.");
      return scoped_ptr<BlockNode>();
    }
    block->set_end_token(&cur_token());
    cur_++;  // Skip past "}".
  }

  return block.Pass();
}

// conditional := "if (" expression ")" block [else_conditional]
// else_conditional := ("else" block) | ("else" conditional)
scoped_ptr<ConditionNode> Parser::ParseCondition() {
  scoped_ptr<ConditionNode> cond(new ConditionNode);

  // Skip past "if".
  const Token& if_token = cur_token();
  cond->set_if_token(if_token);
  DCHECK(if_token.IsIdentifierEqualTo("if"));
  cur_++;

  if (at_end() || !IsFunctionCallArgBeginScoper(cur_token())) {
    *err_ = Err(if_token, "Expecting \"(\" after \"if\"",
                "Did you think this was Python or something?");
    return scoped_ptr<ConditionNode>();
  }

  // Skip over (.
  const Token& open_paren_token = cur_token();
  cur_++;
  if (at_end()) {
    *err_ = Err(if_token, "Unexpected EOF inside if condition");
    return scoped_ptr<ConditionNode>();
  }

  // Condition inside ().
  cond->set_condition(ParseExpression().Pass());
  if (has_error())
    return scoped_ptr<ConditionNode>();

  if (at_end() || !IsFunctionCallArgEndScoper(cur_token())) {
    *err_ = Err(open_paren_token, "Expecting \")\" for \"if\" condition",
                "You didn't finish the thought you started here.");
    return scoped_ptr<ConditionNode>();
  }
  cur_++;  // Skip over )

  // Contents of {}.
  cond->set_if_true(ParseBlock(true).Pass());
  if (has_error())
    return scoped_ptr<ConditionNode>();

  // Optional "else" at the end.
  if (!at_end() && cur_token().IsIdentifierEqualTo("else")) {
    cur_++;

    // The else may be followed by an if or a block.
    if (at_end()) {
      *err_ = MakeEOFError("Ran into end of file after \"else\".",
                           "else, WHAT?!?!?");
      return scoped_ptr<ConditionNode>();
    }
    if (cur_token().IsIdentifierEqualTo("if")) {
      // "else if() {"
      cond->set_if_false(ParseCondition().PassAs<ParseNode>());
    } else if (IsScopeBeginScoper(cur_token())) {
      // "else {"
      cond->set_if_false(ParseBlock(true).PassAs<ParseNode>());
    } else {
      // else <anything else>
      *err_ = Err(cur_token(), "Expected \"if\" or \"{\" after \"else\".",
                  "This is neither of those things.");
      return scoped_ptr<ConditionNode>();
    }
  }

  if (has_error())
    return scoped_ptr<ConditionNode>();
  return cond.Pass();
}

// expression := paren_expression | accessor | identifier | literal |
//               funccall | unary_expression | binary_expression
//
// accessor := identifier <non-newline-whitespace>* "[" expression "]"
//
// The "non-newline-whitespace is used to differentiate between this case:
//   a[1]
// and this one:
//   a
//   [1]
// The second one is kind of stupid (since it does nothing with the values)
// but is still legal.
scoped_ptr<ParseNode> Parser::ParseExpression() {
  scoped_ptr<ParseNode> expr = ParseExpressionExceptBinaryOperators();
  if (has_error())
    return scoped_ptr<ParseNode>();

  // That may have hit EOF, in which case we can't have any binary operators.
  if (at_end())
    return expr.Pass();

  // TODO(brettw) handle operator precidence!
  // Gobble up all subsequent expressions as long as there are binary
  // operators.

  if (IsBinaryOperator(cur_token())) {
    scoped_ptr<BinaryOpNode> binary_op(new BinaryOpNode);
    binary_op->set_left(expr.Pass());
    const Token& operator_token = cur_token();
    binary_op->set_op(operator_token);
    cur_++;
    if (at_end()) {
      *err_ = Err(operator_token, "Unexpected EOF in expression.",
                  "I was looking for the right-hand-side of this operator.");
      return scoped_ptr<ParseNode>();
    }
    binary_op->set_right(ParseExpression().Pass());
    if (has_error())
      return scoped_ptr<ParseNode>();
    return binary_op.PassAs<ParseNode>();
  }

  return expr.Pass();
}


// This internal one does not handle binary operators, since it requires
// looking at the "next" thing. The regular ParseExpression above handles it.
scoped_ptr<ParseNode> Parser::ParseExpressionExceptBinaryOperators() {
  if (at_end())
    return scoped_ptr<ParseNode>();

  const Token& token = cur_token();

  // Unary expression.
  if (IsUnaryOperator(token))
    return ParseUnaryOp().PassAs<ParseNode>();

  // Parenthesized expressions.
  if (token.IsScoperEqualTo("("))
    return ParseParenExpression();

  // Function calls.
  if (token.type() == Token::IDENTIFIER) {
    if (has_next_token() && IsFunctionCallArgBeginScoper(next_token()))
      return ParseFunctionCall().PassAs<ParseNode>();
  }

  // Lists.
  if (token.IsScoperEqualTo("[")) {
    return ParseList(Token(Location(), Token::SCOPER, "["),
                     Token(Location(), Token::SCOPER, "]")).PassAs<ParseNode>();
  }

  // Literals.
  if (token.type() == Token::STRING || token.type() == Token::INTEGER) {
    cur_++;
    return scoped_ptr<ParseNode>(new LiteralNode(token));
  }

  // Accessors.
  if (token.type() == Token::IDENTIFIER &&
      has_next_token() && next_token().IsScoperEqualTo("[") &&
      IsSameLine(token, next_token())) {
    return ParseAccessor().PassAs<ParseNode>();
  }

  // Identifiers.
  if (token.type() == Token::IDENTIFIER) {
    cur_++;
    return scoped_ptr<ParseNode>(new IdentifierNode(token));
  }

  // Handle errors.
  if (token.type() == Token::SEPARATOR) {
    *err_ = Err(token, "Unexpected comma.",
                "You can't put a comma here, it must be in list separating "
                "complete\nthoughts.");
  } else if (IsScopeBeginScoper(token)) {
    *err_ = Err(token, "Unexpected token.",
                "You can't put a \"{\" scope here, it must be in a block.");
  } else {
    *err_ = Err(token, "Unexpected token.",
                "I was really hoping for something else here and you let me down.");
  }
  return scoped_ptr<ParseNode>();
}

// function_call := identifier "(" list_contents ")"
//                  [<non-newline-whitespace>* block]
scoped_ptr<FunctionCallNode> Parser::ParseFunctionCall() {
  scoped_ptr<FunctionCallNode> func(new FunctionCallNode);

  const Token& function_token = cur_token();
  func->set_function(function_token);

  // This function should only get called when we know we have a function,
  // which only happens when there is a paren following the name. Skip past it.
  DCHECK(has_next_token());
  cur_++;  // Skip past function name to (.
  const Token& open_paren_token = cur_token();
  DCHECK(IsFunctionCallArgBeginScoper(open_paren_token));

  if (at_end()) {
    *err_ = Err(open_paren_token, "Unexpected EOF for function call.",
                "You didn't finish the thought you started here.");
    return scoped_ptr<FunctionCallNode>();
  }

  // Arguments.
  func->set_args(ParseList(Token(Location(), Token::SCOPER, "("),
                           Token(Location(), Token::SCOPER, ")")));
  if (has_error())
    return scoped_ptr<FunctionCallNode>();

  // Optional {} after function call for certain functions. The "{" must be on
  // the same line as the ")" to disambiguate the case of a function followed
  // by a random block just used for scoping purposes.
  if (!at_end() && IsScopeBeginScoper(cur_token())) {
    const Token& args_end_token = tokens_[cur_ - 1];
    DCHECK(args_end_token.IsScoperEqualTo(")"));
    if (IsSameLine(args_end_token, cur_token()))
      func->set_block(ParseBlock(true).Pass());
  }

  if (has_error())
    return scoped_ptr<FunctionCallNode>();
  return func.Pass();
}

// list := "[" expression* "]"
// list_contents := [(expression ",")* expression [","]]
//
// The list_contents is also used in function calls surrounded by parens, so
// this function takes the tokens that are expected to surround the list.
scoped_ptr<ListNode> Parser::ParseList(const Token& expected_begin,
                                       const Token& expected_end) {
  scoped_ptr<ListNode> list(new ListNode);

  const Token& open_bracket_token = cur_token();
  list->set_begin_token(open_bracket_token);
  cur_++;  // Skip "[" or "(".

  bool need_separator = false;
  while(true) {
    if (at_end()) {
      *err_ = Err(open_bracket_token, "EOF found when parsing list.",
                  "I expected a \"" + expected_end.value().as_string() +
                  "\" corresponding to this one.");
      return scoped_ptr<ListNode>();
    }
    if (cur_token().type() == expected_end.type() &&
        cur_token().value() == expected_end.value()) {
      list->set_end_token(cur_token());
      cur_++;
      break;
    }

    if (need_separator) {
      DCHECK(!list->contents().empty());
      LocationRange prev_item_range =
          list->contents().at(list->contents().size() - 1)->GetRange();
      *err_ = Err(prev_item_range.end(),
                  "Need comma separating items in list.",
                  "You probably need a comma after this thingy.");
      err_->AppendRange(prev_item_range);
      return scoped_ptr<ListNode>();
    }
    scoped_ptr<ParseNode> expr = ParseExpression().Pass();
    if (has_error())
      return scoped_ptr<ListNode>();
    list->append_item(expr.Pass());

    need_separator = true;
    if (!at_end()) {
      // Skip over the separator, marking that we found it.
      if (cur_token().type() == Token::SEPARATOR) {
        cur_++;
        need_separator = false;
      }
    }
  }
  return list.Pass();
}

// paren_expression := "(" expression ")"
scoped_ptr<ParseNode> Parser::ParseParenExpression() {
  const Token& open_paren_token = cur_token();
  cur_++;  // Skip over (

  scoped_ptr<ParseNode> ret = ParseExpression();
  if (has_error())
    return scoped_ptr<ParseNode>();

  if (at_end()) {
    *err_ = Err(open_paren_token, "EOF found when parsing expression.",
                "I was looking for a \")\" corresponding to this one.");
    return scoped_ptr<ParseNode>();
  }
  if (!cur_token().IsScoperEqualTo(")")) {
    *err_ = Err(open_paren_token, "Expected \")\" for expression",
                "I was looking for a \")\" corresponding to this one.");
    return scoped_ptr<ParseNode>();
  }
  cur_++;  // Skip over )
  return ret.Pass();
}

// unary_expression := "!" expression
scoped_ptr<UnaryOpNode> Parser::ParseUnaryOp() {
  scoped_ptr<UnaryOpNode> unary(new UnaryOpNode);

  DCHECK(!at_end() && IsUnaryOperator(cur_token()));
  const Token& op_token = cur_token();
  unary->set_op(op_token);
  cur_++;

  if (at_end()) {
    *err_ = Err(op_token, "Expected expression.",
                "This operator needs something to operate on.");
    return scoped_ptr<UnaryOpNode>();
  }
  unary->set_operand(ParseExpression().Pass());
  if (has_error())
    return scoped_ptr<UnaryOpNode>();
  return unary.Pass();
}

Err Parser::MakeEOFError(const std::string& message,
                         const std::string& help) const {
  if (tokens_.empty())
    return Err(Location(NULL, 1, 1), message, help);

  const Token& last = tokens_[tokens_.size() - 1];
  return Err(last, message, help);
}
