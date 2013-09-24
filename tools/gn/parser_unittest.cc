// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/input_file.h"
#include "tools/gn/parser.h"
#include "tools/gn/tokenizer.h"

namespace {

bool GetTokens(const InputFile* input, std::vector<Token>* result) {
  result->clear();
  Err err;
  *result = Tokenizer::Tokenize(input, &err);
  return !err.has_error();
}

bool IsIdentifierEqual(const ParseNode* node, const char* val) {
  if (!node)
    return false;
  const IdentifierNode* ident = node->AsIdentifier();
  if (!ident)
    return false;
  return ident->value().value() == val;
}

bool IsLiteralEqual(const ParseNode* node, const char* val) {
  if (!node)
    return false;
  const LiteralNode* lit = node->AsLiteral();
  if (!lit)
    return false;
  return lit->value().value() == val;
}

// Returns true if the given node as a simple assignment to a given value.
bool IsAssignment(const ParseNode* node, const char* ident, const char* value) {
  if (!node)
    return false;
  const BinaryOpNode* binary = node->AsBinaryOp();
  if (!binary)
    return false;
  return binary->op().IsOperatorEqualTo("=") &&
         IsIdentifierEqual(binary->left(), ident) &&
         IsLiteralEqual(binary->right(), value);
}

// Returns true if the given node is a block with one assignment statement.
bool IsBlockWithAssignment(const ParseNode* node,
                           const char* ident, const char* value) {
  if (!node)
    return false;
  const BlockNode* block = node->AsBlock();
  if (!block)
    return false;
  if (block->statements().size() != 1)
    return false;
  return IsAssignment(block->statements()[0], ident, value);
}

void DoParserPrintTest(const char* input, const char* expected) {
  std::vector<Token> tokens;
  InputFile input_file(SourceFile("/test"));
  input_file.SetContents(input);
  ASSERT_TRUE(GetTokens(&input_file, &tokens));

  Err err;
  scoped_ptr<ParseNode> result = Parser::Parse(tokens, &err);
  ASSERT_TRUE(result);

  std::ostringstream collector;
  result->Print(collector, 0);

  EXPECT_EQ(expected, collector.str());
}

// Expects the tokenizer or parser to identify an error at the given line and
// character.
void DoParserErrorTest(const char* input, int err_line, int err_char) {
  InputFile input_file(SourceFile("/test"));
  input_file.SetContents(input);

  Err err;
  std::vector<Token> tokens = Tokenizer::Tokenize(&input_file, &err);
  if (!err.has_error()) {
    scoped_ptr<ParseNode> result = Parser::Parse(tokens, &err);
    ASSERT_FALSE(result);
    ASSERT_TRUE(err.has_error());
  }

  EXPECT_EQ(err_line, err.location().line_number());
  EXPECT_EQ(err_char, err.location().char_offset());
}

}  // namespace

TEST(Parser, BinaryOp) {
  std::vector<Token> tokens;

  // Simple set expression.
  InputFile expr_input(SourceFile("/test"));
  expr_input.SetContents("a=2");
  ASSERT_TRUE(GetTokens(&expr_input, &tokens));
  Err err;
  Parser set(tokens, &err);
  scoped_ptr<ParseNode> expr = set.ParseExpression();
  ASSERT_TRUE(expr);

  const BinaryOpNode* binary_op = expr->AsBinaryOp();
  ASSERT_TRUE(binary_op);

  EXPECT_TRUE(binary_op->left()->AsIdentifier());

  EXPECT_TRUE(binary_op->op().type() == Token::OPERATOR);
  EXPECT_TRUE(binary_op->op().value() == "=");

  EXPECT_TRUE(binary_op->right()->AsLiteral());
}

TEST(Parser, Condition) {
  std::vector<Token> tokens;

  InputFile cond_input(SourceFile("/test"));
  cond_input.SetContents("if(1) { a = 2 }");
  ASSERT_TRUE(GetTokens(&cond_input, &tokens));
  Err err;
  Parser simple_if(tokens, &err);
  scoped_ptr<ConditionNode> cond = simple_if.ParseCondition();
  ASSERT_TRUE(cond);

  EXPECT_TRUE(IsLiteralEqual(cond->condition(), "1"));
  EXPECT_FALSE(cond->if_false());  // No else block.
  EXPECT_TRUE(IsBlockWithAssignment(cond->if_true(), "a", "2"));

  // Now try a complicated if/else if/else one.
  InputFile complex_if_input(SourceFile("/test"));
  complex_if_input.SetContents(
      "if(1) { a = 2 } else if (0) { a = 3 } else { a = 4 }");
  ASSERT_TRUE(GetTokens(&complex_if_input, &tokens));
  Parser complex_if(tokens, &err);
  cond = complex_if.ParseCondition();
  ASSERT_TRUE(cond);

  EXPECT_TRUE(IsLiteralEqual(cond->condition(), "1"));
  EXPECT_TRUE(IsBlockWithAssignment(cond->if_true(), "a", "2"));

  ASSERT_TRUE(cond->if_false());
  const ConditionNode* nested_cond = cond->if_false()->AsConditionNode();
  ASSERT_TRUE(nested_cond);
  EXPECT_TRUE(IsLiteralEqual(nested_cond->condition(), "0"));
  EXPECT_TRUE(IsBlockWithAssignment(nested_cond->if_true(), "a", "3"));
  EXPECT_TRUE(IsBlockWithAssignment(nested_cond->if_false(), "a", "4"));
}

TEST(Parser, FunctionCall) {
  const char* input = "foo(a, 1, 2,) bar()";
  const char* expected =
      "BLOCK\n"
      " FUNCTION(foo)\n"
      "  LIST\n"
      "   IDENTIFIER(a)\n"
      "   LITERAL(1)\n"
      "   LITERAL(2)\n"
      " FUNCTION(bar)\n"
      "  LIST\n";
  DoParserPrintTest(input, expected);
}

TEST(Parser, ParenExpression) {
  const char* input = "(foo(1)) + (a + b)";
  const char* expected =
      "BLOCK\n"
      " BINARY(+)\n"
      "  FUNCTION(foo)\n"
      "   LIST\n"
      "    LITERAL(1)\n"
      "  BINARY(+)\n"
      "   IDENTIFIER(a)\n"
      "   IDENTIFIER(b)\n";
  DoParserPrintTest(input, expected);
  DoParserErrorTest("(a +", 1, 4);
}

TEST(Parser, UnaryOp) {
  std::vector<Token> tokens;

  InputFile ident_input(SourceFile("/test"));
  ident_input.SetContents("!foo");
  ASSERT_TRUE(GetTokens(&ident_input, &tokens));
  Err err;
  Parser ident(tokens, &err);
  scoped_ptr<UnaryOpNode> op = ident.ParseUnaryOp();

  ASSERT_TRUE(op);
  EXPECT_TRUE(op->op().type() == Token::OPERATOR);
  EXPECT_TRUE(op->op().value() == "!");
}

TEST(Parser, CompleteFunction) {
  const char* input =
      "cc_test(\"foo\") {\n"
      "  sources = [\n"
      "    \"foo.cc\",\n"
      "    \"foo.h\"\n"
      "  ]\n"
      "  dependencies = [\n"
      "    \"base\"\n"
      "  ]\n"
      "}\n";
  const char* expected =
      "BLOCK\n"
      " FUNCTION(cc_test)\n"
      "  LIST\n"
      "   LITERAL(\"foo\")\n"
      "  BLOCK\n"
      "   BINARY(=)\n"
      "    IDENTIFIER(sources)\n"
      "    LIST\n"
      "     LITERAL(\"foo.cc\")\n"
      "     LITERAL(\"foo.h\")\n"
      "   BINARY(=)\n"
      "    IDENTIFIER(dependencies)\n"
      "    LIST\n"
      "     LITERAL(\"base\")\n";
  DoParserPrintTest(input, expected);
}

TEST(Parser, FunctionWithConditional) {
  const char* input =
      "cc_test(\"foo\") {\n"
      "  sources = [\"foo.cc\"]\n"
      "  if (OS == \"mac\") {\n"
      "    sources += \"bar.cc\"\n"
      "  } else if (OS == \"win\") {\n"
      "    sources -= [\"asd.cc\", \"foo.cc\"]\n"
      "  } else {\n"
      "    dependencies += [\"bar.cc\"]\n"
      "  }\n"
      "}\n";
  const char* expected =
      "BLOCK\n"
      " FUNCTION(cc_test)\n"
      "  LIST\n"
      "   LITERAL(\"foo\")\n"
      "  BLOCK\n"
      "   BINARY(=)\n"
      "    IDENTIFIER(sources)\n"
      "    LIST\n"
      "     LITERAL(\"foo.cc\")\n"
      "   CONDITION\n"
      "    BINARY(==)\n"
      "     IDENTIFIER(OS)\n"
      "     LITERAL(\"mac\")\n"
      "    BLOCK\n"
      "     BINARY(+=)\n"
      "      IDENTIFIER(sources)\n"
      "      LITERAL(\"bar.cc\")\n"
      "    CONDITION\n"
      "     BINARY(==)\n"
      "      IDENTIFIER(OS)\n"
      "      LITERAL(\"win\")\n"
      "     BLOCK\n"
      "      BINARY(-=)\n"
      "       IDENTIFIER(sources)\n"
      "       LIST\n"
      "        LITERAL(\"asd.cc\")\n"
      "        LITERAL(\"foo.cc\")\n"
      "     BLOCK\n"
      "      BINARY(+=)\n"
      "       IDENTIFIER(dependencies)\n"
      "       LIST\n"
      "        LITERAL(\"bar.cc\")\n";
  DoParserPrintTest(input, expected);
}

TEST(Parser, NestedBlocks) {
  const char* input = "{cc_test(\"foo\") {{foo=1}{}}}";
  const char* expected =
      "BLOCK\n"
      " BLOCK\n"
      "  FUNCTION(cc_test)\n"
      "   LIST\n"
      "    LITERAL(\"foo\")\n"
      "   BLOCK\n"
      "    BLOCK\n"
      "     BINARY(=)\n"
      "      IDENTIFIER(foo)\n"
      "      LITERAL(1)\n"
      "    BLOCK\n";
  DoParserPrintTest(input, expected);
}

TEST(Parser, List) {
  const char* input = "[] a = [1,asd,] b = [1, 2+3 - foo]";
  const char* expected =
      "BLOCK\n"
      " LIST\n"
      " BINARY(=)\n"
      "  IDENTIFIER(a)\n"
      "  LIST\n"
      "   LITERAL(1)\n"
      "   IDENTIFIER(asd)\n"
      " BINARY(=)\n"
      "  IDENTIFIER(b)\n"
      "  LIST\n"
      "   LITERAL(1)\n"
      "   BINARY(+)\n"
      "    LITERAL(2)\n"
      "    BINARY(-)\n"
      "     LITERAL(3)\n"
      "     IDENTIFIER(foo)\n";
  DoParserPrintTest(input, expected);

  DoParserErrorTest("[a, 2+,]", 1, 7);
  DoParserErrorTest("[,]", 1, 2);
  DoParserErrorTest("[a,,]", 1, 4);
}

TEST(Parser, UnterminatedBlock) {
  DoParserErrorTest("hello {", 1, 7);
}

TEST(Parser, BadlyTerminatedNumber) {
  DoParserErrorTest("1234z", 1, 5);
}
