#pragma once

#include <QWidget>

namespace GuiManager::Widgets {

class BlurredWidget : public QWidget {
  Q_OBJECT
public:
  explicit BlurredWidget(QWidget *parent = nullptr);

protected:
  void showEvent(QShowEvent *event) override;

  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void enablePlatformBlur();

#ifdef Q_OS_WIN
  void enableWindowsBlur() const;
#elif defined(Q_OS_MAC)
  void enableMacBlur() const;
#elif defined(Q_OS_LINUX)
  void enableKDEBlur() const;
#endif
};

} // namespace GuiManager::Widgets
