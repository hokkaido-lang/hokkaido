#include "parser.h"

// =========================================================================
// Expression parsing (recursive descent, operator precedence)
// =========================================================================

std::unique_ptr<Expr> Parser::parse_expr() {
  return parse_assignment();
}

std::unique_ptr<Expr> Parser::parse_assignment() {
  auto left = parse_logical_or();
  if (!left) return nullptr;

  auto is_lvalue = [](Expr *e) {
    return dynamic_cast<IdentExpr *>(e) ||
           dynamic_cast<DerefExpr *>(e) ||
           dynamic_cast<SubscriptExpr *>(e) ||
           dynamic_cast<FieldAccessExpr *>(e);
  };

  BinOp compound_op;
  bool is_compound = false;
  if (cur_tok.type == TokenType::PlusEq) { compound_op = BinOp::Add; is_compound = true; }
  else if (cur_tok.type == TokenType::MinusEq) { compound_op = BinOp::Sub; is_compound = true; }
  else if (cur_tok.type == TokenType::StarEq) { compound_op = BinOp::Mul; is_compound = true; }
  else if (cur_tok.type == TokenType::SlashEq) { compound_op = BinOp::Div; is_compound = true; }
  else if (cur_tok.type == TokenType::PercentEq) { compound_op = BinOp::Mod; is_compound = true; }
  else if (cur_tok.type == TokenType::AndEq) { compound_op = BinOp::BitAnd; is_compound = true; }
  else if (cur_tok.type == TokenType::OrEq) { compound_op = BinOp::BitOr; is_compound = true; }
  else if (cur_tok.type == TokenType::XorEq) { compound_op = BinOp::Xor; is_compound = true; }
  else if (cur_tok.type == TokenType::ShlEq) { compound_op = BinOp::Shl; is_compound = true; }
  else if (cur_tok.type == TokenType::ShrEq) { compound_op = BinOp::Shr; is_compound = true; }

  if (is_compound) {
    if (!is_lvalue(left.get())) {
      set_error("left side of compound assignment must be a variable, dereference, subscript, or field access");
      return nullptr;
    }
    next_token(); // consume the compound operator
    auto value = parse_assignment();
    if (!value) return nullptr;
    return make_expr<CompoundAssignExpr>(std::move(left), compound_op, std::move(value));
  }

  if (cur_tok.type == TokenType::Equals) {
    if (!is_lvalue(left.get())) {
      set_error("left side of assignment must be a variable, dereference, subscript, or field access");
      return nullptr;
    }
    next_token(); // consume '='
    auto value = parse_assignment(); // right-associative
    if (!value) return nullptr;
    return make_expr<AssignExpr>(std::move(left), std::move(value));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_logical_or() {
  auto left = parse_logical_and();
  if (!left) return nullptr;

  while (cur_tok.type == TokenType::OrOr) {
    next_token();
    auto right = parse_logical_and();
    if (!right) return nullptr;
    left = make_expr<BinaryExpr>(std::move(left), BinOp::Or, std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_logical_and() {
  auto left = parse_bitwise_or();
  if (!left) return nullptr;

  while (cur_tok.type == TokenType::AndAnd) {
    next_token();
    auto right = parse_bitwise_or();
    if (!right) return nullptr;
    left = make_expr<BinaryExpr>(std::move(left), BinOp::And, std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_bitwise_or() {
  auto left = parse_bitwise_xor();
  if (!left) return nullptr;

  while (cur_tok.type == TokenType::BitOr) {
    next_token();
    auto right = parse_bitwise_xor();
    if (!right) return nullptr;
    left = make_expr<BinaryExpr>(std::move(left), BinOp::BitOr, std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_bitwise_xor() {
  auto left = parse_bitwise_and();
  if (!left) return nullptr;

  while (cur_tok.type == TokenType::Xor) {
    next_token();
    auto right = parse_bitwise_and();
    if (!right) return nullptr;
    left = make_expr<BinaryExpr>(std::move(left), BinOp::Xor, std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_bitwise_and() {
  auto left = parse_comparison();
  if (!left) return nullptr;

  while (cur_tok.type == TokenType::Ampersand) {
    next_token();
    auto right = parse_comparison();
    if (!right) return nullptr;
    left = make_expr<BinaryExpr>(std::move(left), BinOp::BitAnd, std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_comparison() {
  auto left = parse_shift();
  if (!left) return nullptr;

  while (cur_tok.type == TokenType::Eq ||
         cur_tok.type == TokenType::Ne ||
         cur_tok.type == TokenType::Less ||
         cur_tok.type == TokenType::Greater ||
         cur_tok.type == TokenType::LessEqual ||
         cur_tok.type == TokenType::GreaterEqual) {
    BinOp op;
    switch (cur_tok.type) {
      case TokenType::Eq:          op = BinOp::Eq; break;
      case TokenType::Ne:          op = BinOp::Ne; break;
      case TokenType::Less:        op = BinOp::Less; break;
      case TokenType::Greater:     op = BinOp::Greater; break;
      case TokenType::LessEqual:   op = BinOp::Le; break;
      case TokenType::GreaterEqual: op = BinOp::Ge; break;
      default: return left;
    }
    next_token();
    auto right = parse_shift();
    if (!right) return nullptr;
    left = make_expr<BinaryExpr>(std::move(left), op, std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_shift() {
  auto left = parse_additive();
  if (!left) return nullptr;

  while (cur_tok.type == TokenType::Shr || cur_tok.type == TokenType::Shl) {
    BinOp op = (cur_tok.type == TokenType::Shr) ? BinOp::Shr : BinOp::Shl;
    next_token();
    auto right = parse_additive();
    if (!right) return nullptr;
    left = make_expr<BinaryExpr>(std::move(left), op, std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_additive() {
  auto left = parse_multiplicative();
  if (!left) return nullptr;

  while (cur_tok.type == TokenType::Plus || cur_tok.type == TokenType::Minus) {
    BinOp op = (cur_tok.type == TokenType::Plus) ? BinOp::Add : BinOp::Sub;
    next_token();
    auto right = parse_multiplicative();
    if (!right) return nullptr;
    left = make_expr<BinaryExpr>(std::move(left), op, std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_multiplicative() {
  auto left = parse_unary();
  if (!left) return nullptr;

  while (cur_tok.type == TokenType::Star || cur_tok.type == TokenType::Slash || cur_tok.type == TokenType::Percent) {
    BinOp op;
    if (cur_tok.type == TokenType::Star) op = BinOp::Mul;
    else if (cur_tok.type == TokenType::Slash) op = BinOp::Div;
    else op = BinOp::Mod;
    next_token();
    auto right = parse_unary();
    if (!right) return nullptr;
    left = make_expr<BinaryExpr>(std::move(left), op, std::move(right));
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_unary() {
  if (cur_tok.type == TokenType::Minus) {
    next_token();
    auto operand = parse_unary();
    if (!operand) return nullptr;
    return make_expr<UnaryExpr>(UnaryOp::Neg, std::move(operand));
  }
  if (cur_tok.type == TokenType::BitNot) {
    next_token();
    auto operand = parse_unary();
    if (!operand) return nullptr;
    return make_expr<UnaryExpr>(UnaryOp::BitNot, std::move(operand));
  }
  if (cur_tok.type == TokenType::Star) {
    next_token();
    auto operand = parse_unary();
    if (!operand) return nullptr;
    return make_expr<DerefExpr>(std::move(operand));
  }
  auto prim = parse_primary();
  if (!prim) return nullptr;
  return parse_postfix(std::move(prim));
}

std::unique_ptr<Expr> Parser::parse_postfix(std::unique_ptr<Expr> left) {
  // Handle function call: expr(args) — expression-based calls (closures)
  if (cur_tok.type == TokenType::LParen) {
    auto call = make_expr<CallExpr>();
    call->callee_expr = std::move(left);
    next_token(); // consume '('
    while (cur_tok.type != TokenType::RParen && cur_tok.type != TokenType::Eof) {
      if (!call->args.empty()) {
        if (cur_tok.type != TokenType::Comma) {
          set_error("expected ',' or ')' in argument list");
          return nullptr;
        }
        next_token();
      }
      auto arg = parse_expr();
      if (!arg) return nullptr;
      call->args.push_back(std::move(arg));
    }
    if (cur_tok.type != TokenType::RParen) {
      set_error("expected ')' to close argument list");
      return nullptr;
    }
    next_token(); // consume ')'
    left = std::move(call);
  }
  // Handle subscript access: arr[i]
  while (cur_tok.type == TokenType::LSquare) {
    next_token(); // consume '['
    auto index = parse_expr();
    if (!index) return nullptr;
    if (cur_tok.type != TokenType::RSquare) {
      set_error("expected ']' after index expression");
      return nullptr;
    }
    next_token(); // consume ']'
    left = make_expr<SubscriptExpr>(std::move(left), std::move(index));
  }
  // Handle field access: obj.field or obj.0 (tuple positional access)
  // and method call: obj.method(args)
  while (cur_tok.type == TokenType::Dot) {
    next_token(); // consume '.'
    if (cur_tok.type == TokenType::Number) {
      // Tuple positional access: .0, .1, etc.
      std::string field = std::to_string((int)cur_tok.num_val);
      next_token();
      left = make_expr<FieldAccessExpr>(std::move(left), field);
    } else if (cur_tok.type == TokenType::Identifier) {
      std::string name = cur_tok.text;
      next_token();
      // If followed by '(', it's a method call: obj.method(args)
      if (cur_tok.type == TokenType::LParen) {
        auto mcall = make_expr<MethodCallExpr>();
        mcall->object = std::move(left);
        mcall->method_name = name;
        next_token(); // consume '('
        while (cur_tok.type != TokenType::RParen && cur_tok.type != TokenType::Eof) {
          if (!mcall->args.empty()) {
            if (cur_tok.type != TokenType::Comma) {
              set_error("expected ',' or ')' in method arguments");
              return nullptr;
            }
            next_token();
          }
          auto arg = parse_expr();
          if (!arg) return nullptr;
          mcall->args.push_back(std::move(arg));
        }
        if (cur_tok.type != TokenType::RParen) {
          set_error("expected ')' to close method arguments");
          return nullptr;
        }
        next_token(); // consume ')'
        left = std::move(mcall);
      } else {
        left = make_expr<FieldAccessExpr>(std::move(left), name);
      }
    } else {
      set_error("expected field name or index after '.'");
      return nullptr;
    }
  }
  // Handle constructor expression: VariantName { field: expr, ... }
  // or positional: VariantName { expr, expr, ... }
  if (cur_tok.type == TokenType::Less || cur_tok.type == TokenType::LBrace) {
    auto *ident = dynamic_cast<IdentExpr *>(left.get());
    bool is_known = false;
    if (ident) {
      is_known = known_variants.count(ident->name) > 0 || known_structs.count(ident->name) > 0;
      // Also check unqualified name for namespaced structs (e.g. math::Point)
      if (!is_known) {
        auto pos = ident->name.rfind("::");
        if (pos != std::string::npos) {
          std::string unqualified = ident->name.substr(pos + 2);
          is_known = known_variants.count(unqualified) > 0 || known_structs.count(unqualified) > 0;
        }
      }
    }
    if (is_known) {
      std::string variant_name = ident->name;
      std::vector<TypeAnnotation> ctor_type_args;
      if (cur_tok.type == TokenType::Less) {
        next_token();
        while (cur_tok.type != TokenType::Greater && cur_tok.type != TokenType::Shr
               && cur_tok.type != TokenType::Eof) {
          if (!ctor_type_args.empty()) {
            if (cur_tok.type != TokenType::Comma) {
              set_error("expected ',' or '>' in generic type arguments");
              return nullptr;
            }
            next_token();
          }
          ctor_type_args.push_back(parse_type_annotation());
          if (has_error) return nullptr;
        }
        if (cur_tok.type == TokenType::Shr) {
          cur_tok.type = TokenType::Greater;
        } else {
          if (cur_tok.type != TokenType::Greater) {
            set_error("expected '>' to close generic type arguments");
            return nullptr;
          }
          next_token();
        }
        if (cur_tok.type != TokenType::LBrace) {
          set_error("expected '{' after generic type arguments");
          return nullptr;
        }
      }
      next_token(); // consume '{'
      skip_newlines();
      auto ctor = make_expr<ConstructorExpr>();
      ctor->variant_name = variant_name;
      ctor->type_args = std::move(ctor_type_args);
      bool is_named = false;
      bool has_fields = false;
      while (cur_tok.type != TokenType::RBrace && cur_tok.type != TokenType::Eof) {
        std::string field_name;
        std::unique_ptr<Expr> field_val;
        if (cur_tok.type == TokenType::Identifier) {
          std::string possible_name = cur_tok.text;
          next_token();
          skip_newlines();
          if (cur_tok.type == TokenType::Colon) {
            // Named field
            is_named = true;
            field_name = possible_name;
            next_token(); // consume ':'
            skip_newlines();
            field_val = parse_expr();
            if (!field_val) return nullptr;
          } else {
            // Positional: the identifier is an expression
            if (is_named && has_fields) {
              set_error("cannot mix named and positional fields");
              return nullptr;
            }
            field_val = make_expr<IdentExpr>(possible_name);
          }
        } else {
          if (is_named && has_fields) {
            set_error("cannot mix named and positional fields");
            return nullptr;
          }
          field_val = parse_expr();
          if (!field_val) return nullptr;
        }
        ctor->fields.push_back({field_name, std::move(field_val)});
        has_fields = true;
        skip_newlines();
        if (cur_tok.type == TokenType::Comma) {
          next_token();
          skip_newlines();
        }
      }
      if (cur_tok.type != TokenType::RBrace) {
        set_error("expected '}' to close constructor");
        return nullptr;
      }
      next_token(); // consume '}'
      return ctor;
    }
  }
  return left;
}

std::unique_ptr<Expr> Parser::parse_primary() {
  if (cur_tok.type == TokenType::True) {
    auto expr = make_expr<NumberExpr>(cur_tok.num_val);
    next_token();
    return expr;
  }
  if (cur_tok.type == TokenType::False) {
    auto expr = make_expr<NumberExpr>(cur_tok.num_val);
    next_token();
    return expr;
  }
  if (cur_tok.type == TokenType::Number) {
    auto expr = make_expr<NumberExpr>(cur_tok.num_val);
    next_token();
    return expr;
  }
  if (cur_tok.type == TokenType::CharLiteral) {
    auto expr = make_expr<CharExpr>((uint8_t)cur_tok.num_val);
    next_token();
    return expr;
  }
  if (cur_tok.type == TokenType::StringLiteral) {
    auto expr = make_expr<StringExpr>(cur_tok.text);
    next_token();
    return expr;
  }
  if (cur_tok.type == TokenType::Null) {
    next_token();
    return make_expr<NullExpr>();
  }
  if (cur_tok.type == TokenType::Ampersand) {
    next_token(); // consume '&'
    bool is_mut = false;
    if (cur_tok.type == TokenType::Mut) {
      is_mut = true;
      next_token(); // consume 'mut'
    }
    auto operand = parse_unary();
    if (!operand) return nullptr;
    return make_expr<BorrowExpr>(std::move(operand), is_mut);
  }
  if (cur_tok.type == TokenType::LSquare) {
    return parse_array_literal();
  }
  if (cur_tok.type == TokenType::Identifier) {
    std::string name = cur_tok.text;
    next_token();
    while (cur_tok.type == TokenType::ColonColon) {
      next_token(); // consume '::'
      // Turbofish: fn::<type1, type2>(args)
      if (cur_tok.type == TokenType::Less) {
        return parse_turbofish_call(name);
      }
      if (cur_tok.type != TokenType::Identifier) {
        set_error("expected identifier after '::'");
        return nullptr;
      }
      name += "::" + cur_tok.text;
      next_token();
    }
    if (cur_tok.type == TokenType::LParen) {
      return parse_call_rest(name);
    }
    return make_expr<IdentExpr>(name);
  }
  if (cur_tok.type == TokenType::Asm) {
    next_token(); // consume 'asm'
    if (cur_tok.type != TokenType::LParen) {
      set_error("expected '(' after asm");
      return nullptr;
    }
    next_token(); // consume '('
    if (cur_tok.type != TokenType::StringLiteral) {
      set_error("expected string literal in asm");
      return nullptr;
    }
    auto expr = make_expr<AsmExpr>();
    expr->asm_code = cur_tok.text;
    next_token();
    if (cur_tok.type != TokenType::RParen) {
      set_error("expected ')'");
      return nullptr;
    }
    next_token(); // consume ')'
    return expr;
  }
  if (cur_tok.type == TokenType::LParen) {
    next_token();
    skip_newlines();
    auto first = parse_expr();
    if (!first) return nullptr;
    skip_newlines();
    // If followed by a comma, this is a tuple expression
    if (cur_tok.type == TokenType::Comma) {
      auto tup = make_expr<TupleExpr>();
      tup->elements.push_back(std::move(first));
      while (cur_tok.type == TokenType::Comma) {
        next_token(); // consume ','
        skip_newlines();
        auto el = parse_expr();
        if (!el) return nullptr;
        tup->elements.push_back(std::move(el));
        skip_newlines();
      }
      if (cur_tok.type != TokenType::RParen) {
        set_error("expected ')' to close tuple expression");
        return nullptr;
      }
      next_token(); // consume ')'
      return tup;
    }
    // Single expression in parens
    if (cur_tok.type != TokenType::RParen) {
      set_error("expected ')'");
      return nullptr;
    }
    next_token();
    return first;
  }
  if (cur_tok.type == TokenType::Match) {
    return parse_match_expr();
  }
  if (cur_tok.type == TokenType::If) {
    return parse_if_expr();
  }
  if (cur_tok.type == TokenType::Atomic) {
    return parse_atomic_expr();
  }
  if (cur_tok.type == TokenType::Lambda) {
    return parse_lambda_expr();
  }
  set_error("expected expression");
  return nullptr;
}

std::unique_ptr<Expr> Parser::parse_if_expr() {
  next_token(); // consume 'if'
  auto cond = parse_expr();
  if (!cond) return nullptr;
  skip_newlines();

  // if-expression body is a single expression or a block { ... }
  std::unique_ptr<Expr> then_expr;
  if (cur_tok.type == TokenType::LBrace) {
    auto block_stmts = parse_block();
    if (has_error || block_stmts.empty()) return nullptr;
    // Wrap the last statement as an expression, or use the single expression
    then_expr = make_expr<NumberExpr>(0);
    // For blocks, we need to pick the last expression-stmt as the value.
    // Currently blocks produce void — this is a simplification.
    // We evaluate the last expr-stmt's expression.
    if (auto *es = dynamic_cast<ExprStmt *>(block_stmts.back().get())) {
      then_expr = std::move(es->expr);
      block_stmts.pop_back();
    }
    // If there are statements before the final expression, they're side effects.
    // For now, if-expression blocks must be a single expression or end with one.
    if (!block_stmts.empty()) {
      // Still use the last expression as the value; previous stmts are side-effectful
    }
  } else {
    then_expr = parse_expr();
    if (!then_expr) return nullptr;
  }

  skip_newlines();
  if (cur_tok.type != TokenType::Else) {
    set_error("expected 'else' for if-expression");
    return nullptr;
  }
  next_token(); // consume 'else'
  skip_newlines();

  std::unique_ptr<Expr> else_expr;
  if (cur_tok.type == TokenType::If) {
    else_expr = parse_if_expr();
    if (!else_expr) return nullptr;
  } else if (cur_tok.type == TokenType::LBrace) {
    auto block_stmts = parse_block();
    if (has_error || block_stmts.empty()) return nullptr;
    else_expr = make_expr<NumberExpr>(0);
    if (auto *es = dynamic_cast<ExprStmt *>(block_stmts.back().get())) {
      else_expr = std::move(es->expr);
      block_stmts.pop_back();
    }
  } else {
    else_expr = parse_expr();
    if (!else_expr) return nullptr;
  }

  auto expr = make_expr<IfExpr>();
  expr->condition = std::move(cond);
  expr->then_expr = std::move(then_expr);
  expr->else_expr = std::move(else_expr);
  return expr;
}

std::unique_ptr<Expr> Parser::parse_array_literal() {
  next_token(); // consume '['
  std::vector<std::unique_ptr<Expr>> elements;
  while (cur_tok.type != TokenType::RSquare) {
    if (!elements.empty()) {
      if (cur_tok.type != TokenType::Comma) {
        set_error("expected ',' or ']' in array literal");
        return nullptr;
      }
      next_token();
    }
    auto el = parse_expr();
    if (!el) return nullptr;
    elements.push_back(std::move(el));
  }
  next_token(); // consume ']'
  auto expr = make_expr<ArrayLitExpr>();
  expr->elements = std::move(elements);
  return expr;
}

std::unique_ptr<Expr> Parser::parse_call_rest(const std::string &name) {
  next_token(); // consume '('

  std::vector<std::unique_ptr<Expr>> args;
  while (cur_tok.type != TokenType::RParen) {
    if (!args.empty()) {
      if (cur_tok.type != TokenType::Comma) {
        set_error("expected ',' or ')' in arguments");
        return nullptr;
      }
      next_token();
    }
    auto arg = parse_expr();
    if (!arg) return nullptr;
    args.push_back(std::move(arg));
  }
  next_token(); // consume ')'

  auto expr = make_expr<CallExpr>();
  expr->callee = name;
  expr->args = std::move(args);
  return expr;
}

// Parse a turbofish generic call: name::<type1, type2>(args...)
// The current token is '<' (consumed after '::' by parse_primary).
std::unique_ptr<Expr> Parser::parse_turbofish_call(const std::string &name) {
  next_token(); // consume '<'

  std::vector<TypeAnnotation> type_args;
  while (cur_tok.type != TokenType::Greater) {
    if (!type_args.empty()) {
      if (cur_tok.type != TokenType::Comma) {
        set_error("expected ',' or '>' in type arguments");
        return nullptr;
      }
      next_token();
    }
    TypeAnnotation ta = parse_type_annotation();
    if (has_error) return nullptr;
    type_args.push_back(ta);
  }
  next_token(); // consume '>'

  if (cur_tok.type != TokenType::LParen) {
    set_error("expected '(' after type arguments in generic call");
    return nullptr;
  }
  next_token(); // consume '('

  std::vector<std::unique_ptr<Expr>> args;
  while (cur_tok.type != TokenType::RParen) {
    if (!args.empty()) {
      if (cur_tok.type != TokenType::Comma) {
        set_error("expected ',' or ')' in arguments");
        return nullptr;
      }
      next_token();
    }
    auto arg = parse_expr();
    if (!arg) return nullptr;
    args.push_back(std::move(arg));
  }
  next_token(); // consume ')'

  auto expr = make_expr<CallExpr>();
  expr->callee = name;
  expr->args = std::move(args);
  expr->type_args = std::move(type_args);
  return expr;
}

std::unique_ptr<Expr> Parser::parse_match_expr() {
  next_token(); // consume 'match'
  auto value = parse_expr();
  if (!value) return nullptr;
  skip_newlines();
  if (cur_tok.type != TokenType::LBrace) {
    set_error("expected '{' after match expression");
    return nullptr;
  }
  next_token(); // consume '{'
  skip_newlines();

  auto mexpr = make_expr<MatchExpr>();
  mexpr->value = std::move(value);

  while (cur_tok.type != TokenType::RBrace && cur_tok.type != TokenType::Eof) {
    auto pattern = parse_pattern();
    if (!pattern) return nullptr;
    skip_newlines();
    if (cur_tok.type != TokenType::FatArrow) {
      set_error("expected '=>' after pattern in match arm");
      return nullptr;
    }
    next_token(); // consume '=>'
    skip_newlines();
    auto body = parse_expr();
    if (!body) return nullptr;
    MatchArm arm;
    arm.pattern = std::move(pattern);
    arm.expr = std::move(body);
    mexpr->arms.push_back(std::move(arm));
    skip_newlines();
    // Optional comma separator between arms
    if (cur_tok.type == TokenType::Comma) {
      next_token();
      skip_newlines();
    }
  }
  if (cur_tok.type != TokenType::RBrace) {
    set_error("expected '}' to close match");
    return nullptr;
  }
  next_token(); // consume '}'
  return mexpr;
}

std::unique_ptr<Expr> Parser::parse_lambda_expr() {
  next_token(); // consume 'lambda'

  if (cur_tok.type != TokenType::LParen) {
    set_error("expected '(' after 'lambda'");
    return nullptr;
  }
  next_token(); // consume '('

  auto closure = make_expr<ClosureExpr>();
  while (cur_tok.type != TokenType::RParen && cur_tok.type != TokenType::Eof) {
    if (!closure->params.empty()) {
      if (cur_tok.type != TokenType::Comma) {
        set_error("expected ',' or ')' in lambda parameter list");
        return nullptr;
      }
      next_token();
    }
    Param param;
    if (cur_tok.type != TokenType::Identifier) {
      set_error("expected parameter name in lambda");
      return nullptr;
    }
    param.name = cur_tok.text;
    next_token();

    if (cur_tok.type != TokenType::Colon) {
      set_error("expected ':' after lambda parameter name");
      return nullptr;
    }
    next_token();

    param.type_ann = parse_type_annotation();
    if (has_error) return nullptr;
    closure->params.push_back(std::move(param));
  }
  next_token(); // consume ')'

  if (cur_tok.type != TokenType::Arrow) {
    set_error("expected '->' and return type in lambda");
    return nullptr;
  }
  next_token();

  closure->return_type = parse_type_annotation();
  if (has_error) return nullptr;

  skip_newlines();
  closure->body = parse_block();
  if (has_error) return nullptr;

  return closure;
}

std::unique_ptr<Expr> Parser::parse_atomic_expr() {
  next_token(); // consume 'atomic'

  if (cur_tok.type != TokenType::LParen) {
    set_error("expected '(' in atomic expression");
    return nullptr;
  }
  next_token(); // consume '('

  if (cur_tok.type != TokenType::Identifier) {
    set_error("expected atomic operation name (xchg, add, sub, and, or, xor, cas, fence)");
    return nullptr;
  }
  std::string op_name = cur_tok.text;
  next_token();

  AtomicOp op;
  if (op_name == "xchg") op = AtomicOp::Xchg;
  else if (op_name == "add") op = AtomicOp::Add;
  else if (op_name == "sub") op = AtomicOp::Sub;
  else if (op_name == "and") op = AtomicOp::And;
  else if (op_name == "or") op = AtomicOp::Or;
  else if (op_name == "xor") op = AtomicOp::Xor;
  else if (op_name == "cas") op = AtomicOp::CmpXchg;
  else if (op_name == "fence") op = AtomicOp::Fence;
  else {
    set_error("unknown atomic operation '" + op_name + "'");
    return nullptr;
  }

  // Expect comma before first argument (separates op name from args)
  if (cur_tok.type == TokenType::Comma) {
    next_token(); // consume comma
  }

  std::vector<std::unique_ptr<Expr>> args;
  while (cur_tok.type != TokenType::RParen && cur_tok.type != TokenType::Eof) {
    if (!args.empty()) {
      if (cur_tok.type != TokenType::Comma) {
        set_error("expected ',' or ')' in atomic args");
        return nullptr;
      }
      next_token();
    }
    auto arg = parse_expr();
    if (!arg) return nullptr;
    args.push_back(std::move(arg));
  }

  if (cur_tok.type != TokenType::RParen) {
    set_error("expected ')' after atomic arguments");
    return nullptr;
  }
  next_token(); // consume ')'

  auto expr = make_expr<AtomicExpr>();
  expr->op = op;
  expr->args = std::move(args);
  return expr;
}
