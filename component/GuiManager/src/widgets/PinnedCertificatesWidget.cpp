// MIT License
// Copyright (c) 2026 Sourcerer

#include "widgets/PinnedCertificatesWidget.h"

#include <GitRepo.h>
#include <Log.h>

#include <QDateTime>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

#define THREAD_ID "PinnedCertificatesWidget"

namespace GuiManager::Widgets {

namespace {

enum Column { StatusColumn = 3 };

QString formatTime(qint64 msecs) {
  return msecs > 0 ? QDateTime::fromMSecsSinceEpoch(msecs).toString(
                         QStringLiteral("yyyy-MM-dd hh:mm:ss"))
                   : QString();
}

} // namespace

PinnedCertificatesWidget::PinnedCertificatesWidget(
    GitRepo::PinnedCertificateType type, QWidget *parent)
    : QWidget(parent), _type(type) {
  const bool ssh = _type == GitRepo::PinnedCertificateType::Ssh;

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(10);

  auto *title = new QLabel(ssh ? tr("Trusted SSH host keys")
                               : tr("Trusted server certificates"),
                           this);
  QFont titleFont = title->font();
  titleFont.setPointSize(12);
  titleFont.setBold(true);
  title->setFont(titleFont);
  layout->addWidget(title);

  auto *description = new QLabel(
      ssh ? tr("Host keys of SSH servers that are not in known_hosts are "
               "trusted the first time Sourcerer connects. If a server later "
               "presents a different key, the connection is refused until you "
               "trust the new key here.")
          : tr("Self-signed HTTPS certificates are trusted the first time "
               "Sourcerer connects to the server. If the server later "
               "presents a different certificate, the connection is refused "
               "until you trust the new certificate here."),
      this);
  description->setWordWrap(true);
  layout->addWidget(description);

  _model = new QStandardItemModel(0, 4, this);
  _model->setHorizontalHeaderLabels({tr("Host"), tr("Fingerprint (SHA-256)"),
                                     tr("Trusted since"), tr("Status")});
  _tableView = new QTableView(this);
  _tableView->setModel(_model);
  _tableView->setAlternatingRowColors(true);
  _tableView->setSelectionMode(QAbstractItemView::SingleSelection);
  _tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
  _tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
  _tableView->verticalHeader()->setVisible(false);
  // Host, fingerprint and date as wide as they are, the status gets the rest.
  QHeaderView *header = _tableView->horizontalHeader();
  header->setSectionResizeMode(QHeaderView::ResizeToContents);
  header->setSectionResizeMode(StatusColumn, QHeaderView::Stretch);
  layout->addWidget(_tableView);

  auto *buttons = new QHBoxLayout();
  buttons->setSpacing(10);
  const auto addButton = [&](const QString &text) {
    auto *button = new QPushButton(text, this);
    button->setMinimumWidth(120);
    buttons->addWidget(button);
    return button;
  };
  QPushButton *refreshButton = addButton(tr("Refresh"));
  _trustNewButton =
      addButton(ssh ? tr("Trust new key") : tr("Trust new certificate"));
  _forgetButton = addButton(tr("Forget"));
  _trustNewButton->setEnabled(false);
  _forgetButton->setEnabled(false);
  buttons->addStretch();
  layout->addLayout(buttons);

  connect(refreshButton, &QPushButton::clicked, this,
          &PinnedCertificatesWidget::reload);
  connect(_trustNewButton, &QPushButton::clicked, this,
          &PinnedCertificatesWidget::onTrustNewClicked);
  connect(_forgetButton, &QPushButton::clicked, this,
          &PinnedCertificatesWidget::onForgetClicked);
  connect(_tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, &PinnedCertificatesWidget::onSelectionChanged);

  reload();
}

QString PinnedCertificatesWidget::kindName() const {
  return _type == GitRepo::PinnedCertificateType::Ssh ? tr("host key")
                                                      : tr("certificate");
}

void PinnedCertificatesWidget::reload() {
  LOG_PERF(0, "Enter");

  _pins.clear();
  for (const GitRepo::PinnedCertificate &pin :
       GitRepo::getPinnedCertificates()) {
    if (pin.type == _type) {
      _pins.append(pin);
    }
  }

  _model->removeRows(0, _model->rowCount());
  for (const GitRepo::PinnedCertificate &pin : _pins) {
    auto *host = new QStandardItem(pin.host);
    host->setToolTip(pin.host);
    auto *fingerprint = new QStandardItem(pin.fingerprint);
    fingerprint->setToolTip(pin.fingerprint);
    auto *trusted = new QStandardItem(formatTime(pin.trustedSince));

    QStandardItem *status = nullptr;
    if (pin.rejectedFingerprint.isEmpty()) {
      status = new QStandardItem(tr("Trusted"));
    } else {
      status = new QStandardItem(
          tr("Changed, refused on %1").arg(formatTime(pin.rejectedAt)));
      status->setToolTip(tr("The server presented a different %1:\n%2")
                             .arg(kindName(), pin.rejectedFingerprint));
      QFont bold = status->font();
      bold.setBold(true);
      status->setFont(bold);
    }
    _model->appendRow({host, fingerprint, trusted, status});
  }

  onSelectionChanged();

  LOG_PERF(0, "Exit");
}

bool PinnedCertificatesWidget::selectedPin(
    GitRepo::PinnedCertificate *pin) const {
  const QModelIndexList rows = _tableView->selectionModel()->selectedRows();
  if (rows.isEmpty()) {
    return false;
  }
  const int row = rows.first().row();
  if (row < 0 || row >= _pins.size()) {
    return false;
  }
  *pin = _pins.at(row);
  return true;
}

void PinnedCertificatesWidget::onSelectionChanged() {
  GitRepo::PinnedCertificate pin;
  const bool selected = selectedPin(&pin);
  _forgetButton->setEnabled(selected);
  _trustNewButton->setEnabled(selected && !pin.rejectedFingerprint.isEmpty());
}

void PinnedCertificatesWidget::onTrustNewClicked() {
  GitRepo::PinnedCertificate pin;
  if (!selectedPin(&pin) || pin.rejectedFingerprint.isEmpty()) {
    return;
  }

  const QMessageBox::StandardButton reply = QMessageBox::warning(
      this, tr("Trust new %1").arg(kindName()),
      tr("%1 presented a different %2 than the one trusted before.\n\n"
         "Trusted: %3\nNew: %4\n\n"
         "Trust the new %2 only if you know it was replaced, for example "
         "when the server's administrator confirms the new fingerprint. "
         "Otherwise somebody may be intercepting the connection.")
          .arg(pin.host, kindName(), pin.fingerprint,
               pin.rejectedFingerprint),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (reply != QMessageBox::Yes) {
    return;
  }

  if (!GitRepo::trustRejectedCertificate(_type, pin.host)) {
    LOG_WARN("No refused certificate to trust for host: {}", pin.host);
  } else {
    LOG_INFO("Trusted the new certificate of host: {}", pin.host);
  }
  reload();
}

void PinnedCertificatesWidget::onForgetClicked() {
  GitRepo::PinnedCertificate pin;
  if (!selectedPin(&pin)) {
    return;
  }

  const QMessageBox::StandardButton reply = QMessageBox::question(
      this, tr("Forget %1").arg(kindName()),
      tr("Forget the %1 trusted for %2?\n\n"
         "The next connection to %2 trusts whatever %1 the server presents "
         "then.")
          .arg(kindName(), pin.host),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (reply != QMessageBox::Yes) {
    return;
  }

  GitRepo::forgetPinnedCertificate(_type, pin.host);
  LOG_INFO("Forgot the pinned certificate of host: {}", pin.host);
  reload();
}

} // namespace GuiManager::Widgets
