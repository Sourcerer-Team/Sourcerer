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

  QStyle *styleBase() const;

  QIcon standardIcon(StandardPixmap standardPixmap, const QStyleOption *option,
                     const QWidget *widget) const;

  void polish(QPalette &palette);

  void polish(QApplication *app);

  void unpolish(QApplication *app);

private:
  QStyle *baseStyle() const;

private:
#ifdef Q_OS_WIN
  QHash<StandardPixmap, QPixmap> _pixmap_cache;
#endif
};
