#pragma once

#include <QWidget>

namespace GuiManager::Widgets {

enum class ContainerType {
  Unknown = 0,
  Clone,
  GitRepo,
  Home,
  InitGitRepo,
  PleaseWait,
  Settings,
  last
};

class ContainerWidget : public QWidget {
  Q_OBJECT
public:
  explicit ContainerWidget(ContainerType type, QWidget *parent = nullptr);
  ~ContainerWidget() override;

  virtual ContainerType type() const { return _type; }

  virtual void init() {}

private:
  ContainerType _type;
};

} // namespace GuiManager::Widgets