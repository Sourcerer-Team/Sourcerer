#pragma once

#include <ConfigManager.h>

#include <QWidget>

namespace Ui {
class TitlebarWidget;
}

namespace GuiManager {

class TitlebarWidget : public QWidget {
  Q_OBJECT

public:
  explicit TitlebarWidget(QWidget *parent = nullptr);
  ~TitlebarWidget();

  void addTab(const QString& text);

private:
  Ui::TitlebarWidget *ui;
  ConfigManager::ConfigManager &_configManager;
};

} // namespace GuiManager
