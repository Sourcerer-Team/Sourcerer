#include "widgets/ProfileBoardWidget.h"
#include "delegates/FixedHeightDelegate.h"
#include "widgets/ui_ProfileBoardWidget.h"

#include <Log.h>

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#define THREAD_ID "ProfileBoardWidget"

namespace GuiManager::Widgets {

ProfileBoardWidget::ProfileBoardWidget(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint), ui(new Ui::ProfileBoardWidget),
      _arrowX(0), _currentlyEditProfileIndex(-1),
      _operation(OperationType::Unknown), _model(new QStandardItemModel),
      _userProfileManager(Common::UserProfileManager::instance()) {
  LOG_PERF(0, "Enter");

#ifdef Q_OS_WIN
  setWindowFlag(Qt::Tool);
#endif

  setAttribute(Qt::WA_TranslucentBackground);
  setMouseTracking(true);

  ui->setupUi(this);
  ui->listViewProfiles->setItemDelegate(
      new Delegates::FixedHeightDelegate(ui->listViewProfiles));

  layout()->setContentsMargins(15, 25, 15, 15); // Leave space for arrow

  qApp->installEventFilter(this);

  connect(ui->listViewProfiles, &QListView::clicked, this,
          &ProfileBoardWidget::onListItemClicked);
  connect(ui->pushButtonAdd, &QPushButton::clicked, this,
          &ProfileBoardWidget::onButtonAddClicked);
  connect(ui->pushButtonRemove, &QPushButton::clicked, this,
          &ProfileBoardWidget::onButtonRemoveClicked);
  connect(ui->pushButtonEdit, &QPushButton::clicked, this,
          &ProfileBoardWidget::onButtonEditClicked);
  connect(ui->pushButtonBack, &QPushButton::clicked, this,
          &ProfileBoardWidget::onButtonBackClicked);
  connect(ui->pushButtonSave, &QPushButton::clicked, this,
          &ProfileBoardWidget::onButtonSaveClicked);
  connect(ui->pushButtonRemoveBack, &QPushButton::clicked, this,
          &ProfileBoardWidget::onButtonBackClicked);
  connect(ui->pushButtonRemoveYes, &QPushButton::clicked, this,
          &ProfileBoardWidget::onButtonRemoveYesClicked);
  connect(ui->pushButtonRemoveNo, &QPushButton::clicked, this,
          &ProfileBoardWidget::onButtonBackClicked);

  fillProfiles();
  resetButtons();

  LOG_PERF(0, "Exit");
}

ProfileBoardWidget::~ProfileBoardWidget() {
  LOG_PERF(0, "Enter");

  delete _model;

  LOG_PERF(0, "Exit");
}

bool ProfileBoardWidget::eventFilter(QObject *watched, QEvent *event) {
  if (event->type() == QEvent::MouseButtonPress) {
    const QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
    const QPoint globalPos = mouseEvent->globalPos();

    // Check if click is outside this widget
    if (isVisible() && !geometry().contains(globalPos)) {
      hide();
      emit hidden();
      LOG_DEBUG("Clicked outside the widget!");
      return true;
    }
  }

  return QWidget::eventFilter(watched, event);
}

void ProfileBoardWidget::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  QPainterPath path;

  int w = width();
  int h = height();
  int arrowWidth = 20;
  int arrowHeight = 10;
  int radius = 10;

  // Clamp arrowX to stay within bounds
  int ax =
      qBound(radius + arrowWidth / 2, _arrowX, w - radius - arrowWidth / 2);

  // Rounded rectangle with top arrow
  path.moveTo(radius, arrowHeight);
  path.lineTo(ax - arrowWidth / 2, arrowHeight);
  path.lineTo(ax, 0); // Arrow tip
  path.lineTo(ax + arrowWidth / 2, arrowHeight);
  path.lineTo(w - radius, arrowHeight);
  path.arcTo(w - 2 * radius, arrowHeight, 2 * radius, 2 * radius, 90, -90);
  path.lineTo(w, h - radius);
  path.arcTo(w - 2 * radius, h - 2 * radius, 2 * radius, 2 * radius, 0, -90);
  path.lineTo(radius, h);
  path.arcTo(0, h - 2 * radius, 2 * radius, 2 * radius, 270, -90);
  path.lineTo(0, arrowHeight + radius);
  path.arcTo(0, arrowHeight, 2 * radius, 2 * radius, 180, -90);
  path.closeSubpath();

  painter.setBrush(QColor(80, 80, 80, 255)); // Semi-transparent
  painter.setPen(Qt::NoPen);
  painter.drawPath(path);
}

void ProfileBoardWidget::onButtonAddClicked() {
  LOG_PERF(0, "Enter");

  _operation = OperationType::Add;

  ui->stackedWidget->setCurrentIndex(static_cast<int>(ScreenType::Edit));

  LOG_PERF(0, "Exit");
}

void ProfileBoardWidget::onButtonBackClicked() {
  LOG_PERF(0, "Enter");

  _operation = OperationType::Unknown;

  _currentlyEditProfileIndex = -1;

  ui->lineEditAuthor->setText(QString());
  ui->lineEditEmail->setText(QString());
  ui->labelRemoveQuestion->setText(QString());

  ui->stackedWidget->setCurrentIndex(static_cast<int>(ScreenType::Display));

  LOG_PERF(0, "Exit");
}

void ProfileBoardWidget::onButtonEditClicked() {
  LOG_PERF(0, "Enter");

  _operation = OperationType::Edit;

  _currentlyEditProfileIndex =
      ui->listViewProfiles->selectionModel()->currentIndex().row();

  const Common::UserProfile &currentProfile =
      _userProfileManager.currentProfile();

  ui->lineEditAuthor->setText(currentProfile.author);
  ui->lineEditEmail->setText(currentProfile.email);

  ui->stackedWidget->setCurrentIndex(static_cast<int>(ScreenType::Edit));

  LOG_PERF(0, "Exit");
}

void ProfileBoardWidget::onButtonRemoveClicked() {
  LOG_PERF(0, "Enter");

  const int profileIndex =
      ui->listViewProfiles->selectionModel()->currentIndex().row();

  const QVector<Common::UserProfile> profiles = _userProfileManager.profiles();

  Q_ASSERT(profileIndex >= 0);
  Q_ASSERT(profileIndex < profiles.size());

  if (profileIndex < 0 || profileIndex >= profiles.size()) {
    LOG_PERF(0, "Exit");
    return;
  };

  const Common::UserProfile &profile = profiles.at(profileIndex);

  ui->labelRemoveQuestion->setText(
      tr("Are you sure you want to remove the profile \"%1\"?")
          .arg(profile.author));

  ui->stackedWidget->setCurrentIndex(static_cast<int>(ScreenType::Remove));

  LOG_PERF(0, "Exit");
}

void ProfileBoardWidget::onButtonRemoveYesClicked() {
  LOG_PERF(0, "Enter");

  const int profileIndex =
      ui->listViewProfiles->selectionModel()->currentIndex().row();

  QVector<Common::UserProfile> profiles = _userProfileManager.profiles();

  Q_ASSERT(profileIndex >= 0);
  Q_ASSERT(profileIndex < profiles.size());

  if (profileIndex < 0 || profileIndex >= profiles.size()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const bool currentProfileRemoval = profiles.at(profileIndex).email ==
                                     _userProfileManager.currentProfile().email;

  profiles.removeAt(profileIndex);

  if (currentProfileRemoval) {
    _userProfileManager.setCurrentIndex(profiles.size() > 0 ? 0 : -1);
  }

  _userProfileManager.setProfiles(profiles);
  _userProfileManager.save();

  fillProfiles();

  onButtonBackClicked();

  LOG_PERF(0, "Exit");
}

void ProfileBoardWidget::onButtonSaveClicked() {
  LOG_PERF(0, "Enter");

  const QString author = ui->lineEditAuthor->text().trimmed();
  const QString email = ui->lineEditEmail->text().trimmed();

  QVector<Common::UserProfile> profiles = _userProfileManager.profiles();

  Q_ASSERT(_operation == OperationType::Add ||
           _operation == OperationType::Edit);

  if (_operation == OperationType::Edit) {
    Q_ASSERT(_currentlyEditProfileIndex >= 0);
    Q_ASSERT(_currentlyEditProfileIndex < profiles.size());

    if (_currentlyEditProfileIndex < 0 ||
        _currentlyEditProfileIndex >= profiles.size()) {
      LOG_PERF(0, "Exit");
      return;
    }

    profiles[_currentlyEditProfileIndex].author = author;
    profiles[_currentlyEditProfileIndex].email = email;
  } else if (_operation == OperationType::Add) {
    profiles.append(Common::UserProfile{author, email});
  } else {
    LOG_PERF(0, "Exit");
    return;
  }

  _userProfileManager.setProfiles(profiles);

  if (_operation == OperationType::Add) {
    _userProfileManager.setCurrentIndex(profiles.size() - 1);
  }

  _userProfileManager.save();

  fillProfiles();

  onButtonBackClicked();

  LOG_PERF(0, "Exit");
}

void ProfileBoardWidget::onListItemClicked(const QModelIndex &index) {
  LOG_PERF(0, "Enter");

  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const int profileIndex = index.row();

  _userProfileManager.setCurrentIndex(profileIndex);
  _userProfileManager.save();

  fillProfiles();

  const QModelIndex selectedIndex = _model->index(profileIndex, 0);

  ui->listViewProfiles->setCurrentIndex(selectedIndex);
  ui->listViewProfiles->selectionModel()->select(selectedIndex,
                                                 QItemSelectionModel::Select);

  resetButtons();

  LOG_PERF(0, "Exit");
}

void ProfileBoardWidget::fillProfiles() {
  LOG_PERF(0, "Enter");

  _model->clear();

  const auto &profiles = _userProfileManager.profiles();
  const auto &currentProfile = _userProfileManager.currentProfile();

  for (int i = 0; i < profiles.size(); ++i) {
    const auto &profile = profiles.at(i);
    QStandardItem *item =
        new QStandardItem(profile.email == currentProfile.email
                              ? QPixmap(":icons/profile-default.png")
                              : QPixmap(":icons/profile.png"),
                          profile.author);
    item->setData(i); // Store index for retrieval
    _model->appendRow(item);
  }

  ui->listViewProfiles->setModel(_model);

  LOG_PERF(0, "Exit");
}

void ProfileBoardWidget::resetButtons() {
  LOG_PERF(0, "Enter");

  ui->pushButtonEdit->setVisible(
      !ui->listViewProfiles->selectionModel()->selectedIndexes().isEmpty());
  ui->pushButtonRemove->setVisible(
      !ui->listViewProfiles->selectionModel()->selectedIndexes().isEmpty());

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
