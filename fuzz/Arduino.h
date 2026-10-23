// Minimal Arduino String shim for host fuzzing. RequestValidation.h only needs
// String plus the standard library, so no Arduino toolchain is required.
#pragma once

#include <cctype>
#include <string>

class String : public std::string
{
public:
  using std::string::string;
  String(const std::string &value) : std::string(value) {}

  bool isEmpty() const { return empty(); }

  int indexOf(const char *needle) const
  {
    const auto position = find(needle);
    return position == npos ? -1 : static_cast<int>(position);
  }

  int indexOf(char needle) const
  {
    const auto position = find(needle);
    return position == npos ? -1 : static_cast<int>(position);
  }

  int lastIndexOf(char needle) const
  {
    const auto position = rfind(needle);
    return position == npos ? -1 : static_cast<int>(position);
  }

  String substring(size_t start) const { return substr(start); }
  String substring(size_t start, size_t count) const { return substr(start, count); }

  void toLowerCase()
  {
    for (auto &character : *this) character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  }

  bool endsWith(const char *suffix) const
  {
    const String tail(suffix);
    return size() >= tail.size() && compare(size() - tail.size(), tail.size(), tail) == 0;
  }

  void trim()
  {
    size_t begin = 0;
    size_t end = size();
    while (begin < end && std::isspace(static_cast<unsigned char>((*this)[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>((*this)[end - 1]))) --end;
    assign(substr(begin, end - begin));
  }
};
