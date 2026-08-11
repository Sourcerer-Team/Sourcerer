#include "widgets/TopBarWidget.h"
#include "widgets/TitleBarButton.h"

#include <ConfigKey.h>
#include <Log.h>
#include <StylesheetManager.h>

#include <QEvent>
#include <QHBoxLayout>
#include <QWindow>

#define THREAD_ID "TopBarWidget"

namespace GuiManager::Widgets {

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

  auto minimizeButton =
      new TitleBarButton(StylesheetManager::StylesheetManager::recolorPixmap(
                             QPixmap(":/icons/minimize_button.png"), iconColor),
                         this);
  minimizeButton->setObjectName("titleBarMinimizeButton");
  minimizeButton->setProperty("iconSource",
                              QStringLiteral(":/icons/minimize_button.png"));
  minimizeButton->setFlat(true);
  minimizeButton->setMinimumSize(QSize(32, 32));
  minimizeButton->setMaximumSize(QSize(32, 32));

  auto maximizeButton =
      new TitleBarButton(StylesheetManager::StylesheetManager::recolorPixmap(
                             QPixmap(":/icons/maximize_button.png"), iconColor),
                         this);
  maximizeButton->setObjectName("titleBarMaximizeButton");
  maximizeButton->setProperty("iconSource",
                              QStringLiteral(":/icons/maximize_button.png"));
  maximizeButton->setFlat(true);
  maximizeButton->setMinimumSize(QSize(32, 32));
  maximizeButton->setMaximumSize(QSize(32, 32));

  auto closeButton =
      new TitleBarButton(StylesheetManager::StylesheetManager::recolorPixmap(
                             QPixmap(":/icons/close_button.png"), iconColor),
                         this);
  closeButton->setObjectName("titleBarCloseButton");
  closeButton->setProperty("iconSource",
                           QStringLiteral(":/icons/close_button.png"));
  closeButton->setFlat(true);
  closeButton->setMinimumSize(QSize(32, 32));
  closeButton->setMaximumSize(QSize(32, 32));

  auto systemBarLayout = new QHBoxLayout();
  systemBarLayout->setContentsMargins(0, 0, 0, 0);
  systemBarLayout->setSizeConstraint(QLayout::SetFixedSize);
  systemBarLayout->addWidget(_profileButton);
  systemBarLayout->addWidget(minimizeButton);
  systemBarLayout->addWidget(maximizeButton);
  systemBarLayout->addWidget(closeButton);

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
  titleBarLayout->addWidget(_titleBarWidget);
  titleBarLayout->addSpacerItem(
      new QSpacerItem(QSizePolicy::Expanding, QSizePolicy::Fixed));
  titleBarLayout->addLayout(systemBarLayout);

  connect(_profileButton, &QPushButton::clicked, this,
          &TopBarWidget::onProfileClicked);
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
  connect(_titleBarWidget, &TitlebarWidget::connectToAzureIssueTracker, this,
          &TopBarWidget::connectToAzureIssueTracker);
  connect(_titleBarWidget, &TitlebarWidget::connectToBitbucketIssueTracker,
          this, &TopBarWidget::connectToBitbucketIssueTracker);
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
  connect(_titleBarWidget, &TitlebarWidget::about, this, &TopBarWidget::about);
  connect(_titleBarWidget, &TitlebarWidget::closedTab, this,
          &TopBarWidget::closedTab);
  connect(_titleBarWidget, &TitlebarWidget::selectedTab, this,
          &TopBarWidget::selectedTab);
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
