#include "TokenString.h"

#include <sstream>

namespace Util {

TokenString::TokenString(const QString &str, QChar separator)
    : _str(str), _separator(separator) {
  reset();
}

TokenString::~TokenString() {}

void TokenString::add(const QString &token) {
  QString builder = _str;
  if (!_str.isEmpty())
    builder += _separator;

  builder += token;

  _str = builder;
}

void TokenString::addEmpty() {
  QString builder = _str + _separator;
  _str = builder;
}

bool TokenString::hasNext() const {
  return static_cast<size_t>(_str.length()) > _currentIndex;
}

QString TokenString::getNextToken() {
  const size_t start = _currentIndex;
  size_t pos = start;

  bool foundSeparator = false;

  while (pos < static_cast<size_t>(_str.length())) {
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

  return _str.sliced(start, pos - start);
}

const QString &TokenString::getString() const {
  return _str;
}

void TokenString::reset() {
  _currentIndex = 0;
}

} // namespace Util
