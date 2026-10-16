#include "ConfigGroup.h"

namespace ConfigManager {

ConfigItem::ConfigItem() : _orderNumber(-1), _type(ConfigKeyType::Unknown) {}

ConfigGroup::ConfigGroup(const QString &name) : _name(name) {}

void ConfigGroup::merge(const ConfigGroup &group) {
  if (name() == group.name()) {
    _items += group.items();

    std::sort(_items.begin(), _items.end(),
              [](const ConfigItem &x, const ConfigItem &y) {
                return x.orderNumber() < y.orderNumber();
              });

    for (const auto &s0 : group.subgroups()) {
      bool merged = false;
      for (auto &s1 : _subgroups) {
        if (s0.name() == s1.name()) {
          s1.merge(s0);
          merged = true;
          break;
        }
      }

      if (!merged)
        _subgroups += s0;
    }

    std::sort(_subgroups.begin(), _subgroups.end(),
              [](const ConfigGroup &x, const ConfigGroup &y) {
                return x.name() < y.name();
              });
  } else {
    bool merged = false;
    for (auto &s1 : _subgroups) {
      if (group.name() == s1.name()) {
        s1.merge(group);
        merged = true;
        break;
      }
    }

    if (!merged)
      _subgroups += group;

    std::sort(_subgroups.begin(), _subgroups.end(),
              [](const ConfigGroup &x, const ConfigGroup &y) {
                return x.name() < y.name();
              });
  }
}

} // namespace ConfigManager
