#pragma once

#include <QWidget>

namespace GuiManager {

namespace Widgets {

enum class ContainerType {
  Unknown = 0,
  Home,
  PleaseWait,
  Settings,

  last
};

class ContainerWidget : public QWidget {
  Q_OBJECT
public:
  explicit ContainerWidget(ContainerType type, QWidget *parent = nullptr);
  virtual ~ContainerWidget();

  virtual ContainerType type() const { return _type; }

signals:

private:
  ContainerType _type;
};

} // namespace Widgets
} // namespace GuiManager
