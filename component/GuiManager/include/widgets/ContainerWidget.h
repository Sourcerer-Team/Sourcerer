#pragma once

#include <QWidget>

namespace GuiManager::Widgets {

enum class ContainerType {
  Unknown = 0,
  Clone,
  GitCredentials,
  GitKeys,
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

  virtual void deselected() {}

  virtual void selected() {}

  virtual void setupShortcuts() {}

  virtual QString path() const { return QString(); }

private:
  ContainerType _type;
};

} // namespace GuiManager::Widgets
