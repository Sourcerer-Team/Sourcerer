#include "widgets/ProfileBoardWidget.h"
#include "delegates/FixedHeightDelegate.h"
#include "widgets/ui_ProfileBoardWidget.h"

#include <ConfigManager.h>
#include <Log.h>
#include <WaitingSpinnerWidget.h>

#include <QByteArray>
#include <QDateTime>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QUrl>

#define THREAD_ID "ProfileBoardWidget"

namespace GuiManager::Widgets {

ProfileBoardWidget::ProfileBoardWidget(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint), ui(new Ui::ProfileBoardWidget),
      _arrowX(0), _currentlyEditProfileIndex(-1),
      _operation(OperationType::Unknown), _model(new QStandardItemModel),
      _userProfileManager(Common::UserProfileManager::instance()),
      _cloudCredentialStorage(Util::CloudCredentialtorage::getInstance()),
      _cloudRepo(Util::CloudRepo::getInstance()) {
  LOG_PERF(0, "Enter");

#ifdef Q_OS_WIN
  setWindowFlag(Qt::Tool);
#endif

  setAttribute(Qt::WA_TranslucentBackground);
  setMouseTracking(true);

  ui->setupUi(this);
  ui->listViewProfiles->setItemDelegate(
      new Delegates::FixedHeightDelegate(ui->listViewProfiles));

  // Create and configure waiting spinner for signin
  _signinSpinner = new WaitingSpinnerWidget(this, true, true);
  _signinSpinner->setColor(QColor(100, 200, 255));
  _signinSpinner->setLineLength(3);
  _signinSpinner->setLineWidth(2);

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
  connect(ui->pushButtonSigninSubmit, &QPushButton::clicked, this,
          &ProfileBoardWidget::onButtonSigninSubmitClicked);
  connect(ui->pushButtonSignout, &QPushButton::clicked, this,
          &ProfileBoardWidget::onButtonSignoutClicked);
  connect(ui->labelSignupLink, &QLabel::linkActivated, this,
          &ProfileBoardWidget::onSignupLinkClicked);

  // Connect Enter key in signin fields to submit button
  connect(ui->lineEditSigninUser, &QLineEdit::returnPressed, this,
          &ProfileBoardWidget::onButtonSigninSubmitClicked);
  connect(ui->lineEditSigninPassword, &QLineEdit::returnPressed, this,
          &ProfileBoardWidget::onButtonSigninSubmitClicked);

  // Connect to CloudRepo signals
  connect(&_cloudRepo, &Util::CloudRepo::signinSuccess, this,
          &ProfileBoardWidget::onCloudSigninSuccess);
  connect(&_cloudRepo, &Util::CloudRepo::signinFailed, this,
          &ProfileBoardWidget::onCloudSigninFailed);
  connect(&_cloudRepo, &Util::CloudRepo::signinError, this,
          &ProfileBoardWidget::onCloudSigninError);

  fillProfiles();
  resetButtons();

  // Try to load stored cloud credentials
  QString username, password, jwt;
  if (_cloudCredentialStorage.getCredentials(username, password, jwt)) {
    // Credentials exist
    _isLoggedIn = true;

    LOG_INFO("Loaded stored cloud credentials for user: {}", username);

    // Extract JWT claims to get user's first and last name
    QString jwt;
    if (_cloudCredentialStorage.getJWT(jwt)) {
      const Util::JWTDecoder::Claims claims =
          Util::JWTDecoder::decodeClaims(jwt);
      _userFirstName = claims.givenName;
      _userLastName = claims.familyName;
      _plan = claims.plan;

      LOG_INFO("Extracted JWT claims - FirstName: {}, LastName: {}, Plan: {}",
               _userFirstName, _userLastName, _plan);
    }

    LOG_INFO("Attempting automatic refresh of credentials...");

    tryRefreshExpiredCredentials();
  }

  updatePageForLoginState();

  LOG_PERF(0, "Exit");
}

// Dummy login state for demonstration. Replace with real logic as needed.
bool ProfileBoardWidget::isUserLoggedIn() const {
  return _isLoggedIn;
}

void ProfileBoardWidget::updatePageForLoginState() {
  LOG_INFO("updatePageForLoginState called - _isLoggedIn: {}, firstName: '{}', "
           "lastName: '{}', plan: '{}'",
           _isLoggedIn, _userFirstName, _userLastName, _plan);
  if (isUserLoggedIn()) {
    ui->stackedWidget->setCurrentWidget(ui->pageProfiles);
    ui->pushButtonSignout->setVisible(true);
    ui->labelSignupLink->hide();

    // Update the label with user's name from JWT claims
    QString labelText = "Profiles";
    if (!_userFirstName.isEmpty() || !_userLastName.isEmpty()) {
      labelText = QString("Profile for:\n%1 %2 [%3]")
                      .arg(_userFirstName, _userLastName, _plan)
                      .trimmed();
    }
    ui->labelProfiles->setText(labelText);
    LOG_INFO("Updated page - label: '{}', signout button visible: {}",
             labelText.toStdString(), ui->pushButtonSignout->isVisible());
  } else {
    showSigninPage();
  }
}

void ProfileBoardWidget::onButtonSigninSubmitClicked() {
  QString user = ui->lineEditSigninUser->text().trimmed();
  QString pass = ui->lineEditSigninPassword->text();
  QString emailError, passwordError;
  bool valid = true;
  if (!validateEmail(user, emailError)) {
    ui->labelSigninError->setText(emailError);
    valid = false;
  } else if (!validatePassword(pass, passwordError)) {
    ui->labelSigninError->setText(passwordError);
    valid = false;
  } else {
    ui->labelSigninError->clear();
  }
  if (valid) {
    doLogin(user, pass);
  }
}

void ProfileBoardWidget::onButtonSignoutClicked() {
  QMessageBox::StandardButton reply = QMessageBox::question(
      this, "Confirm sign out", "Are you sure you want to sign out?",
      QMessageBox::Yes | QMessageBox::No);
  if (reply == QMessageBox::Yes) {
    doLogout();
  }
}

void ProfileBoardWidget::onSignupLinkClicked() {
  ::ConfigManager::ConfigManager &configManager =
      ::ConfigManager::ConfigManager::getInstance();

  const QString signupUrl =
      configManager.getString(::ConfigManager::ConfigKey::CloudSignupUrl);
  QDesktopServices::openUrl(QUrl(signupUrl));
}

void ProfileBoardWidget::doLogin(const QString &user, const QString &password) {
  // Dummy login: just set login state if user and password are non-empty
  if (!user.isEmpty() && !password.isEmpty()) {
    // Hide form widgets and show spinner
    hideSigninWidgets();

    if (_signinSpinner) {
      _signinSpinner->start();
    }

    // Request signin from cloud service
    const QString user_ = user;
    const QString password_ = password;

    _dispatcher.dispatch([=]() { _cloudRepo.signin(user_, password_); });
  }
}

void ProfileBoardWidget::doLogout() {

  const auto guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    const auto reply = _cloudRepo.signOut();
    if (reply.first != QNetworkReply::NoError) {
      LOG_ERROR(Log::ErrorId::Network,
                "Signout request failed with error code: {}", (int)reply.first);
      ::Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        QMessageBox::warning(
            this, "Sign out failed",
            "An error occurred while signing out. Please try again.");
      });
      return;
    }

    ::Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      // Clear login state
      _isLoggedIn = false;

      // Clear JWT claims
      _userFirstName.clear();
      _userLastName.clear();

      // Clear stored credentials
      _cloudCredentialStorage.clearAll();

      // Update page and show signin form
      updatePageForLoginState();

      // Show signin form widgets
      ui->lineEditSigninUser->show();
      ui->lineEditSigninPassword->show();
      ui->pushButtonSigninSubmit->show();

      // Clear input fields for security
      ui->lineEditSigninUser->clear();
      ui->lineEditSigninPassword->clear();
      ui->labelSigninTitle->setText("Sign in to continue");

      ui->lineEditSigninUser->setFocus();
    });
  });
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

  ui->stackedWidget->setCurrentIndex(static_cast<int>(ScreenType::Profiles));

  const QVector<Common::UserProfile> profiles = _userProfileManager.profiles();

  ui->pushButtonEdit->setVisible(!profiles.isEmpty());
  ui->pushButtonRemove->setVisible(!profiles.isEmpty());

  if (profiles.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  selectCurrentProfile();

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

  selectCurrentProfile();

  LOG_PERF(0, "Exit");
}

void ProfileBoardWidget::selectCurrentProfile() {
  LOG_PERF(0, "Enter");

  const int currentIndex = _userProfileManager.currentIndex();
  const QModelIndex selectedIndex = _model->index(currentIndex, 0);
  ui->listViewProfiles->setCurrentIndex(selectedIndex);
  ui->listViewProfiles->selectionModel()->select(selectedIndex,
                                                 QItemSelectionModel::Select);

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

// Email validation regex: ^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$
bool ProfileBoardWidget::validateEmail(const QString &email,
                                       QString &errorMsg) const {
  if (email.isEmpty()) {
    errorMsg = "Enter email";
    return false;
  }

  const QRegularExpression emailRegex(
      "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$");
  if (!emailRegex.match(email).hasMatch()) {
    errorMsg = "Enter a valid email address";
    return false;
  }
  errorMsg.clear();
  return true;
}

// Password validation regex:
// ^(?=.*[a-z])(?=.*[A-Z])(?=.*\d)[a-zA-Z\d!@#\$&*~]{8,}$
bool ProfileBoardWidget::validatePassword(const QString &password,
                                          QString &errorMsg) const {
  if (password.isEmpty()) {
    errorMsg = "Enter password";
    return false;
  }

  const QRegularExpression passwordRegex(
      "^(?=.*[a-z])(?=.*[A-Z])(?=.*\\d)[a-zA-Z\\d!@#\\$&*~]{8,}$");
  if (!passwordRegex.match(password).hasMatch()) {
    errorMsg = "Password must be at least 8 characters, include upper and "
               "lower case letters, and a number";
    return false;
  }
  errorMsg.clear();
  return true;
}

void ProfileBoardWidget::tryRefreshExpiredCredentials() {
  LOG_PERF(0, "Enter");

  QString username, password, jwt;
  if (!_cloudCredentialStorage.getCredentials(username, password, jwt)) {
    LOG_WARN("No credentials to refresh");
    LOG_PERF(0, "Exit");
    return;
  }

  _dispatcher.dispatch([=]() {
    // Request signin from cloud service. Credentials will be stored
    // automatically on success.
    _cloudRepo.signin(username, password);
  });

  LOG_PERF(0, "Exit");
}

void ProfileBoardWidget::hideSigninWidgets() {
  LOG_PERF(0, "Enter");

  ui->labelSigninIcon->hide();
  ui->labelSigninTitle->hide();
  ui->lineEditSigninUser->hide();
  ui->lineEditSigninPassword->hide();
  ui->pushButtonSigninSubmit->hide();
  ui->labelSignupLink->hide();

  LOG_PERF(0, "Exit");
}

void ProfileBoardWidget::showSigninWidgets() {
  LOG_PERF(0, "Enter");

  ui->labelSigninIcon->show();
  ui->labelSigninTitle->show();
  ui->lineEditSigninUser->show();
  ui->lineEditSigninPassword->show();
  ui->pushButtonSigninSubmit->show();
  ui->labelSignupLink->show();

  LOG_PERF(0, "Exit");
}

void ProfileBoardWidget::showSigninPage() {
  LOG_PERF(0, "Enter");

  ui->stackedWidget->setCurrentWidget(ui->pageSignin);
  ui->pushButtonSignout->setVisible(false);

  // Ensure signin form widgets are visible
  showSigninWidgets();

  ui->lineEditSigninUser->clear();
  ui->lineEditSigninPassword->clear();
  ui->labelSigninError->clear();

  ui->lineEditSigninUser->setFocus();

  LOG_PERF(0, "Exit");
}

void ProfileBoardWidget::onCloudSigninSuccess(const QString &email,
                                              const QString &password,
                                              const QString &accessToken,
                                              const QString &response) {
  LOG_PERF(0, "Enter");
  LOG_INFO("Cloud signin success for: {}", email);

  // Hide spinner
  if (_signinSpinner) {
    _signinSpinner->stop();
  }

  // Verify JWT signature before accepting credentials
  const QString publicKeyPem = Util::JWTDecoder::getJWTPublicKey();
  if (publicKeyPem.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Security,
              "JWT public key not configured, skipping signature verification. "
              "This should be configured for security.");

    showSigninPage();

    ui->labelSigninError->setText("JWT public key not configured");

    LOG_PERF(0, "Exit");
    return;
  } else {
    if (!Util::JWTDecoder::verifySignature(accessToken, publicKeyPem)) {
      LOG_ERROR(Log::ErrorId::Security,
                "JWT signature verification failed during signin for user: {}",
                email);

      showSigninPage();

      ui->labelSigninError->setText("JWT signature verification failed");

      LOG_PERF(0, "Exit");
      return;
    }
    LOG_DEBUG("JWT signature verification successful for user: {}", email);
  }

  // Store credentials with configurable expiration time
  const qlonglong expirationMinutes =
      ConfigManager::ConfigManager::getInstance().getLong(
          ConfigManager::ConfigKey::CloudJwtExpirationMinutes);
  const qint64 expirationTime =
      QDateTime::currentMSecsSinceEpoch() + (expirationMinutes * 60 * 1000);

  _cloudCredentialStorage.storeCredentials(email, password, accessToken,
                                           expirationTime, true);

  LOG_INFO("Credentials stored successfully");

  // Extract JWT claims to get user's first and last name
  const Util::JWTDecoder::Claims claims =
      Util::JWTDecoder::decodeClaims(accessToken);
  _userFirstName = claims.givenName;
  _userLastName = claims.familyName;
  _plan = claims.plan;
  LOG_INFO("Updated JWT claims - FirstName: {}, LastName: {}, Plan: {}",
           _userFirstName, _userLastName, _plan);

  // Update login state
  _isLoggedIn = true;

  // Clear input fields
  ui->lineEditSigninUser->clear();
  ui->lineEditSigninPassword->clear();

  // Switch to profiles page
  updatePageForLoginState();

  LOG_PERF(0, "Exit");
}

void ProfileBoardWidget::onCloudSigninFailed(const QString &email,
                                             const QString &errorMessage) {
  LOG_PERF(0, "Enter");
  LOG_ERROR(Log::ErrorId::Network, "Cloud signin failed: {}", errorMessage);

  // Hide spinner and show form widgets
  if (_signinSpinner) {
    _signinSpinner->stop();
  }

  showSigninWidgets();

  if (ui->stackedWidget->currentWidget() == ui->pageSignin) {
    ui->labelSigninError->setText("Login failed: " + errorMessage);
  }

  ui->stackedWidget->setCurrentWidget(ui->pageSignin);

  // Prefill email for retry
  if (!email.isEmpty()) {
    ui->lineEditSigninUser->setText(email);
    ui->lineEditSigninPassword->setFocus();
    ui->lineEditSigninPassword->selectAll();
  }

  _userFirstName.clear();
  _userLastName.clear();
  _plan = "Community";

  LOG_PERF(0, "Exit");
}

void ProfileBoardWidget::onCloudSigninError(const QString &email,
                                            QNetworkReply::NetworkError error) {
  LOG_PERF(0, "Enter");
  LOG_ERROR(Log::ErrorId::Network, "Cloud signin error: {}", (int)error);

  // Hide spinner and show form widgets
  if (_signinSpinner) {
    _signinSpinner->stop();
  }

  showSigninWidgets();

  if (ui->stackedWidget->currentWidget() == ui->pageSignin) {
    ui->labelSigninError->setText("Login error");
  }

  // Prefill email for retry
  if (!email.isEmpty()) {
    ui->lineEditSigninUser->setText(email);
    ui->lineEditSigninPassword->setFocus();
    ui->lineEditSigninPassword->selectAll();
  }

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
