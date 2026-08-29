#include "parser.h"

// =========================================================================
// Statement and pattern parsing
// =========================================================================

std::vector<std::unique_ptr<Stmt>> Parser::parse_block() {
  if (cur_tok.type != TokenType::LBrace) {
    set_error("expected '{'", "function/struct/enum bodies must be wrapped in braces");
    return {};
  }
  next_token(); // consume '{'
  skip_newlines();

  std::vector<std::unique_ptr<Stmt>> stmts;
  while (cur_tok.type != TokenType::RBrace && cur_tok.type != TokenType::Eof) {
    auto stmt = parse_stmt();
    if (!stmt) break;
    stmts.push_back(std::move(stmt));
    skip_newlines();
  }
  if (has_error) return {};

  if (cur_tok.type != TokenType::RBrace) {
    set_error("expected '}'");
    return {};
  }
  next_token(); // consume '}'
  return stmts;
}

std::unique_ptr<Stmt> Parser::parse_stmt() {
  // Labeled statement: 'label: for ...
  if (cur_tok.type == TokenType::Tick) {
    next_token();
    if (cur_tok.type != TokenType::Identifier) {
      set_error("expected identifier after label prefix '");
      return nullptr;
    }
    std::string label = cur_tok.text;
    next_token();
    skip_newlines();
    if (cur_tok.type != TokenType::Colon) {
      set_error("expected ':' after label name");
      return nullptr;
    }
    next_token();
    skip_newlines();
    if (cur_tok.type != TokenType::For) {
      set_error("expected 'for' after label");
      return nullptr;
    }
    auto stmt = parse_for_stmt();
    if (stmt)
      static_cast<ForStmt *>(stmt.get())->label = label;
    return stmt;
  }
  if (cur_tok.type == TokenType::Let) {
    return parse_let_stmt();
  }
  if (cur_tok.type == TokenType::Return) {
    return parse_return_stmt();
  }
  if (cur_tok.type == TokenType::If) {
    return parse_if_stmt();
  }
  if (cur_tok.type == TokenType::For) {
    return parse_for_stmt();
  }
  if (cur_tok.type == TokenType::While) {
    return parse_while_stmt();
  }
  if (cur_tok.type == TokenType::Break) {
    return parse_break_stmt();
  }
  if (cur_tok.type == TokenType::Continue) {
    return parse_continue_stmt();
  }
  if (cur_tok.type == TokenType::Region) {
    return parse_region_stmt();
  }
  auto expr = parse_expr();
  if (!expr) return nullptr;
  return make_stmt<ExprStmt>(std::move(expr));
}

std::unique_ptr<LetStmt> Parser::parse_let_stmt() {
  next_token(); // consume 'let'

  TypeAnnotation type_ann;
  std::string name;
  std::unique_ptr<Expr> init;
  if (!parse_let_common(type_ann, name, init)) return nullptr;

  auto stmt = make_stmt<LetStmt>();
  stmt->type_ann = type_ann;
  stmt->name = name;
  stmt->init_expr = std::move(init);
  return stmt;
}

std::unique_ptr<ReturnStmt> Parser::parse_return_stmt() {
  next_token(); // consume 'return'

  if (cur_tok.type == TokenType::Number ||
      cur_tok.type == TokenType::True ||
      cur_tok.type == TokenType::False ||
      cur_tok.type == TokenType::CharLiteral ||
      cur_tok.type == TokenType::StringLiteral ||
      cur_tok.type == TokenType::Identifier ||
      cur_tok.type == TokenType::Asm ||
      cur_tok.type == TokenType::Match ||
      cur_tok.type == TokenType::Enum ||
      cur_tok.type == TokenType::LParen ||
      cur_tok.type == TokenType::LSquare ||
      cur_tok.type == TokenType::Minus ||
      cur_tok.type == TokenType::BitNot ||
      cur_tok.type == TokenType::Star ||
      cur_tok.type == TokenType::Ampersand ||
      cur_tok.type == TokenType::Null ||
      cur_tok.type == TokenType::Lambda) {
    auto expr = parse_expr();
    if (!expr) return nullptr;
    auto stmt = make_stmt<ReturnStmt>();
    stmt->value = std::move(expr);
    return stmt;
  }

  return make_stmt<ReturnStmt>(); // bare return (void)
}

std::unique_ptr<IfStmt> Parser::parse_if_stmt() {
  next_token(); // consume 'if'

  auto cond = parse_expr();
  if (!cond) return nullptr;

  skip_newlines();
  auto then_branch = parse_block();
  if (has_error) return nullptr;

  skip_newlines();
  std::vector<std::unique_ptr<Stmt>> else_branch;
  if (cur_tok.type == TokenType::Else) {
    next_token(); // consume 'else'
    skip_newlines();
    if (cur_tok.type == TokenType::If) {
      // `else if ...` chains: parse the nested if as a single statement
      // rather than requiring a `{ }` block. gen_if_stmt() runs
      // else_branch through gen_stmt(), which dynamic_casts back to
      // IfStmt and recurses, so no codegen changes are needed.
      auto elseif_stmt = parse_if_stmt();
      if (!elseif_stmt) return nullptr;
      else_branch.push_back(std::move(elseif_stmt));
    } else {
      else_branch = parse_block();
      if (has_error) return nullptr;
    }
  }

  auto stmt = make_stmt<IfStmt>();
  stmt->condition = std::move(cond);
  stmt->then_branch = std::move(then_branch);
  stmt->else_branch = std::move(else_branch);
  return stmt;
}

std::unique_ptr<ForStmt> Parser::parse_for_stmt() {
  next_token(); // consume 'for'
  skip_newlines();

  // Check for `for x in range` syntax (desugar to C-style for)
  if (cur_tok.type == TokenType::Identifier) {
    // Peek ahead: if we see `identifier in expr .. expr`, it's a for-in loop
    std::string var_name = cur_tok.text;
    next_token(); // consume variable name

    if (cur_tok.type == TokenType::In) {
      next_token(); // consume 'in'

      // Parse start expression
      auto start_expr = parse_expr();
      if (!start_expr) return nullptr;

      bool inclusive = false;
      if (cur_tok.type == TokenType::DotDotEq) {
        inclusive = true;
        next_token(); // consume '..='
      } else if (cur_tok.type == TokenType::DotDot) {
        next_token(); // consume '..'
      } else {
        set_error("expected '..' or '..=' after range start");
        return nullptr;
      }

      // Parse end expression
      auto end_expr = parse_expr();
      if (!end_expr) return nullptr;

      skip_newlines();
      auto body = parse_block();
      if (has_error) return nullptr;

      // Desugar: let var = start; for ; var < end (or <=); var = var + 1 { body }
      auto stmt = make_stmt<ForStmt>();

      // init: let var: <inferred> = start
      auto let_init = make_stmt<LetStmt>();
      let_init->name = var_name;
      let_init->type_ann.kind = TypeKind::Infer;
      let_init->init_expr = std::move(start_expr);
      stmt->init = std::move(let_init);

      // condition: var < end (or var <= end for inclusive)
      auto var_expr = make_expr<IdentExpr>(var_name);
      BinOp cmp_op = inclusive ? BinOp::Le : BinOp::Less;
      stmt->condition = std::make_unique<BinaryExpr>(std::move(var_expr), cmp_op, std::move(end_expr));

      // update: var = var + 1
      auto var_for_inc = make_expr<IdentExpr>(var_name);
      auto one = make_expr<NumberExpr>(1.0);
      auto inc = std::make_unique<BinaryExpr>(std::move(var_for_inc), BinOp::Add, std::move(one));
      auto var_for_assign = make_expr<IdentExpr>(var_name);
      stmt->update = std::make_unique<AssignExpr>(std::move(var_for_assign), std::move(inc));

      stmt->body = std::move(body);
      return stmt;
    }

    // Not a for-in loop — rewind and parse as regular for loop
    // We consumed identifier, so we need to put it back via parse_expr
    // Actually, we already consumed the identifier. Parse the rest as C-style for.
    // The init is an expression assignment or just the identifier as expression.
    auto expr = make_expr<IdentExpr>(var_name);
    auto init_expr = parse_postfix(std::move(expr));
    if (!init_expr) return nullptr;

    auto init = make_stmt<ExprStmt>(std::move(init_expr));

    if (cur_tok.type != TokenType::Semicolon) {
      set_error("expected ';' after for init");
      return nullptr;
    }
    next_token();
    skip_newlines();

    std::unique_ptr<Expr> cond;
    if (cur_tok.type != TokenType::Semicolon) {
      cond = parse_expr();
      if (!cond) return nullptr;
    }

    if (cur_tok.type != TokenType::Semicolon) {
      set_error("expected ';' after for condition");
      return nullptr;
    }
    next_token();
    skip_newlines();

    std::unique_ptr<Expr> update;
    if (cur_tok.type != TokenType::LBrace) {
      update = parse_expr();
      if (!update) return nullptr;
    }

    skip_newlines();
    auto body = parse_block();
    if (has_error) return nullptr;

    auto stmt = make_stmt<ForStmt>();
    stmt->init = std::move(init);
    stmt->condition = std::move(cond);
    stmt->update = std::move(update);
    stmt->body = std::move(body);
    return stmt;
  }

  // Original C-style for loop: for init; cond; update { body }
  std::unique_ptr<Stmt> init;
  if (cur_tok.type == TokenType::Let) {
    init = parse_let_stmt();
    if (!init) return nullptr;
  } else if (cur_tok.type != TokenType::Semicolon) {
    auto expr = parse_expr();
    if (!expr) return nullptr;
    init = make_stmt<ExprStmt>(std::move(expr));
  }

  if (cur_tok.type != TokenType::Semicolon) {
    set_error("expected ';' after for init");
    return nullptr;
  }
  next_token();
  skip_newlines();

  std::unique_ptr<Expr> cond;
  if (cur_tok.type != TokenType::Semicolon) {
    cond = parse_expr();
    if (!cond) return nullptr;
  }

  if (cur_tok.type != TokenType::Semicolon) {
    set_error("expected ';' after for condition");
    return nullptr;
  }
  next_token();
  skip_newlines();

  std::unique_ptr<Expr> update;
  if (cur_tok.type != TokenType::LBrace) {
    update = parse_expr();
    if (!update) return nullptr;
  }

  skip_newlines();
  auto body = parse_block();
  if (has_error) return nullptr;

  auto stmt = make_stmt<ForStmt>();
  stmt->init = std::move(init);
  stmt->condition = std::move(cond);
  stmt->update = std::move(update);
  stmt->body = std::move(body);
  return stmt;
}

std::unique_ptr<WhileStmt> Parser::parse_while_stmt() {
  next_token(); // consume 'while'
  skip_newlines();

  auto cond = parse_expr();
  if (!cond) return nullptr;

  skip_newlines();
  auto body = parse_block();
  if (has_error) return nullptr;

  auto stmt = make_stmt<WhileStmt>();
  stmt->condition = std::move(cond);
  stmt->body = std::move(body);
  return stmt;
}

std::unique_ptr<BreakStmt> Parser::parse_break_stmt() {
  next_token(); // consume 'break'
  skip_newlines();
  std::string label;
  if (cur_tok.type == TokenType::Tick) {
    next_token(); // consume '
    if (cur_tok.type != TokenType::Identifier) {
      set_error("expected identifier after ' in break");
      return nullptr;
    }
    label = cur_tok.text;
    next_token();
  }
  auto stmt = make_stmt<BreakStmt>();
  stmt->label = label;
  return stmt;
}

std::unique_ptr<ContinueStmt> Parser::parse_continue_stmt() {
  next_token(); // consume 'continue'
  skip_newlines();
  std::string label;
  if (cur_tok.type == TokenType::Tick) {
    next_token(); // consume '
    if (cur_tok.type != TokenType::Identifier) {
      set_error("expected identifier after ' in continue");
      return nullptr;
    }
    label = cur_tok.text;
    next_token();
  }
  auto stmt = make_stmt<ContinueStmt>();
  stmt->label = label;
  return stmt;
}

std::unique_ptr<Stmt> Parser::parse_region_stmt() {
  next_token(); // consume 'region'

  std::string name;
  if (cur_tok.type == TokenType::Identifier) {
    name = cur_tok.text;
    next_token();
    skip_newlines();
  }

  if (cur_tok.type != TokenType::LBrace) {
    set_error("expected '{' after region" + (name.empty() ? "" : " '" + name + "'"));
    return nullptr;
  }

  auto body = parse_block();
  if (has_error) return nullptr;

  auto stmt = make_stmt<RegionStmt>();
  stmt->name = name;
  stmt->body = std::move(body);
  return stmt;
}

// =========================================================================
// Pattern parsing
// =========================================================================

std::unique_ptr<Pattern> Parser::parse_pattern() {
  if (cur_tok.type == TokenType::Identifier) {
    std::string name = cur_tok.text;
    next_token();
    // Wildcard
    if (name == "_")
      return make_pattern<WildcardPattern>();
    // Qualified enum variant: EnumName::VariantName or VariantName
    while (cur_tok.type == TokenType::ColonColon) {
      next_token(); // consume '::'
      if (cur_tok.type != TokenType::Identifier) {
        set_error("expected variant name after '::'");
        return nullptr;
      }
      name += "::" + cur_tok.text;
      next_token();
    }
    // Struct pattern: Identifier { ... }
    skip_newlines();
    if (cur_tok.type == TokenType::LBrace) {
      next_token(); // consume '{'
      skip_newlines();
      auto pat = make_pattern<StructPattern>();
      pat->struct_name = name;
      while (cur_tok.type != TokenType::RBrace && cur_tok.type != TokenType::Eof) {
        if (cur_tok.type != TokenType::Identifier) {
          set_error("expected field name in struct pattern");
          return nullptr;
        }
        std::string field_name = cur_tok.text;
        next_token();
        skip_newlines();
        std::unique_ptr<Pattern> field_pat;
        if (cur_tok.type == TokenType::Colon) {
          next_token(); // consume ':'
          skip_newlines();
          field_pat = parse_pattern();
          if (!field_pat) return nullptr;
        } else {
          // Shorthand: field_name acts as both the field name and variable binding
          field_pat = make_pattern<VariablePattern>(field_name);
        }
        pat->fields.push_back({field_name, std::move(field_pat)});
        skip_newlines();
        if (cur_tok.type == TokenType::Comma) {
          next_token();
          skip_newlines();
        }
      }
      if (cur_tok.type != TokenType::RBrace) {
        set_error("expected '}' to close struct pattern");
        return nullptr;
      }
      next_token(); // consume '}'
      return pat;
    }
    // Variable pattern
    return make_pattern<VariablePattern>(name);
  }
  if (cur_tok.type == TokenType::Number ||
      cur_tok.type == TokenType::True ||
      cur_tok.type == TokenType::False) {
    auto expr = make_expr<NumberExpr>(cur_tok.num_val);
    next_token();
    return make_pattern<LiteralPattern>(std::move(expr));
  }
  if (cur_tok.type == TokenType::StringLiteral) {
    auto expr = make_expr<StringExpr>(cur_tok.text);
    next_token();
    return make_pattern<LiteralPattern>(std::move(expr));
  }
  if (cur_tok.type == TokenType::CharLiteral) {
    auto expr = make_expr<CharExpr>((uint8_t)cur_tok.text[0]);
    next_token();
    return make_pattern<LiteralPattern>(std::move(expr));
  }
  if (cur_tok.type == TokenType::Null) {
    next_token();
    return make_pattern<LiteralPattern>(make_expr<NullExpr>());
  }
  if (cur_tok.type == TokenType::Minus) {
    next_token();
    if (cur_tok.type != TokenType::Number) {
      set_error("expected number after '-' in pattern");
      return nullptr;
    }
    auto expr = make_expr<NumberExpr>(-cur_tok.num_val);
    next_token();
    return make_pattern<LiteralPattern>(std::move(expr));
  }
  set_error("expected pattern");
  return nullptr;
}
