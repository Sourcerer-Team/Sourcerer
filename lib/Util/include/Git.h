// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <QString>

namespace Util {

class Git {
public:
  static QString parseRepoName(const QString &url);

private:
  Git() = delete;
};

} // namespace Util
