#pragma once

#include <QString>

#include <spdlog/fmt/fmt.h>

template <> struct fmt::formatter<QString> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && (*it == 'f' || *it == 'e'))
      it++;
    if (it != end && *it != '}')
      throw format_error("invalid format");
    return it;
  }

  template <typename FormatContext>
  auto format(const QString &str, FormatContext &ctx) const
      -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{}", str.toStdString());
  }
};
