#include "cubical.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <utility>

static std::string fetch_last_error() {
  char *err = cubical_get_last_error();
  if (!err) return {};
  std::string msg(err);
  cubical_free_string(err);
  return msg;
}

cubical_value::cubical_value(const std::string &source) {
  {
    char *res = cubical_eval(source.c_str());
    if (res) {
      result_ = res;
      valid_ = true;
      cubical_free_string(res);
      error_msg_.clear();
    } else {
      error_msg_ = fetch_last_error();
      if (error_msg_.empty())
        error_msg_ = "cubical evaluation failed (unknown error)";
    }
  }
  {
    char *res = cubical_eval_json(source.c_str());
    if (res) {
      json_ = res;
      cubical_free_string(res);
    }
  }
}

const std::string &cubical_value::error() const {
  return error_msg_;
}

int64_t cubical_value::as_int() const {
  if (!valid_) return -1;
  auto eq_pos = result_.find(" = ");
  if (eq_pos == std::string::npos) return -1;
  std::string nat_str = result_.substr(eq_pos + 3);
  return parse_nat(nat_str);
}

int64_t cubical_value::parse_nat(const std::string &s) const {
  std::string t = s;
  while (!t.empty() && t.front() == ' ') t.erase(0, 1);
  while (!t.empty() && t.back() == ' ') t.pop_back();
  if (t.empty()) return -1;

  // Try decimal first (Nat may now be displayed as "253" instead of
  // "suc (suc ... zero)").
  try {
    size_t pos;
    int64_t val = std::stoll(t, &pos);
    if (pos == t.length() && val >= 0) {
      return val;
    }
  } catch (...) {}

  while (t.size() >= 2 && t.front() == '(' && t.back() == ')') {
    t = t.substr(1, t.size() - 2);
    while (!t.empty() && t.front() == ' ') t.erase(0, 1);
    while (!t.empty() && t.back() == ' ') t.pop_back();

    try {
      size_t pos;
      int64_t val = std::stoll(t, &pos);
      if (pos == t.length() && val >= 0) {
        return val;
      }
    } catch (...) {}
  }

  if (t == "zero") return 0;
  if (t.size() >= 4 && t.substr(0, 4) == "suc ") {
    int64_t inner = parse_nat(t.substr(4));
    return (inner >= 0) ? inner + 1 : -1;
  }
  if (t.size() >= 4 && t.substr(0, 4) == "suc(") {
    int64_t inner = parse_nat("(" + t.substr(3));
    return (inner >= 0) ? inner + 1 : -1;
  }
  return -1;
}

// ---------------------------------------------------------------------------
// Minimal JSON parser for cubical structured values
// ---------------------------------------------------------------------------

namespace {

struct JsonLoc {
  const std::string &s;
  size_t pos = 0;

  void skip_ws() {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' ||
                              s[pos] == '\n' || s[pos] == '\r'))
      pos++;
  }

  bool peek(char c) {
    skip_ws();
    return pos < s.size() && s[pos] == c;
  }

  char next() {
    skip_ws();
    return pos < s.size() ? s[pos++] : '\0';
  }

  bool match(char c) {
    skip_ws();
    if (pos < s.size() && s[pos] == c) {
      pos++;
      return true;
    }
    return false;
  }

  std::string parse_string() {
    skip_ws();
    if (!match('"')) return {};
    std::string out;
    while (pos < s.size() && s[pos] != '"') {
      if (s[pos] == '\\') {
        pos++;
        if (pos >= s.size()) break;
        switch (s[pos]) {
          case '"': out += '"'; break;
          case '\\': out += '\\'; break;
          case 'n': out += '\n'; break;
          case 'r': out += '\r'; break;
          case 't': out += '\t'; break;
          default: out += s[pos]; break;
        }
      } else {
        out += s[pos];
      }
      pos++;
    }
    match('"');
    return out;
  }

  int64_t parse_number() {
    skip_ws();
    size_t start = pos;
    if (pos < s.size() && s[pos] == '-') pos++;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') pos++;
    return std::stoll(s.substr(start, pos - start));
  }

  bool parse_bool() {
    skip_ws();
    if (s.substr(pos, 4) == "true") { pos += 4; return true; }
    if (s.substr(pos, 5) == "false") { pos += 5; return false; }
    return false;
  }

  std::unique_ptr<cubical_value::CubicalValue> parse_value();

  std::unique_ptr<cubical_value::CubicalValue> parse_object() {
    skip_ws();
    if (!match('{')) return nullptr;
    auto val = std::make_unique<cubical_value::CubicalValue>();

    while (!peek('}') && pos < s.size()) {
      std::string key = parse_string();
      if (key.empty()) break;
      match(':');
      auto v = parse_value();
      if (!v) break;

      if (key == "kind") {
        if (v->kind == cubical_value::CubicalValue::String) {
          if (v->str_value == "nat") val->kind = cubical_value::CubicalValue::Nat;
          else if (v->str_value == "bool") val->kind = cubical_value::CubicalValue::Bool;
          else if (v->str_value == "string") val->kind = cubical_value::CubicalValue::String;
          else if (v->str_value == "pair") val->kind = cubical_value::CubicalValue::Pair;
          else if (v->str_value == "array") val->kind = cubical_value::CubicalValue::Array;
          else if (v->str_value == "constructor") val->kind = cubical_value::CubicalValue::Constructor;
        }
      } else if (key == "value") {
        if (val->kind == cubical_value::CubicalValue::Nat) val->nat_value = v->nat_value;
        else if (val->kind == cubical_value::CubicalValue::Bool) val->bool_value = v->bool_value;
        else if (val->kind == cubical_value::CubicalValue::String) val->str_value = std::move(v->str_value);
      } else if (key == "first") {
        val->first = std::move(v);
      } else if (key == "second") {
        val->second = std::move(v);
      } else if (key == "elements") {
        if (v->kind == cubical_value::CubicalValue::Array) val->elements = std::move(v->elements);
      } else if (key == "data") {
        if (v->kind == cubical_value::CubicalValue::String) val->data_name = std::move(v->str_value);
      } else if (key == "constructor") {
        if (v->kind == cubical_value::CubicalValue::String) val->con_name = std::move(v->str_value);
      } else if (key == "args") {
        if (v->kind == cubical_value::CubicalValue::Array) val->args = std::move(v->elements);
      }

      match(',');
    }
    match('}');
    return val;
  }
};

std::unique_ptr<cubical_value::CubicalValue> JsonLoc::parse_value() {
  skip_ws();
  if (pos >= s.size()) return nullptr;

  if (s[pos] == '{') return parse_object();
  if (s[pos] == '[') {
    // Array value
    match('[');
    auto arr = std::make_unique<cubical_value::CubicalValue>();
    arr->kind = cubical_value::CubicalValue::Array;
    while (!peek(']') && pos < s.size()) {
      auto elem = parse_value();
      if (elem) {
        arr->elements.push_back(std::move(elem));
      }
      match(',');
    }
    match(']');
    return arr;
  }
  if (s[pos] == '"') {
    auto str = std::make_unique<cubical_value::CubicalValue>();
    str->kind = cubical_value::CubicalValue::String;
    str->str_value = parse_string();
    return str;
  }
  if (s[pos] == 't' || s[pos] == 'f') {
    bool b = parse_bool();
    auto val = std::make_unique<cubical_value::CubicalValue>();
    val->kind = cubical_value::CubicalValue::Bool;
    val->bool_value = b;
    return val;
  }
  if (s[pos] == '-' || (s[pos] >= '0' && s[pos] <= '9')) {
    auto val = std::make_unique<cubical_value::CubicalValue>();
    val->kind = cubical_value::CubicalValue::Nat;
    val->nat_value = parse_number();
    return val;
  }
  return nullptr;
}

} // anonymous namespace

std::unique_ptr<cubical_value::CubicalValue> cubical_value::CubicalValue::parse(const std::string &json) {
  JsonLoc loc{json, 0};
  return loc.parse_value();
}

std::unique_ptr<cubical_value::CubicalValue> cubical_value::parse_json() const {
  return CubicalValue::parse(json_);
}
