#include "widgets/TopBarWidget.h"
#include "widgets/TitleBarButton.h"

#include <ConfigKey.h>
#include <Log.h>
#include <StylesheetManager.h>

#include <QColor>
#include <QEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QRectF>
#include <QStringList>
#include <QWindow>

#define THREAD_ID "TopBarWidget"

namespace GuiManager::Widgets {

#ifdef Q_OS_MACOS
namespace {

enum class MacWindowButtonRole { Close, Minimize, Zoom };

QPixmap macWindowButtonPixmap(MacWindowButtonRole role) {
  QPixmap pixmap(14, 14);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  QColor circleColor;
  switch (role) {
  case MacWindowButtonRole::Close:
    circleColor = QColor(QStringLiteral("#FF5F57"));
    break;
  case MacWindowButtonRole::Minimize:
    circleColor = QColor(QStringLiteral("#FEBC2E"));
    break;
  case MacWindowButtonRole::Zoom:
    circleColor = QColor(QStringLiteral("#28C840"));
    break;
  }

  painter.setPen(Qt::NoPen);
  painter.setBrush(circleColor);
  painter.drawEllipse(QRectF(1.5, 1.5, 11.0, 11.0));

  return pixmap;
}

TitleBarButton *macWindowButton(MacWindowButtonRole role, QWidget *parent,
                                const QString &objectName,
                                const QString &tooltip) {
  auto *button = new TitleBarButton(macWindowButtonPixmap(role), parent);
  button->setObjectName(objectName);
  button->setToolTip(tooltip);
  button->setProperty("iconSource", QStringLiteral("macWindowButton"));
  button->setFlat(true);
  button->setStyleSheet(
      QStringLiteral("QPushButton { background-color: transparent; border: "
                     "none; padding: 0; margin: 0; }"
                     "QPushButton:hover { background-color: transparent; }"
                     "QPushButton:pressed { background-color: transparent; }"));
  button->setMinimumSize(QSize(15, 15));
  button->setMaximumSize(QSize(15, 15));
  button->setIconSize(QSize(14, 14));
  return button;
}

} // namespace
#endif

static bool isTilingWindowManager() {
#ifdef Q_OS_LINUX
  // Check standard XDG desktop or session variables
  const QString desktop = qgetenv("XDG_CURRENT_DESKTOP").toLower();
  const QString session = qgetenv("DESKTOP_SESSION").toLower();

  // List of common tiling window managers / compositors
  const QStringList tilingWms = {"i3",    "sway",    "bspwm", "hyprland",
                                 "qtile", "awesome", "dwm"};

  for (const QString &wm : tilingWms) {
    if (desktop.contains(wm) || session.contains(wm)) {
      return true;
    }
  }
#endif

  return false;
}

TopBarWidget::TopBarWidget(QWidget *parent)
    : QWidget{parent},
      _configManager(ConfigManager::ConfigManager::getInstance()),
      _titleBarWidget(new TitlebarWidget), _profileButton(nullptr) {
  LOG_PERF(0, "Enter");

  setMouseTracking(true);
  const QColor iconColor =
      StylesheetManager::StylesheetManager::menuBarButtonIconColor();

  _profileButton =
      new TitleBarButton(StylesheetManager::StylesheetManager::recolorPixmap(
                             QPixmap(":/icons/profile.png"), iconColor),
                         this);
  _profileButton->setObjectName("titleBarProfileButton");
  _profileButton->setProperty("iconSource",
                              QStringLiteral(":/icons/profile.png"));
  _profileButton->setFlat(true);
  _profileButton->setMinimumSize(QSize(32, 32));
  _profileButton->setMaximumSize(QSize(32, 32));

  QPushButton *settingsButton =
      new TitleBarButton(StylesheetManager::StylesheetManager::recolorPixmap(
                             QPixmap(":/icons/settings.png"), iconColor),
                         this);
  settingsButton->setObjectName("titleBarSettingsButton");
  settingsButton->setProperty("iconSource",
                              QStringLiteral(":/icons/settings.png"));
  settingsButton->setFlat(true);
  settingsButton->setMinimumSize(QSize(32, 32));
  settingsButton->setMaximumSize(QSize(32, 32));

  const bool tilingWm = isTilingWindowManager();

  QPushButton *minimizeButton = nullptr;
  QPushButton *maximizeButton = nullptr;
  QPushButton *closeButton = nullptr;
#ifdef Q_OS_MACOS
  minimizeButton =
      macWindowButton(MacWindowButtonRole::Minimize, this,
                      QStringLiteral("titleBarMinimizeButton"), tr("Minimize"));
  maximizeButton =
      macWindowButton(MacWindowButtonRole::Zoom, this,
                      QStringLiteral("titleBarMaximizeButton"), tr("Maximize"));
  closeButton =
      macWindowButton(MacWindowButtonRole::Close, this,
                      QStringLiteral("titleBarCloseButton"), tr("Close"));
#else
  if (!tilingWm) {
    minimizeButton = new TitleBarButton(
        StylesheetManager::StylesheetManager::recolorPixmap(
            QPixmap(":/icons/minimize_button.png"), iconColor),
        this);
    minimizeButton->setObjectName("titleBarMinimizeButton");
    minimizeButton->setProperty("iconSource",
                                QStringLiteral(":/icons/minimize_button.png"));
    minimizeButton->setFlat(true);
    minimizeButton->setMinimumSize(QSize(32, 32));
    minimizeButton->setMaximumSize(QSize(32, 32));
  }

  if (!tilingWm) {
    maximizeButton = new TitleBarButton(
        StylesheetManager::StylesheetManager::recolorPixmap(
            QPixmap(":/icons/maximize_button.png"), iconColor),
        this);
    maximizeButton->setObjectName("titleBarMaximizeButton");
    maximizeButton->setProperty("iconSource",
                                QStringLiteral(":/icons/maximize_button.png"));
    maximizeButton->setFlat(true);
    maximizeButton->setMinimumSize(QSize(32, 32));
    maximizeButton->setMaximumSize(QSize(32, 32));
  }

  closeButton =
      new TitleBarButton(StylesheetManager::StylesheetManager::recolorPixmap(
                             QPixmap(":/icons/close_button.png"), iconColor),
                         this);
  closeButton->setObjectName("titleBarCloseButton");
  closeButton->setProperty("iconSource",
                           QStringLiteral(":/icons/close_button.png"));
  closeButton->setFlat(true);
  closeButton->setMinimumSize(QSize(32, 32));
  closeButton->setMaximumSize(QSize(32, 32));
#endif

  auto systemBarLayout = new QHBoxLayout();
  systemBarLayout->setContentsMargins(7, 1, 0, 0);
  systemBarLayout->setSpacing(6);
  systemBarLayout->setSizeConstraint(QLayout::SetFixedSize);

#ifdef Q_OS_MACOS
  if (closeButton) {
    systemBarLayout->addWidget(closeButton);
  }
  if (minimizeButton) {
    systemBarLayout->addWidget(minimizeButton);
  }
  if (maximizeButton) {
    systemBarLayout->addWidget(maximizeButton);
  }
#else
  systemBarLayout->addWidget(_profileButton);

  systemBarLayout->addWidget(settingsButton);

  if (!tilingWm) {
    systemBarLayout->addWidget(minimizeButton);
    systemBarLayout->addWidget(maximizeButton);
  }

  systemBarLayout->addWidget(closeButton);
#endif

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);

  LOG_DEBUG("TitleBar height is {} pixels.", titleBarHeight);

  setMinimumHeight(titleBarHeight);
  setMaximumHeight(titleBarHeight);

  const qlonglong borderWidth =
      _configManager.getLong(ConfigManager::ConfigKey::MainWindowBorderWidth);

  LOG_DEBUG("Border width is {} pixels.", borderWidth);

  auto titleBarLayout = new QHBoxLayout(this);
  titleBarLayout->setContentsMargins(borderWidth, borderWidth, borderWidth, 0);
#ifdef Q_OS_MACOS
  titleBarLayout->addLayout(systemBarLayout);
  titleBarLayout->addWidget(_titleBarWidget);
  titleBarLayout->addSpacerItem(
      new QSpacerItem(QSizePolicy::Expanding, QSizePolicy::Fixed));
  auto *rightControlsLayout = new QHBoxLayout();
  rightControlsLayout->setContentsMargins(0, 0, 0, 0);
  rightControlsLayout->setSpacing(0);
  rightControlsLayout->setSizeConstraint(QLayout::SetFixedSize);
  rightControlsLayout->addWidget(_profileButton);
  rightControlsLayout->addWidget(settingsButton);
  titleBarLayout->addLayout(rightControlsLayout);
#else
  titleBarLayout->addWidget(_titleBarWidget);
  titleBarLayout->addSpacerItem(
      new QSpacerItem(QSizePolicy::Expanding, QSizePolicy::Fixed));
  titleBarLayout->addLayout(systemBarLayout);
#endif

  connect(_profileButton, &QPushButton::clicked, this,
          &TopBarWidget::onProfileClicked);
  connect(settingsButton, &QPushButton::clicked, this, &TopBarWidget::settings);
  connect(minimizeButton, &QPushButton::clicked, this,
          &TopBarWidget::minimized);
  connect(maximizeButton, &QPushButton::clicked, this,
          &TopBarWidget::maximized);
  connect(closeButton, &QPushButton::clicked, this, &TopBarWidget::closed);

  connect(_titleBarWidget, &TitlebarWidget::open, this, &TopBarWidget::open);
  connect(_titleBarWidget, &TitlebarWidget::clone, this, &TopBarWidget::clone);
  connect(_titleBarWidget, &TitlebarWidget::init, this, &TopBarWidget::init);
  connect(_titleBarWidget, &TitlebarWidget::saveWorkspace, this,
          &TopBarWidget::saveWorkspace);
  connect(_titleBarWidget, &TitlebarWidget::connectToGitHubIssueTracker, this,
          &TopBarWidget::connectToGitHubIssueTracker);
  connect(_titleBarWidget, &TitlebarWidget::connectToGitLabIssueTracker, this,
          &TopBarWidget::connectToGitLabIssueTracker);
  connect(_titleBarWidget, &TitlebarWidget::connectToAzureIssueTracker, this,
          &TopBarWidget::connectToAzureIssueTracker);
  connect(_titleBarWidget, &TitlebarWidget::connectToBitbucketIssueTracker,
          this, &TopBarWidget::connectToBitbucketIssueTracker);
  connect(_titleBarWidget, &TitlebarWidget::connectToCustomIssueTracker, this,
          &TopBarWidget::connectToCustomIssueTracker);
  connect(_titleBarWidget, &TitlebarWidget::settings, this,
          &TopBarWidget::settings);
  connect(_titleBarWidget, &TitlebarWidget::credentials, this,
          &TopBarWidget::credentials);
  connect(_titleBarWidget, &TitlebarWidget::sshKeys, this,
          &TopBarWidget::sshKeys);
  connect(_titleBarWidget, &TitlebarWidget::terminal, this,
          &TopBarWidget::terminal);
  connect(_titleBarWidget, &TitlebarWidget::bugReport, this,
          &TopBarWidget::bugReport);
  connect(_titleBarWidget, &TitlebarWidget::releaseNotes, this,
          &TopBarWidget::releaseNotes);
  connect(_titleBarWidget, &TitlebarWidget::about, this, &TopBarWidget::about);
  connect(_titleBarWidget, &TitlebarWidget::themeChanged, this,
          &TopBarWidget::themeChanged);
  connect(_titleBarWidget, &TitlebarWidget::closedTab, this,
          &TopBarWidget::closedTab);
  connect(_titleBarWidget, &TitlebarWidget::selectedTab, this,
          &TopBarWidget::selectedTab);
  connect(_titleBarWidget, &TitlebarWidget::movedTab, this,
          &TopBarWidget::movedTab);
  connect(_titleBarWidget, &TitlebarWidget::resizeTabBar, this,
          &TopBarWidget::resizeTabBar);

  LOG_PERF(0, "Exit");
}

void TopBarWidget::onProfileClicked() {
  LOG_PERF(0, "Enter");

  emit profile(
      _profileButton->mapToGlobal(QPoint(_profileButton->width() / 2, 0)).x());

  LOG_PERF(0, "Exit");
}

void TopBarWidget::mouseDoubleClickEvent(QMouseEvent *event) {
  LOG_PERF(0, "Enter");

  if (event->button() == Qt::LeftButton) {
    emit doubleClicked();
    LOG_PERF(0, "Exit");
    return;
  }

  QWidget::mouseDoubleClickEvent(event);

  LOG_PERF(0, "Exit");
}

void TopBarWidget::mousePressEvent(QMouseEvent *event) {
  LOG_PERF(0, "Enter");

  if (event->button() == Qt::LeftButton) {
    const qlonglong borderWidth =
        _configManager.getLong(ConfigManager::ConfigKey::MainWindowBorderWidth);
    if (event->pos().y() <= borderWidth) {
      QWidget *parentWindow = window();
      if (parentWindow && parentWindow->windowHandle()) {
        Qt::Edges edges = Qt::TopEdge;
        if (event->pos().x() <= borderWidth) {
          edges |= Qt::LeftEdge;
        } else if (event->pos().x() >= width() - borderWidth) {
          edges |= Qt::RightEdge;
        }
        parentWindow->windowHandle()->startSystemResize(edges);
      }
      LOG_PERF(0, "Exit");
      return;
    }

    emit dragged();
    LOG_PERF(0, "Exit");
    return;
  }

  QWidget::mousePressEvent(event);

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
