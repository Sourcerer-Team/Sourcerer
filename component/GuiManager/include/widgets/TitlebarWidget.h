#pragma once

#include <ConfigManager.h>

#include <QWidget>

namespace Ui {
class TitlebarWidget;
}

namespace GuiManager {

namespace Widgets {

class TitlebarWidget : public QWidget {
  Q_OBJECT

public:
  explicit TitlebarWidget(QWidget *parent = nullptr);
  ~TitlebarWidget();

  void addTab(const QString &text);

signals:
  void home();
  void open();
  void clone();
  void init();
  void settings();
  void about();

private slots:
  void onHome();
  void onOpen();
  void onClone();
  void onInit();
  void onSettings();
  void onAbout();

private:
  Ui::TitlebarWidget *ui;
  ConfigManager::ConfigManager &_configManager;
};

} // namespace Widgets
} // namespace GuiManager
