#pragma once

#include <string>

/// Format a compiler error message with optional source location.
/// When line <= 0, the location is omitted (location unknown).
/// Output format:
///   error: <message>
///    --> <file>:<line>:<col>
inline std::string error_at(const std::string &file, int line, int col, const std::string &msg) {
  if (line <= 0)
    return "error: " + msg;
  return "error: " + msg + "\n --> " + file + ":" + std::to_string(line) + ":" + std::to_string(col);
}
