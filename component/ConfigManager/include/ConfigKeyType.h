// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

namespace ConfigManager {

enum class ConfigKeyType {
  Unknown = 0,

  Boolean,
  Color,
  DateTime,
  Long,
  String,

  last
};

}
