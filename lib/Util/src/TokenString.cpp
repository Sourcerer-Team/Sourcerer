#include "TokenString.h"

#include <sstream>

TokenString::TokenString(const std::string &str, char separator)
    : _str(str), _separator(separator) {
  reset();
}

TokenString::~TokenString() {}

void TokenString::add(const std::string &token) {
  std::ostringstream oss;
  oss << _str;

  if (!_str.empty()) {
    oss << _separator;
  }

  oss << token;

  _str = oss.str();
}

void TokenString::addEmpty() {
  std::ostringstream oss;
  oss << _str << _separator;
  _str = oss.str();
}

bool TokenString::hasNext() const { return _str.length() > _currentIndex; }

std::string TokenString::getNextToken() {
  const size_t start = _currentIndex;
  size_t pos = start;

  bool foundSeparator = false;

  while (pos < _str.length()) {
    if (_str[pos] == _separator) {
      foundSeparator = true;
      break;
    }

    ++pos;
  }

  if (foundSeparator) {
    _currentIndex = pos + 1;
  } else {
    _currentIndex = pos;
  }

  return _str.substr(start, pos - start);
}

const std::string &TokenString::getString() const { return _str; }

void TokenString::reset() { _currentIndex = 0; }
