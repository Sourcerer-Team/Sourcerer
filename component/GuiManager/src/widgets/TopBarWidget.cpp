#include "widgets/TopBarWidget.h"
#include "widgets/TitleBarButton.h"

#include <ConfigKey.h>
#include <Log.h>

#include <QEvent>
#include <QHBoxLayout>

#define THREAD_ID "TopBarWidget"

namespace GuiManager::Widgets {

TopBarWidget::TopBarWidget(QWidget *parent)
    : QWidget{parent},
      _configManager(ConfigManager::ConfigManager::getInstance()),
      _titleBarWidget(new TitlebarWidget), _profileButton(nullptr) {
  LOG_PERF(0, "Enter");

  setMouseTracking(true);

  const QColor minimizeButtonColor = _configManager.getColor(
      ConfigManager::ConfigKey::TitleBarMinimizeButtonColor);
  const QColor maximizeButtonColor = _configManager.getColor(
      ConfigManager::ConfigKey::TitleBarMaximizeButtonColor);
  const QColor closeButtonColor = _configManager.getColor(
      ConfigManager::ConfigKey::TitleBarCloseButtonColor);

  LOG_DEBUG("Minimize button color '{}", minimizeButtonColor.name());
  LOG_DEBUG("Maximize button color '{}", maximizeButtonColor.name());
  LOG_DEBUG("Close button color '{}", closeButtonColor.name());

  _profileButton = new TitleBarButton(QPixmap(":/icons/profile.png"),
                                      minimizeButtonColor, this);
  _profileButton->setFlat(true);
  _profileButton->setMinimumSize(QSize(32, 32));
  _profileButton->setMaximumSize(QSize(32, 32));

  auto minimizeButton = new TitleBarButton(
      QPixmap(":/icons/minimize_button.png"), minimizeButtonColor, this);
  minimizeButton->setFlat(true);
  minimizeButton->setMinimumSize(QSize(32, 32));
  minimizeButton->setMaximumSize(QSize(32, 32));

  auto maximizeButton = new TitleBarButton(
      QPixmap(":/icons/maximize_button.png"), maximizeButtonColor, this);
  maximizeButton->setFlat(true);
  maximizeButton->setMinimumSize(QSize(32, 32));
  maximizeButton->setMaximumSize(QSize(32, 32));

  auto closeButton = new TitleBarButton(QPixmap(":/icons/close_button.png"),
                                        closeButtonColor, this);
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
  connect(_titleBarWidget, &TitlebarWidget::settings, this,
          &TopBarWidget::settings);
  connect(_titleBarWidget, &TitlebarWidget::credentials, this,
          &TopBarWidget::credentials);
  connect(_titleBarWidget, &TitlebarWidget::terminal, this,
          &TopBarWidget::terminal);
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

  emit profile(_profileButton->x() + _profileButton->width() / 2);

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
    emit dragged();
    LOG_PERF(0, "Exit");
    return;
  }

  QWidget::mousePressEvent(event);

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
