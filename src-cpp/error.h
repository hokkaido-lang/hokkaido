#pragma once

#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>

// =========================================================================
// Hokkaido Diagnostics
// =========================================================================
//
// Renders errors like:
//
//   error: expected ';' after expression
//     --> main.hk:5:12
//     |
//   5 | let x = 10 + 20
//     |             ^^ expected ';'
//     |
//   help: try adding a semicolon

static inline bool hok_color_enabled() {
  // Disable color when output is not a terminal (e.g. piped)
  if (!isatty(fileno(stderr))) return false;
  const char *no_color = std::getenv("NO_COLOR");
  return !no_color || no_color[0] == '\0';
}

// ANSI color codes
namespace diag {
  struct Color {
    const char *code;
    bool enabled;
    Color(const char *c, bool e = hok_color_enabled()) : code(c), enabled(e) {}
  };

  inline std::ostream &operator<<(std::ostream &os, const Color &c) {
    if (c.enabled) os << c.code;
    return os;
  }

  // Static instances
  inline const char *red()    { return "\033[1;31m"; }
  inline const char *yellow() { return "\033[1;33m"; }
  inline const char *cyan()   { return "\033[1;36m"; }
  inline const char *green()  { return "\033[1;32m"; }
  inline const char *dim()    { return "\033[2m"; }
  inline const char *bold()   { return "\033[1m"; }
  inline const char *reset()  { return "\033[0m"; }
}

/// Extract a specific line from source text (1-indexed).
/// Returns empty string if line_num is out of range.
static inline std::string get_source_line(const std::string &source, int line_num) {
  if (line_num <= 0 || source.empty()) return "";
  int line = 1;
  size_t start = 0;
  for (size_t i = 0; i <= source.size(); i++) {
    if (i == source.size() || source[i] == '\n') {
      if (line == line_num) {
        // Trim trailing whitespace
        size_t end = i;
        while (end > start && (source[end - 1] == '\r' || source[end - 1] == ' ' || source[end - 1] == '\t'))
          end--;
        return source.substr(start, end - start);
      }
      start = i + 1;
      line++;
    }
  }
  return "";
}

/// Format a diagnostic message with source context.
///
/// Output format:
///   error: <message>
///     --> <file>:<line>:<col>
///     |
///  <N> | <source line>
///     |       ^^^ underline pointing at the error
///     |
///   help: <suggestion>  (if provided)
///
/// When source_text is empty, the source line and underline are omitted.
static inline std::string format_diagnostic(
    const std::string &level,     // "error", "warning", "note"
    const std::string &msg,
    const std::string &file,
    int line,
    int col,
    const std::string &source_text = "",
    const std::string &suggestion = "",
    int span_length = 1)
{
  bool color = hok_color_enabled();
  std::ostringstream os;

  // Header: "error: <message>"
  if (color)
    os << diag::red();
  os << level << ": " << msg;
  if (color)
    os << diag::reset();
  os << "\n";

  // Location: "  --> file:line:col"
  if (line > 0) {
    if (color)
      os << diag::cyan();
    os << "  --> " << file << ":" << line << ":" << col;
    if (color)
      os << diag::reset();
    os << "\n";
  }

  // Source line with underline
  if (line > 0 && !source_text.empty()) {
    std::string src_line = get_source_line(source_text, line);
    if (!src_line.empty()) {
      // Line number gutter width
      int gutter = std::to_string(line).size();

      if (color)
        os << diag::dim();
      os << " " << std::string(gutter, ' ') << " |";
      if (color)
        os << diag::reset();
      os << "\n";

      // Source line: " 5 | let x = 10 + 20"
      if (color)
        os << diag::dim();
      os << " " << line << " | " << src_line;
      if (color)
        os << diag::reset();
      os << "\n";

      // Underline: "   |       ^^^"
      if (color)
        os << diag::dim();
      os << " " << std::string(gutter, ' ') << " | ";
      if (color)
        os << diag::reset();

      // Pad to the error column (1-indexed)
      int underline_start = col - 1;
      if (underline_start < 0) underline_start = 0;
      if (underline_start > (int)src_line.size()) underline_start = (int)src_line.size();

      os << std::string(underline_start, ' ');

      // Underline span
      if (span_length <= 0) span_length = 1;
      if (color)
        os << diag::red();
      if (underline_start >= (int)src_line.size()) {
        // Column is past end of line — just show caret at end
        os << '^';
      } else {
        for (int i = 0; i < span_length && (underline_start + i) <= (int)src_line.size(); i++) {
          if ((underline_start + i) < (int)src_line.size()) {
            char c = src_line[underline_start + i];
            os << (c == '\t' ? '\t' : '^');
          } else {
            os << '^';
          }
        }
      }
      if (color)
        os << diag::reset();
      os << " " << level;
      if (!suggestion.empty()) {
        if (color)
          os << " " << diag::yellow();
        os << ": " << suggestion;
        if (color)
          os << diag::reset();
      }
      os << "\n";
    }
  }

  // Suggestion line (if no source line was shown, still display the suggestion)
  if (!suggestion.empty() && (line <= 0 || source_text.empty() || get_source_line(source_text, line).empty())) {
    if (color)
      os << diag::yellow();
    os << "   help: " << suggestion;
    if (color)
      os << diag::reset();
    os << "\n";
  }

  return os.str();
}

// =========================================================================
// Legacy API (backward compatible)
// =========================================================================

/// Format a compiler error message with optional source location.
/// When line <= 0, the location is omitted (location unknown).
/// Output format:
///   error: <message>
///    --> <file>:<line>:<col>
inline std::string error_at(const std::string &file, int line, int col, const std::string &msg) {
  return format_diagnostic("error", msg, file, line, col);
}

/// Format with source context and optional suggestion.
inline std::string error_at(const std::string &file, int line, int col,
                            const std::string &msg,
                            const std::string &source_text,
                            const std::string &suggestion = "",
                            int span_length = 1) {
  return format_diagnostic("error", msg, file, line, col, source_text, suggestion, span_length);
}
