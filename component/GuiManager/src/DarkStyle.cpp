#include "DarkStyle.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QStyleFactory>

#ifdef Q_OS_WIN

#ifdef NTDDI_VERSION
#undef NTDDI_VERSION
#endif

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif

#define NTDDI_VERSION NTDDI_VISTA
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <windows.h>
#include <shellapi.h>

#include <QLibrary>

namespace StyleCommon {

QPixmap winStandardPixmap(QStyle::StandardPixmap standardPixmap) {
  QPixmap pixmap;

  HICON hicon = nullptr;

  LPCWSTR icon_handle_xp = nullptr;
  SHSTOCKICONID icon_handle_vista = SIID_INVALID;

  switch (standardPixmap) {
  case QStyle::SP_MessageBoxInformation: {
    icon_handle_xp = IDI_INFORMATION;
    icon_handle_vista = SIID_INFO;
    break;
  }
  case QStyle::SP_MessageBoxWarning: {
    icon_handle_xp = IDI_WARNING;
    icon_handle_vista = SIID_WARNING;
    break;
  }
  case QStyle::SP_MessageBoxCritical: {
    icon_handle_xp = IDI_ERROR;
    icon_handle_vista = SIID_ERROR;
    break;
  }
  case QStyle::SP_MessageBoxQuestion: {
    icon_handle_xp = IDI_QUESTION;
    icon_handle_vista = SIID_HELP;
    break;
  }
  default:
    break;
  }

  typedef HRESULT(STDAPICALLTYPE * tSHGetStockIconInfo)(
      SHSTOCKICONID siid, UINT uFlags, SHSTOCKICONINFO * psii);
  const auto pSHGetStockIconInfo = reinterpret_cast<tSHGetStockIconInfo>(
      QLibrary::resolve("shell32", "SHGetStockIconInfo"));

  if (pSHGetStockIconInfo) {
    if (icon_handle_vista != SIID_INVALID) {
      SHSTOCKICONINFO sii = {};
      sii.cbSize = sizeof(sii);

      if (SUCCEEDED(pSHGetStockIconInfo(icon_handle_vista,
                                        SHGSI_ICON | SHGSI_LARGEICON, &sii))) {
        hicon = sii.hIcon;
      }
    }
  } else {
    if (icon_handle_xp) {
      hicon = LoadIconW(nullptr, icon_handle_xp);
    }
  }

  if (hicon) {
    pixmap = QPixmap::fromImage(QImage::fromHICON(hicon));
    DestroyIcon(hicon);
  }

  return pixmap;
}

} // namespace StyleCommon

#endif

DarkStyle::DarkStyle() : DarkStyle(styleBase()) {
#ifdef Q_OS_WIN
  _pixmapCache[SP_MessageBoxInformation] =
      StyleCommon::winStandardPixmap(SP_MessageBoxInformation);
  _pixmapCache[SP_MessageBoxWarning] =
      StyleCommon::winStandardPixmap(SP_MessageBoxWarning);
  _pixmapCache[SP_MessageBoxCritical] =
      StyleCommon::winStandardPixmap(SP_MessageBoxCritical);
  _pixmapCache[SP_MessageBoxQuestion] =
      StyleCommon::winStandardPixmap(SP_MessageBoxQuestion);
#endif
}

DarkStyle::DarkStyle(QStyle *style) : QProxyStyle(style) {}

DarkStyle::~DarkStyle() {}

QStyle *DarkStyle::styleBase() {
  QStyle *base = QStyleFactory::create(QStringLiteral("Fusion"));
  return base;
}

QIcon DarkStyle::standardIcon(const StandardPixmap standardPixmap,
                              const QStyleOption *option,
                              const QWidget *widget) const {
#ifdef Q_OS_WIN
  switch (standardPixmap) {
  case SP_MessageBoxInformation:
  case SP_MessageBoxWarning:
  case SP_MessageBoxCritical:
  case SP_MessageBoxQuestion: {
    QPixmap pixmap = _pixmapCache.value(standardPixmap, QPixmap());

    if (!pixmap.isNull())
      return QIcon(pixmap);

    break;
  }
  default:
    break;
  }
#endif

  return QProxyStyle::standardIcon(standardPixmap, option, widget);
}

void DarkStyle::polish(QPalette &palette) {
  palette.setColor(QPalette::Window, QColor(53, 53, 53));
  palette.setColor(QPalette::WindowText, QColor(255, 255, 255));
  palette.setColor(QPalette::Disabled, QPalette::WindowText,
                   QColor(127, 127, 127));
  palette.setColor(QPalette::Base, QColor(42, 42, 42));
  palette.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
  palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
  palette.setColor(QPalette::ToolTipText, QColor(255, 255, 255));
  palette.setColor(QPalette::Text, QColor(255, 255, 255));
  palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
  palette.setColor(QPalette::Dark, QColor(35, 35, 35));
  palette.setColor(QPalette::Shadow, QColor(20, 20, 20));
  palette.setColor(QPalette::Button, QColor(53, 53, 53));
  palette.setColor(QPalette::ButtonText, QColor(255, 255, 255));
  palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                   QColor(127, 127, 127));
  palette.setColor(QPalette::BrightText, QColor(255, 0, 0));
  palette.setColor(QPalette::Link, QColor(42, 130, 218));
  palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
  palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
  palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
  palette.setColor(QPalette::Disabled, QPalette::HighlightedText,
                   QColor(127, 127, 127));
}

void DarkStyle::polish(QApplication *app) {
  if (!app)
    return;

  QFont defaultFont = app->font();
  defaultFont.setPointSize(defaultFont.pointSize() + 2);
  app->setFont(defaultFont);

  if (QFile file(QStringLiteral(":/darkstyle.qss"));
      file.open(QFile::ReadOnly | QFile::Text)) {
    const QString style_sheet = QLatin1String(file.readAll());
    app->setStyleSheet(style_sheet);
    file.close();
  }
}

void DarkStyle::unpolish(QApplication *app) {
  if (!app)
    return;

  QFont defaultFont = app->font();
  defaultFont.setPointSize(defaultFont.pointSize() - 2);
  app->setFont(defaultFont);
}

QStyle *DarkStyle::baseStyle() {
  return styleBase();
}
