#pragma once

#include <QHash>
#include <QObject>
#include <QProxyStyle>

class DarkStyle : public QProxyStyle {
  Q_OBJECT
public:
  explicit DarkStyle();

  DarkStyle(QStyle *style);

  ~DarkStyle();

  static QStyle *styleBase();

  QIcon standardIcon(StandardPixmap standardPixmap, const QStyleOption *option,
                     const QWidget *widget) const override;

  void polish(QPalette &palette) override;

  void polish(QApplication *app) override;

  void unpolish(QApplication *app) override;

private:
  static QStyle *baseStyle();

private:
#ifdef Q_OS_WIN
  QHash<StandardPixmap, QPixmap> _pixmapCache;
#endif
};
