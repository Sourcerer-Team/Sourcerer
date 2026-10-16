#pragma once

#include <ConfigGroup.h>

#include <QHoverEvent>
#include <QWidget>

namespace GuiManager {

namespace Widgets {

class SettingsPaneItemWidget : public QWidget {
  Q_OBJECT
  Q_PROPERTY(QColor hoverColor READ hoverColor WRITE setHoverColor NOTIFY
                 hoverColorChanged)
public:
  explicit SettingsPaneItemWidget(const ConfigManager::ConfigItem &item,
                                  QWidget *parent = nullptr);

  const ConfigManager::ConfigItem &item() const { return _item; }
  ConfigManager::ConfigItem &item() { return _item; }

  virtual void inflate();

  // QObject interface
public:
  bool eventFilter(QObject *watched, QEvent *event) override;

signals:
  void hoverColorChanged();

protected:
  virtual QWidget *createWidget() = 0;
  virtual void load();
  virtual void save() const;

private:
  bool hover(QHoverEvent *event, bool entered);
  QColor hoverColor() const { return _hoverColor; }
  void setHoverColor(const QColor &color) { _hoverColor = color; }

private:
  QColor _hoverColor;
  ConfigManager::ConfigItem _item;
};

} // namespace Widgets
} // namespace GuiManager
