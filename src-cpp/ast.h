#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

// =========================================================================
// Hokkaido Language — AST
// =========================================================================

enum class TypeKind {
  Void,
  Infer,  // type inferred from init expression
  Int8,
  Int16,
  Int32,
  Int64,
  Uint8,
  Uint16,
  Uint32,
  Uint64,
  Float16,
  Float32,
  Float64,
  Bool,
  String,
  Char,
  Tuple,
  Struct,
  Enum,
  TypeParam,
  Slice,
  Fn,
  Ref,    // &T  — shared reference
  MutRef, // &mut T — mutable reference
};

struct TypeAnnotation {
  TypeKind kind;
  int pointer_depth = 0;
  int array_size = 0; // 0 means not an array
  std::string struct_name; // for Struct, Enum, TypeParam kind
  std::vector<TypeAnnotation> tuple_types; // for Tuple kind / Fn (param types + return type last)
  std::vector<TypeAnnotation> type_args; // for generic types like Foo<int>
};

enum class BinOp {
  Add, Sub, Mul, Div, Mod,
  Eq, Ne, Less, Greater, Le, Ge,
  And, Or, Shr, Shl,
  BitAnd, BitOr, Xor
};

enum class UnaryOp {
  Neg, BitNot
};

enum class AtomicOp {
  Xchg, Add, Sub, And, Or, Xor, CmpXchg, Fence
};

struct Expr {
  int line = 0;
  int col = 0;
  std::string file;
  virtual ~Expr() = default;
};

struct NumberExpr : Expr {
  double value;
  NumberExpr(double v) : value(v) {}
};

struct StringExpr : Expr {
  std::string value;
  StringExpr(const std::string &v) : value(v) {}
};

struct CharExpr : Expr {
  uint8_t value;
  CharExpr(uint8_t v) : value(v) {}
};

struct IdentExpr : Expr {
  std::string name;
  IdentExpr(const std::string &n) : name(n) {}
};

struct UnaryExpr : Expr {
  UnaryOp op;
  std::unique_ptr<Expr> operand;
  UnaryExpr(UnaryOp o, std::unique_ptr<Expr> e) : op(o), operand(std::move(e)) {}
};

struct BinaryExpr : Expr {
  std::unique_ptr<Expr> left;
  BinOp op;
  std::unique_ptr<Expr> right;
  BinaryExpr(std::unique_ptr<Expr> l, BinOp o, std::unique_ptr<Expr> r)
    : left(std::move(l)), op(o), right(std::move(r)) {}
};

struct CallExpr : Expr {
  std::string callee; // for named calls (e.g. foo(x))
  std::unique_ptr<Expr> callee_expr; // for expression calls (e.g. closure(x))
  std::vector<std::unique_ptr<Expr>> args;
  std::vector<TypeAnnotation> type_args;
};

struct AsmExpr : Expr {
  std::string asm_code;
};

struct AtomicExpr : Expr {
  AtomicOp op;
  std::vector<std::unique_ptr<Expr>> args;
};

struct AssignExpr : Expr {
  std::unique_ptr<Expr> target;
  std::unique_ptr<Expr> value;
  AssignExpr(std::unique_ptr<Expr> t, std::unique_ptr<Expr> v)
    : target(std::move(t)), value(std::move(v)) {}
};

struct CompoundAssignExpr : Expr {
  std::unique_ptr<Expr> target;
  BinOp op;
  std::unique_ptr<Expr> value;
  CompoundAssignExpr(std::unique_ptr<Expr> t, BinOp o, std::unique_ptr<Expr> v)
    : target(std::move(t)), op(o), value(std::move(v)) {}
};

struct NullExpr : Expr {};

struct BorrowExpr : Expr {
  std::unique_ptr<Expr> operand;
  bool is_mut; // false = &T, true = &mut T
  BorrowExpr(std::unique_ptr<Expr> o, bool m = false)
    : operand(std::move(o)), is_mut(m) {}
};

struct DerefExpr : Expr {
  std::unique_ptr<Expr> operand;
  DerefExpr(std::unique_ptr<Expr> o) : operand(std::move(o)) {}
};

struct SubscriptExpr : Expr {
  std::unique_ptr<Expr> array;
  std::unique_ptr<Expr> index;
  SubscriptExpr(std::unique_ptr<Expr> a, std::unique_ptr<Expr> i)
    : array(std::move(a)), index(std::move(i)) {}
};

struct ArrayLitExpr : Expr {
  std::vector<std::unique_ptr<Expr>> elements;
};

struct TupleExpr : Expr {
  std::vector<std::unique_ptr<Expr>> elements;
};

struct FieldAccessExpr : Expr {
  std::unique_ptr<Expr> object;
  std::string field;
  FieldAccessExpr(std::unique_ptr<Expr> o, const std::string &f)
    : object(std::move(o)), field(f) {}
};

// Pattern matching

struct Pattern {
  int line = 0;
  int col = 0;
  std::string file;
  virtual ~Pattern() = default;
};

struct WildcardPattern : Pattern {};

struct LiteralPattern : Pattern {
  std::unique_ptr<Expr> value;
  LiteralPattern(std::unique_ptr<Expr> v) : value(std::move(v)) {}
};

struct VariablePattern : Pattern {
  std::string name;
  VariablePattern(const std::string &n) : name(n) {}
};

struct StructPattern : Pattern {
  std::string struct_name;
  // Each field: (field_name, sub_pattern)
  std::vector<std::pair<std::string, std::unique_ptr<Pattern>>> fields;
};

struct VariantPattern : Pattern {
  std::string enum_name;
  std::string variant_name;
  std::vector<std::pair<std::string, std::unique_ptr<Pattern>>> fields;
};

struct MatchArm {
  std::unique_ptr<Pattern> pattern;
  std::unique_ptr<Expr> expr;
};

struct MatchExpr : Expr {
  std::unique_ptr<Expr> value;
  std::vector<MatchArm> arms;
};

struct IfExpr : Expr {
  std::unique_ptr<Expr> condition;
  std::unique_ptr<Expr> then_expr;
  std::unique_ptr<Expr> else_expr;
};


struct Decl {
  int line = 0;
  int col = 0;
  std::string file;
  virtual ~Decl() = default;
};

// =========================================================================
// Declarations
// =========================================================================

struct PackageDecl : Decl {
  std::string name;
};

struct ImportDecl : Decl {
  std::string path;
  std::string alias;
};

struct LetDecl : Decl {
  TypeAnnotation type_ann;
  std::string name;
  std::unique_ptr<Expr> init_expr;
  bool is_pub = false;
};

struct StructField {
  std::string name;
  TypeAnnotation type_ann;
};

struct StructDecl : Decl {
  std::string name;
  std::vector<StructField> fields;
  bool is_pub = false;
  std::vector<std::string> type_params; // generic type params like <T, U>
  std::map<std::string, std::vector<std::string>> type_param_bounds; // T -> [Trait1, Trait2]
};

// Algebraic data types (tagged unions / Rust-style enums)

struct AdtVariant {
  std::string name;
  std::vector<StructField> fields;
};

struct AdtDecl : Decl {
  std::string name;
  std::vector<AdtVariant> variants;
  bool is_pub = false;
};

struct ConstructorExpr : Expr {
  std::string enum_name;
  std::string variant_name;
  std::vector<std::pair<std::string, std::unique_ptr<Expr>>> fields;
  std::vector<TypeAnnotation> type_args; // generic type args like <int, float>
};

struct Param {
  std::string name;
  TypeAnnotation type_ann;
};

struct Stmt {
  int line = 0;
  int col = 0;
  std::string file;
  virtual ~Stmt() = default;
};

struct ExprStmt : Stmt {
  std::unique_ptr<Expr> expr;
  ExprStmt(std::unique_ptr<Expr> e) : expr(std::move(e)) {}
};

struct LetStmt : Stmt {
  TypeAnnotation type_ann;
  std::string name;
  std::unique_ptr<Expr> init_expr;
};

struct ReturnStmt : Stmt {
  std::unique_ptr<Expr> value;
};

struct BreakStmt : Stmt {
  std::string label;
};

struct ContinueStmt : Stmt {
  std::string label;
};

struct RegionStmt : Stmt {
  std::string name;
  std::vector<std::unique_ptr<Stmt>> body;
};

struct IfStmt : Stmt {
  std::unique_ptr<Expr> condition;
  std::vector<std::unique_ptr<Stmt>> then_branch;
  std::vector<std::unique_ptr<Stmt>> else_branch;
};

struct ForStmt : Stmt {
  std::string label;
  std::unique_ptr<Stmt> init;
  std::unique_ptr<Expr> condition;
  std::unique_ptr<Expr> update;
  std::vector<std::unique_ptr<Stmt>> body;
};

struct WhileStmt : Stmt {
  std::string label;
  std::unique_ptr<Expr> condition;
  std::vector<std::unique_ptr<Stmt>> body;
};

struct FnDecl : Decl {
  std::string name;
  std::vector<Param> params;
  TypeAnnotation return_type;
  std::vector<std::unique_ptr<Stmt>> body;
  // `extern fn foo(...) -> T` declarations: no body, declares a foreign
  // (typically C) symbol to link against rather than generating code for it.
  bool is_extern = false;
  // Whether the parameter list ends in `...` (a C-style variadic function,
  // e.g. printf). Only meaningful when is_extern is true.
  bool is_variadic = false;
  // Generic type parameter names, e.g. ["T", "U"] for `fn foo<T, U>(...)`.
  std::vector<std::string> type_params;
  // Trait bounds on type params: T -> [Trait1, Trait2]
  std::map<std::string, std::vector<std::string>> type_param_bounds;
  bool is_pub = false;
};

struct ClosureExpr : Expr {
  std::vector<Param> params;
  TypeAnnotation return_type;
  std::vector<std::unique_ptr<Stmt>> body;
};

// Trait method signature (declaration only, no body)
struct TraitMethodSig {
  std::string name;
  std::vector<Param> params;
  TypeAnnotation return_type;
};

// Trait declaration: trait Name { fn method(...) -> T; ... }
struct TraitDecl : Decl {
  std::string name;
  std::vector<TraitMethodSig> methods;
  bool is_pub = false;
};

// Impl block: impl Type { ... } or impl Trait for Type { ... }
struct ImplDecl : Decl {
  std::string trait_name; // empty for inherent impl
  std::string type_name;
  std::vector<std::unique_ptr<FnDecl>> methods;
  bool is_pub = false;
};

// Method call expression: obj.method(args)
struct MethodCallExpr : Expr {
  std::unique_ptr<Expr> object;
  std::string method_name;
  std::vector<std::unique_ptr<Expr>> args;
};