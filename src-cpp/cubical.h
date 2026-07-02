#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// =========================================================================
// Cubical FFI — link against the Rust cubical_c static library
// =========================================================================

extern "C" {
/// Evaluate a cubical source string. Returns a C string (caller must free
/// with cubical_free_string), or nullptr on error.
char *cubical_eval(const char *source);

/// Evaluate a cubical source string and return the result as JSON.
/// Returns a C string (caller must free with cubical_free_string),
/// or nullptr on error.
char *cubical_eval_json(const char *source);

/// Evaluate a cubical source string and return the result as a 64-bit
/// integer. The expression must evaluate to a natural number (Nat).
/// Returns -1 on error.
int64_t cubical_eval_int(const char *source);

/// Get the last error message (caller must free with cubical_free_string).
/// Returns nullptr if no error.
char *cubical_get_last_error();

/// Free a string returned by cubical_eval or cubical_get_last_error.
void cubical_free_string(char *s);
}

// =========================================================================
// Cubical C++ Wrapper
// =========================================================================

/// A compile-time evaluated cubical expression.
class cubical_value {
  std::string result_;
  bool valid_ = false;
  std::string error_msg_;
  std::string json_;

public:
  explicit cubical_value(const std::string &source);

  bool valid() const { return valid_; }
  const std::string &str() const { return result_; }
  const std::string &error() const;

  /// If the result is a natural number, return its value. Returns -1 if
  /// the result is not a Nat or evaluation failed.
  int64_t as_int() const;

  /// Return the raw JSON result string from cubical_eval_json.
  /// Only valid if valid() is true.
  const std::string &json() const { return json_; }

  /// Parse the JSON result into a structured value tree.
  /// Returns nullptr on parse failure.
  struct CubicalValue;
  std::unique_ptr<CubicalValue> parse_json() const;

  /// A structured cubical value parsed from JSON.
  struct CubicalValue {
    enum Kind { Nat, Bool, Pair, Array, Constructor, String };
    Kind kind;
    int64_t nat_value = 0;
    bool bool_value = false;
    std::unique_ptr<CubicalValue> first;
    std::unique_ptr<CubicalValue> second;
    std::vector<std::unique_ptr<CubicalValue>> elements;
    std::string data_name;      // for Constructor
    std::string con_name;      // for Constructor
    std::vector<std::unique_ptr<CubicalValue>> args; // for Constructor
    std::string str_value;     // for String

    static std::unique_ptr<CubicalValue> parse(const std::string &json);
  };

private:
  int64_t parse_nat(const std::string &s) const;
};