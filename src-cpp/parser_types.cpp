#include "parser.h"

// =========================================================================
// Type annotation parsing
// =========================================================================

TypeAnnotation Parser::parse_type_annotation() {
  TypeAnnotation ann;

  // Reference type: &T or &mut T
  if (cur_tok.type == TokenType::Ampersand) {
    next_token(); // consume '&'
    bool is_mut = false;
    if (cur_tok.type == TokenType::Mut) {
      is_mut = true;
      next_token(); // consume 'mut'
    }
    TypeAnnotation inner = parse_type_annotation();
    if (has_error) return ann;
    ann = {is_mut ? TypeKind::MutRef : TypeKind::Ref};
    ann.tuple_types.push_back(std::move(inner));
    return ann;
  }

  if (cur_tok.type == TokenType::Void) {
    ann = {TypeKind::Void};
    next_token();
  } else if (cur_tok.type == TokenType::Int8) {
    ann = {TypeKind::Int8};
    next_token();
  } else if (cur_tok.type == TokenType::Int16) {
    ann = {TypeKind::Int16};
    next_token();
  } else if (cur_tok.type == TokenType::Int32) {
    ann = {TypeKind::Int32};
    next_token();
  } else if (cur_tok.type == TokenType::Int64) {
    ann = {TypeKind::Int64};
    next_token();
  } else if (cur_tok.type == TokenType::Uint8) {
    ann = {TypeKind::Uint8};
    next_token();
  } else if (cur_tok.type == TokenType::Uint16) {
    ann = {TypeKind::Uint16};
    next_token();
  } else if (cur_tok.type == TokenType::Uint32) {
    ann = {TypeKind::Uint32};
    next_token();
  } else if (cur_tok.type == TokenType::Uint64) {
    ann = {TypeKind::Uint64};
    next_token();
  } else if (cur_tok.type == TokenType::Float16) {
    ann = {TypeKind::Float16};
    next_token();
  } else if (cur_tok.type == TokenType::Float32) {
    ann = {TypeKind::Float32};
    next_token();
  } else if (cur_tok.type == TokenType::Float64) {
    ann = {TypeKind::Float64};
    next_token();
  } else if (cur_tok.type == TokenType::Bool) {
    ann = {TypeKind::Bool};
    next_token();
  } else if (cur_tok.type == TokenType::String) {
    ann = {TypeKind::String};
    next_token();
  } else if (cur_tok.type == TokenType::Char) {
    ann = {TypeKind::Char};
    next_token();
  } else if (cur_tok.type == TokenType::Cubical) {
    ann = {TypeKind::Cubical};
    next_token();
  } else if (cur_tok.type == TokenType::LParen) {
    // Tuple type: (T1, T2, ...)
    next_token(); // consume '('
    skip_newlines();
    std::vector<TypeAnnotation> elem_types;
    while (cur_tok.type != TokenType::RParen && cur_tok.type != TokenType::Eof) {
      if (!elem_types.empty()) {
        if (cur_tok.type != TokenType::Comma) {
          set_error("expected ',' or ')' in tuple type");
          return ann;
        }
        next_token();
        skip_newlines();
      }
      elem_types.push_back(parse_type_annotation());
      if (has_error) return ann;
      skip_newlines();
    }
    if (cur_tok.type != TokenType::RParen) {
      set_error("expected ')' to close tuple type");
      return ann;
    }
    next_token(); // consume ')'
    // A 1-tuple (T) is just T, not a tuple
    if (elem_types.size() == 1) {
      ann = elem_types[0];
    } else {
      ann = {TypeKind::Tuple};
      ann.tuple_types = std::move(elem_types);
    }
  } else if (cur_tok.type == TokenType::Fn) {
    // Function type: fn(T1, T2, ...) -> Ret
    next_token(); // consume 'fn'
    if (cur_tok.type != TokenType::LParen) {
      set_error("expected '(' after 'fn' in function type");
      return ann;
    }
    next_token(); // consume '('
    skip_newlines();
    std::vector<TypeAnnotation> fn_param_types;
    while (cur_tok.type != TokenType::RParen && cur_tok.type != TokenType::Eof) {
      if (!fn_param_types.empty()) {
        if (cur_tok.type != TokenType::Comma) {
          set_error("expected ',' or ')' in function type parameter list");
          return ann;
        }
        next_token();
        skip_newlines();
      }
      fn_param_types.push_back(parse_type_annotation());
      if (has_error) return ann;
      skip_newlines();
    }
    if (cur_tok.type != TokenType::RParen) {
      set_error("expected ')' to close function type parameter list");
      return ann;
    }
    next_token(); // consume ')'
    if (cur_tok.type != TokenType::Arrow) {
      set_error("expected '->' and return type in function type");
      return ann;
    }
    next_token(); // consume '->'
    TypeAnnotation fn_ret_type = parse_type_annotation();
    if (has_error) return ann;
    ann = {TypeKind::Fn};
    ann.tuple_types = std::move(fn_param_types);
    ann.tuple_types.push_back(std::move(fn_ret_type));
  } else if (cur_tok.type == TokenType::Identifier) {
    // Check if this is a type parameter name or Self
    if (cur_tok.text == "Self") {
      ann = {TypeKind::TypeParam};
      ann.struct_name = "Self";
      next_token();
    } else if (type_param_names.find(cur_tok.text) != type_param_names.end()) {
      ann = {TypeKind::TypeParam};
      ann.struct_name = cur_tok.text;
      next_token();
    } else {
      // Struct type: the identifier is the struct name (possibly namespaced,
      // e.g. foo::Point or a::b::Point).
      ann = {TypeKind::Struct};
      ann.struct_name = cur_tok.text;
      next_token();
      while (cur_tok.type == TokenType::ColonColon) {
        next_token(); // consume '::'
        if (cur_tok.type != TokenType::Identifier) {
          set_error("expected identifier after '::' in type name");
          return ann;
        }
        ann.struct_name += "::" + cur_tok.text;
        next_token();
      }
    }
  } else {
    set_error("expected type (void, int8, int16, int32, int64, uint8, uint16, uint32, uint64, float, bool, string, char, cubical, tuple, or struct name)");
    ann = {TypeKind::Int64};
    has_error = true;
    return ann;
  }

  // Handle generic type arguments: Foo<int, float>
  if (cur_tok.type == TokenType::Less && ann.kind == TypeKind::Struct) {
    next_token();
    while (cur_tok.type != TokenType::Greater && cur_tok.type != TokenType::Shr
           && cur_tok.type != TokenType::Eof) {
      if (!ann.type_args.empty()) {
        if (cur_tok.type != TokenType::Comma) {
          set_error("expected ',' or '>' in generic type arguments");
          return ann;
        }
        next_token();
      }
      ann.type_args.push_back(parse_type_annotation());
      if (has_error) return ann;
    }
    if (cur_tok.type == TokenType::Shr) {
      cur_tok.type = TokenType::Greater;
      // Don't consume: the caller's level will also see this as its '>'
      return ann;
    }
    if (cur_tok.type != TokenType::Greater) {
      set_error("expected '>' to close generic type arguments");
      return ann;
    }
    next_token();
  }

  // Parse pointer indirection levels (e.g. int* -> pointer to int)
  while (cur_tok.type == TokenType::Star) {
    ann.pointer_depth++;
    next_token();
  }

  // Parse array size (e.g. int[10]) or slice (e.g. int[])
  if (cur_tok.type == TokenType::LSquare) {
    next_token(); // consume '['
    if (cur_tok.type == TokenType::RSquare) {
      // Slice type: T[]
      next_token(); // consume ']'
      TypeAnnotation slice_ann = {TypeKind::Slice};
      slice_ann.tuple_types.push_back(ann);
      return slice_ann;
    }
    if (cur_tok.type != TokenType::Number) {
      set_error("expected array size as number literal");
      return ann;
    }
    ann.array_size = (int)cur_tok.num_val;
    next_token(); // consume number
    if (cur_tok.type != TokenType::RSquare) {
      set_error("expected ']' after array size");
      return ann;
    }
    next_token(); // consume ']'
  }

  return ann;
}
