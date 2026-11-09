#pragma once

#include <GitRepoTypes.h>

#include <QStandardItemModel>
#include <QWidget>

class QPushButton;
class QTableView;

namespace GuiManager::Widgets {

// The self-signed HTTPS certificates or SSH host keys trusted on first use
// (GitRepo::getPinnedCertificates()). A server that presents a different one
// is refused; here the user can trust the new one or forget the pin.
class PinnedCertificatesWidget final : public QWidget {
  Q_OBJECT

public:
  explicit PinnedCertificatesWidget(GitRepo::PinnedCertificateType type,
                                    QWidget *parent = nullptr);

  // Reads the pins again.
  void reload();

private slots:
  void onTrustNewClicked();
  void onForgetClicked();
  void onSelectionChanged();

private:
  // The pin of the selected row, false if none is selected.
  bool selectedPin(GitRepo::PinnedCertificate *pin) const;
  QString kindName() const;

  GitRepo::PinnedCertificateType _type;
  QVector<GitRepo::PinnedCertificate> _pins;
  QStandardItemModel *_model = nullptr;
  QTableView *_tableView = nullptr;
  QPushButton *_trustNewButton = nullptr;
  QPushButton *_forgetButton = nullptr;
};

} // namespace GuiManager::Widgets
