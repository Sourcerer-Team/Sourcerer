#pragma once

#include <QString>

namespace Util {

class TokenString {
public:
  explicit TokenString(const QString &str = "", QChar separator = ',');
  virtual ~TokenString();

  void add(const QString &token);
  void addEmpty();

  bool hasNext() const;

  QString getNextToken();

  const QString &getString() const;

  void reset();

private:
  QString _str;
  QChar _separator;
  size_t _currentIndex;
};

} // namespace Util
