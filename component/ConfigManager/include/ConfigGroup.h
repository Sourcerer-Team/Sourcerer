#pragma once

#include "ConfigKeyType.h"

#include <QString>
#include <QVariant>
#include <QVector>

namespace ConfigManager {

class ConfigItem {
public:
  ConfigItem();

  const QString &description() const { return _description; }
  const QString &key() const { return _key; }
  const QString &name() const { return _name; }
  int orderNumber() const { return _orderNumber; }
  ConfigKeyType type() const { return _type; }
  const QVariant &value() const { return _value; }

  void setDescription(const QString &description) {
    _description = description;
  }
  void setKey(const QString &key) { _key = key; }
  void setName(const QString &name) { _name = name; }
  void setOrderNumber(int orderNumber) { _orderNumber = orderNumber; }
  void setType(ConfigKeyType type) { _type = type; }
  void setValue(const QVariant &value) { _value = value; }

private:
  ConfigKeyType _type;
  QVariant _value;
  QString _name;
  QString _description;
  QString _key;
  int _orderNumber;
};

class ConfigGroup {
public:
  ConfigGroup(const QString &name = "");

  const QVector<ConfigGroup> &subgroups() const { return _subgroups; }
  const QVector<ConfigItem> &items() const { return _items; }
  const QString &name() const { return _name; }

  void addSubgroup(const ConfigGroup &subgroup) { _subgroups += subgroup; }
  void addItem(const ConfigItem &item) { _items += item; }
  void setName(const QString &name) { _name = name; }

  void merge(const ConfigGroup &group);
  bool isEmpty() const { return _subgroups.isEmpty() && _items.isEmpty(); }

private:
  QString _name;
  QVector<ConfigGroup> _subgroups;
  QVector<ConfigItem> _items;
};

} // namespace ConfigManager
