#include "xyxy/compiler.h"

#include "xyxy/inst.h"
#include "xyxy/object.h"
#include "xyxy/scanner.h"

namespace xyxy {

// NOTE: the order must be the same as token definition.
// TODO(zq7): this whole piece code looks very ugly.
// clang-format off
// (
REGISTER_PREC_RULE(TOKEN_LEFT_PAREN)
    .PrefixRule(&Compiler::ParseGrouping)
    .InfixRule(nullptr)
    .PrecOrder(PREC_NONE);

REGISTER_PREC_RULE(TOKEN_RIGHT_PAREN);
REGISTER_PREC_RULE(TOKEN_LEFT_BRACE);
REGISTER_PREC_RULE(TOKEN_RIGHT_BRACE);
REGISTER_PREC_RULE(TOKEN_COMMA);
REGISTER_PREC_RULE(TOKEN_DOT);

// -
REGISTER_PREC_RULE(TOKEN_MINUS)
    .PrefixRule(&Compiler::ParseUnary)
    .InfixRule(&Compiler::ParseBinary)
    .PrecOrder(PREC_TERM);

// +
REGISTER_PREC_RULE(TOKEN_PLUS)
    .PrefixRule(nullptr)
    .InfixRule(&Compiler::ParseBinary)
    .PrecOrder(PREC_TERM);

REGISTER_PREC_RULE(TOKEN_SEMICOLON);

// /
REGISTER_PREC_RULE(TOKEN_SLASH)
    .PrefixRule(nullptr)
    .InfixRule(&Compiler::ParseBinary)
    .PrecOrder(PREC_FACTOR);

// *
REGISTER_PREC_RULE(TOKEN_STAR)
    .PrefixRule(nullptr)
    .InfixRule(&Compiler::ParseBinary)
    .PrecOrder(PREC_FACTOR);

REGISTER_PREC_RULE(TOKEN_BANG)
    .PrefixRule(&Compiler::ParseUnary);

REGISTER_PREC_RULE(TOKEN_BANG_EQUAL)
    .InfixRule(&Compiler::ParseBinary)
    .PrecOrder(PREC_EQUALITY);

REGISTER_PREC_RULE(TOKEN_EQUAL);

REGISTER_PREC_RULE(TOKEN_EQUAL_EQUAL)
    .InfixRule(&Compiler::ParseBinary)
    .PrecOrder(PREC_EQUALITY);

REGISTER_PREC_RULE(TOKEN_GREATER)
    .InfixRule(&Compiler::ParseBinary)
    .PrecOrder(PREC_COMPARISON);

REGISTER_PREC_RULE(TOKEN_GREATER_EQUAL)
    .InfixRule(&Compiler::ParseBinary)
    .PrecOrder(PREC_COMPARISON);

REGISTER_PREC_RULE(TOKEN_LESS)
    .InfixRule(&Compiler::ParseBinary)
    .PrecOrder(PREC_COMPARISON);

REGISTER_PREC_RULE(TOKEN_LESS_EQUAL)
    .InfixRule(&Compiler::ParseBinary)
    .PrecOrder(PREC_COMPARISON);

REGISTER_PREC_RULE(TOKEN_IDENTIFIER)
    .PrefixRule(&Compiler::ParseVariable);

REGISTER_PREC_RULE(TOKEN_STRING)
    .PrefixRule(&Compiler::ParseString);

REGISTER_PREC_RULE(TOKEN_NUMBER)
    .PrefixRule(&Compiler::ParseNumber);

REGISTER_PREC_RULE(TOKEN_AND);
REGISTER_PREC_RULE(TOKEN_IF);
REGISTER_PREC_RULE(TOKEN_ELSE);
REGISTER_PREC_RULE(TOKEN_FALSE)
    .PrefixRule(&Compiler::ParseLiteral);

REGISTER_PREC_RULE(TOKEN_FUN);
REGISTER_PREC_RULE(TOKEN_FOR);

REGISTER_PREC_RULE(TOKEN_NIL)
    .PrefixRule(&Compiler::ParseLiteral);

REGISTER_PREC_RULE(TOKEN_OR);
REGISTER_PREC_RULE(TOKEN_CLASS);
REGISTER_PREC_RULE(TOKEN_PRINT);
REGISTER_PREC_RULE(TOKEN_RETURN);
REGISTER_PREC_RULE(TOKEN_SUPER);
REGISTER_PREC_RULE(TOKEN_THIS);

REGISTER_PREC_RULE(TOKEN_TRUE)
    .PrefixRule(&Compiler::ParseLiteral);

REGISTER_PREC_RULE(TOKEN_VAR);
REGISTER_PREC_RULE(TOKEN_WHILE);
REGISTER_PREC_RULE(TOKEN_NEWLINE);
REGISTER_PREC_RULE(TOKEN_WHITESPACE);

REGISTER_PREC_RULE(TOKEN_ERROR);
REGISTER_PREC_RULE(TOKEN_EOF);

REGISTER_PREC_RULE(TOKEN_NONE)
    .PrefixRule(&Compiler::ParseGrouping);

// clang-format on

void Compiler::Advance() {
  prev_ = curr_;
  while (true) {
    curr_ = scanner_->ScanToken();
    VLOG(1) << "ScanToken: | Curr: " << GetLexeme(curr_)
            << " | Prev: " << GetLexeme(prev_);
    if (curr_.type != TOKEN_ERROR) {
      break;
    }
  }
}

void Compiler::Consume(TokenType type, const string& msg) {
  CHECK(curr_.type == type) << msg;
  Advance();
  return;
}

void Compiler::EmitByte(uint8 byte) { GetChunk()->Write(byte, prev_.line); }

void Compiler::EmitReturn() {
  VLOG(1) << "Emiting OP_RETURN";
  EmitByte(OP_RETURN);
}

void Compiler::EmitByte(uint8 byte1, uint8 byte2) {
  EmitByte(byte1);
  EmitByte(byte2);
}

int Compiler::MakeConstant(Value val) {
  // Get the index after adding this val into chunk.
  int idx = GetChunk()->AddConstant(val);
  CHECK(idx <= UINT8_MAX) << "Not expect to many consts in one chunk.";
  return idx;
}

void Compiler::EmitConstant(Value val) {
  VLOG(1) << "Emiting OP_CONSTANT: " << val.ToString();
  EmitByte(OP_CONSTANT, MakeConstant(val));
}

#define DEBUG_PARSER_ENTER(type)      \
  DebugParser debug;                  \
  debug.enter_pos = prev_.start;      \
  debug.parse_depth = parse_depth_++; \
  debug.msg = type

#define DEBUG_PARSER_EXIT()                    \
  debug.exit_pos = prev_.start + prev_.length; \
  parse_depth_--;                              \
  DebugParsing(debug)

// TODO(zq7): Make debug drawing looks more beautiful.
void Compiler::DebugParsing(const DebugParser& debug) {
  string prefix(debug.parse_depth * 3, '-');
  CHECK(debug.enter_pos < debug.exit_pos);
  string source = scanner_->GetSource(debug.enter_pos, debug.exit_pos);
  VLOG(1) << prefix << debug.msg << "--" << std::to_string(debug.parse_depth)
          << "|   " << source;
}

void Compiler::ParseString() {
  DEBUG_PARSER_ENTER("string");

  string str = scanner_->GetSource(prev_.start, prev_.start + prev_.length);
  int n = str.size();
  CHECK(n >= 2);
  EmitConstant(Value(new ObjString(str.substr(1, n - 2))));

  DEBUG_PARSER_EXIT();
}

string Compiler::GetLexeme(Token tt) { return scanner_->GetLexeme(tt); }

void Compiler::ParseNumber() {
  DEBUG_PARSER_ENTER("number");

  double val = strtod(GetLexeme(prev_).c_str(), nullptr);
  EmitConstant(Value(val));

  DEBUG_PARSER_EXIT();
}

void Compiler::ParseExpression() {
  DEBUG_PARSER_ENTER("expression");

  ParseWithPrecedenceOrder(PREC_ASSIGNMENT);

  DEBUG_PARSER_EXIT();
}

void Compiler::ParseGrouping() {
  DEBUG_PARSER_ENTER("group");

  ParseExpression();
  Consume(TOKEN_RIGHT_PAREN, "Expect ')' after ParseExpression.");

  DEBUG_PARSER_EXIT();
}

void Compiler::ParseUnary() {
  DEBUG_PARSER_ENTER("unary");

  TokenType op_type = prev_.type;
  ParseWithPrecedenceOrder(PREC_UNARY);
  switch (op_type) {
    case TOKEN_MINUS:
      EmitByte(OP_NEGATE);
      break;
    case TOKEN_BANG:
      EmitByte(OP_NOT);
      break;
    default:
      CHECK(false) << "Unreachable.";
      break;
  }

  DEBUG_PARSER_EXIT();
}

void Compiler::ParseBinary() {
  DEBUG_PARSER_ENTER("binary");

  TokenType op_type = prev_.type;
  PrecedenceRule* rule = GetRule(op_type);
  ParseWithPrecedenceOrder(PrecOrder(rule->prec_order + 1));
  switch (op_type) {
    case TOKEN_PLUS:
      VLOG(1) << "Emiting OP_ADD";
      EmitByte(OP_ADD);
      break;
    case TOKEN_MINUS:
      EmitByte(OP_SUB);
      break;
    case TOKEN_STAR:
      EmitByte(OP_MUL);
      break;
    case TOKEN_SLASH:
      EmitByte(OP_DIV);
      break;
    case TOKEN_BANG_EQUAL:
      EmitByte(OP_EQUAL, OP_NOT);
      break;
    case TOKEN_EQUAL_EQUAL:
      EmitByte(OP_EQUAL);
      break;
    case TOKEN_GREATER:
      EmitByte(OP_GREATER);
      break;
    case TOKEN_GREATER_EQUAL:
      EmitByte(OP_LESS, OP_NOT);
      break;
    case TOKEN_LESS:
      EmitByte(OP_LESS);
      break;
    case TOKEN_LESS_EQUAL:
      EmitByte(OP_GREATER, OP_NOT);
      break;
    default:
      CHECK(false) << "Unrecogined token: " << op_type;
      break;
  }

  DEBUG_PARSER_EXIT();
}

void Compiler::ParseLiteral() {
  DEBUG_PARSER_ENTER("literal");

  switch (prev_.type) {
    case TOKEN_FALSE:
      EmitByte(OP_FALSE);
      break;
    case TOKEN_TRUE:
      EmitByte(OP_TRUE);
      break;
    case TOKEN_NIL:
      EmitByte(OP_NIL);
      break;
    default:
      CHECK(false) << "Unreachable.";
      break;
  }

  DEBUG_PARSER_EXIT();
}

void Compiler::ParseWithPrecedenceOrder(PrecOrder prec_order) {
  DEBUG_PARSER_ENTER("prec_order");

  Advance();

  auto rule = GetRule(prev_.type);
  CHECK(rule);
  ParseFunc prefix_rule = rule->prefix_rule;
  CHECK(prefix_rule);

  prefix_rule(this);

  while (GetRule(curr_.type)->prec_order >= prec_order) {
    Advance();
    ParseFunc infix_rule = GetRule(prev_.type)->infix_rule;
    infix_rule(this);
  }

  DEBUG_PARSER_EXIT();
}

bool Compiler::CheckType(TokenType type) { return curr_.type == type; }

bool Compiler::Match(TokenType type) {
  if (!CheckType(type)) {
    return false;
  }
  Advance();
  return true;
}

// declaration    -> varDecl
//                | statement ;
void Compiler::ParseDeclaration() {
  VLOG(1) << "Parsing declaration...";
  DEBUG_PARSER_ENTER("declaration");
  if (Match(TOKEN_VAR)) {
    ParseVarDeclaration();
  }
  else {
    ParseStatement();
  }

  // TODO(): Error handling.
  DEBUG_PARSER_EXIT();
}

uint8 Compiler::IdentifierConstant(const string& name) {
  return MakeConstant(Value(new ObjString(name)));
}

void Compiler::ParseVarDeclaration() {
  VLOG(1) << "Parsing var declaration...";
  Consume(TOKEN_IDENTIFIER, "Expect variable name.");
  uint8 global = IdentifierConstant(GetLexeme(prev_));
  if (Match(TOKEN_EQUAL)) {
    ParseExpression();
  }
  else {
    // By default, assign a variable to NILL;
    EmitByte(OP_NIL);
  }
  Consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  DefineVariable(global);
}

void Compiler::DefineVariable(uint8 global) {
  VLOG(1) << "Emiting OP_DEFINE_GLOBAL " << global;
  EmitByte(OP_DEFINE_GLOBAL, global);
}

void Compiler::ParseVariable() {
  VLOG(1) << "Parsing variable...";
  uint8 arg = IdentifierConstant(GetLexeme(prev_));
  VLOG(1) << "Emiting OP_GET_GLOBAL " << arg;
  EmitByte(OP_GET_GLOBAL, arg);
}

// statement      -> exprStmt
//                | printStmt ;
void Compiler::ParseStatement() {
  VLOG(1) << "Parsing statement...";
  if (Match(TOKEN_PRINT)) {
    DEBUG_PARSER_ENTER("statement");
    ParsePrintStatement();
    DEBUG_PARSER_EXIT();
  }
  else {
    ParseExpressStatement();
  }
}

void Compiler::ParsePrintStatement() {
  VLOG(1) << "Parsing print...";
  DEBUG_PARSER_ENTER("print");
  ParseExpression();
  Consume(TOKEN_SEMICOLON, "Expect ';' after a value.");
  VLOG(1) << "Emiting OP_PRINT";
  EmitByte(OP_PRINT);
  DEBUG_PARSER_EXIT();
}

// var_a = "xy";
void Compiler::ParseExpressStatement() {
  ParseExpression();
  Consume(TOKEN_SEMICOLON, "Expect ';' after an expression.");
  EmitByte(OP_POP);
}

}  // namespace xyxy
