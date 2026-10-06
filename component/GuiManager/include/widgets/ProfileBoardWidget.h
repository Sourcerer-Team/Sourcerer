#pragma once

#include <CloudCredentialStorage.h>
#include <CloudRepo.h>
#include <Dispatcher.h>
#include <JWTDecoder.h>
#include <UserProfileManager.h>
#include <WaitingSpinnerWidget.h>

#include <QEnableSharedFromThis>
#include <QEvent>
#include <QHash>
#include <QNetworkReply>
#include <QPixmap>
#include <QSet>
#include <QStandardItemModel>
#include <QString>
#include <QWidget>

class QNetworkAccessManager;

namespace Ui {
class ProfileBoardWidget;
}

namespace GuiManager::Widgets {

class ProfileBoardWidget : public QWidget,
                           public QEnableSharedFromThis<ProfileBoardWidget> {
  Q_OBJECT

  typedef QSharedPointer<ProfileBoardWidget> SharedPtr;
  typedef QWeakPointer<ProfileBoardWidget> WeakPtr;

public:
  explicit ProfileBoardWidget(QWidget *parent = nullptr);
  ~ProfileBoardWidget() override;

  SharedPtr getSelf() { return sharedFromThis(); }

  void setArrowTargetX(int x) { _arrowX = x; }

signals:
  void hidden();

  // QObject interface
public:
  bool eventFilter(QObject *watched, QEvent *event) override;

  // QWidget interface
protected:
  void paintEvent(QPaintEvent *event) override;
  void changeEvent(QEvent *event) override;

private slots:
  void onButtonAddClicked();
  void onButtonBackClicked();
  void onButtonEditClicked();
  void onButtonRemoveClicked();
  void onButtonRemoveYesClicked();
  void onButtonSaveClicked();
  void onListItemClicked(const QModelIndex &index);
  void onButtonSigninSubmitClicked();
  void onButtonSignoutClicked();
  void onSignupLinkClicked();
  void onSkipSigninClicked();
  void onCloudSigninSuccess(const QString &email, const QString &password,
                            const QString &accessToken,
                            const QString &response);
  void onCloudSigninFailed(const QString &email, const QString &errorMessage);
  void onCloudSigninError(const QString &email,
                          QNetworkReply::NetworkError error);

private:
  enum class ScreenType {
    Unknown = -1,

    Signin = 0,
    Profiles = 1,
    Edit = 2,
    Remove = 3,

    last
  };

  void updatePageForLoginState();
  bool isUserLoggedIn() const;
  void doLogin(const QString &user, const QString &password);
  void doLogout();

  enum class OperationType { Unknown = 0, Add, Edit, last };

  void fillProfiles();
  void selectCurrentProfile();
  void resetButtons();
  void updateProfileAvatar(int row, const QString &authorName,
                           const QString &authorEmail);

  bool validateEmail(const QString &email, QString &errorMsg) const;
  bool validatePassword(const QString &password, QString &errorMsg) const;

  void tryRefreshExpiredCredentials();

  void hideSigninWidgets();
  void showSigninWidgets();
  void showSigninPage();

private:
  Ui::ProfileBoardWidget *ui;
  int _arrowX; // Default center
  int _currentlyEditProfileIndex;
  OperationType _operation;
  QStandardItemModel *_model;
  QNetworkAccessManager *_profileAvatarNetworkManager;
  QHash<QString, QPixmap> _profileAvatarCache;
  QSet<QString> _profileAvatarInFlight;
  Common::UserProfileManager &_userProfileManager;
  Util::CloudCredentialtorage &_cloudCredentialStorage;
  Util::CloudRepo &_cloudRepo;
  Util::Dispatcher _dispatcher;
  // For login state unrelated to UserProfileManager
  bool _isLoggedIn = false;
  QString _userFirstName;
  QString _userLastName;
  QString _plan;
  bool _autoLogin;
  WaitingSpinnerWidget *_signinSpinner = nullptr;
};

} // namespace GuiManager::Widgets
