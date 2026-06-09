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
