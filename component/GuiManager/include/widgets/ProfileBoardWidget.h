#pragma once

#include <UserProfileManager.h>

#include <QStandardItemModel>
#include <QWidget>

namespace Ui {
class ProfileBoardWidget;
}

namespace GuiManager::Widgets {

class ProfileBoardWidget : public QWidget {
  Q_OBJECT

public:
  explicit ProfileBoardWidget(QWidget *parent = nullptr);
  ~ProfileBoardWidget() override;

  void setArrowTargetX(int x) { _arrowX = x; }

signals:
  void hidden();

  // QObject interface
public:
  bool eventFilter(QObject *watched, QEvent *event) override;

  // QWidget interface
protected:
  void paintEvent(QPaintEvent *event) override;

private slots:
  void onButtonAddClicked();
  void onButtonBackClicked();
  void onButtonEditClicked();
  void onButtonRemoveClicked();
  void onButtonRemoveYesClicked();
  void onButtonSaveClicked();
  void onListItemClicked(const QModelIndex &index);

private:
  enum class ScreenType {
    Unknown = -1,
    Display = 0,
    Edit = 1,
    Remove = 2,
    last
  };

  enum class OperationType { Unknown = 0, Add, Edit, last };

  void fillProfiles();
  void resetButtons();

private:
  Ui::ProfileBoardWidget *ui;
  int _arrowX; // Default center
  int _currentlyEditProfileIndex;
  OperationType _operation;
  QStandardItemModel *_model;
  Common::UserProfileManager &_userProfileManager;
};

} // namespace GuiManager::Widgets
