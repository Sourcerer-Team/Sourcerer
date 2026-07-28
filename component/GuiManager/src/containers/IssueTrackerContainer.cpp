#include "containers/IssueTrackerContainer.h"

#include "ui_IssueTrackerContainer.h"

#include <GitHubRepo.h>
#include <Log.h>

#include <QObject>
#include <QStringList>

#define THREAD_ID "IssueTrackerContainer"

namespace GuiManager::Containers {

namespace {

QString normalizedServiceType(const QString &serviceType) {
  return serviceType.trimmed().toLower();
}

QUrl buildIssueTrackerUrl(const QString &serviceType,
                          const QString &repositoryName) {
  const QString normalized = normalizedServiceType(serviceType);
  const QString trimmedRepositoryName = repositoryName.trimmed();
  if (normalized == QStringLiteral("github")) {
    const QStringList parts = trimmedRepositoryName.split(QStringLiteral("/"),
                                                         Qt::SkipEmptyParts);
    if (parts.size() >= 2) {
      const IssueTrackers::GitHubRepo repo(parts.first(), parts.last());
      return QUrl(repo.issuesUrl());
    }
  }

  return QUrl();
}

QString unsupportedMessage(const QString &serviceType,
                           const QString &repositoryName) {
  if (serviceType.trimmed().isEmpty() || repositoryName.trimmed().isEmpty()) {
    return QObject::tr("No issue tracker is selected.");
  }

  return QObject::tr("This issue tracker is not supported yet: %1 %2")
      .arg(serviceType.trimmed(), repositoryName.trimmed());
}

} // namespace

IssueTrackerContainer::IssueTrackerContainer(QString serviceType,
                                             QString repositoryName,
                                             QWidget *parent)
    : ContainerWidget(ContainerType::IssueTracker, parent),
      _configManager(ConfigManager::ConfigManager::getInstance()),
      _toastHost(ToastHostWidget::getInstance()),
      ui(new Ui::IssueTrackerContainer),
      _selected(false),
      _serviceType(serviceType.trimmed()),
      _repositoryName(repositoryName.trimmed()), _browserView(nullptr) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

#if GUI_MANAGER_HAS_WEBENGINE
  _browserView = new QWebEngineView(this);
#else
  _browserView = new QTextBrowser(this);
  _browserView->setOpenExternalLinks(true);
  _browserView->setOpenLinks(true);
#endif

  ui->mainLayout->setContentsMargins(0, 0, 0, 0);
  ui->mainLayout->setSpacing(0);
  ui->mainLayout->addWidget(_browserView);

  updateIssueUrl();
  loadIssuePage();

  LOG_PERF(0, "Exit");
}

IssueTrackerContainer::~IssueTrackerContainer() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void IssueTrackerContainer::init()
{
  LOG_PERF(0, "Enter");

  loadIssuePage();

  LOG_PERF(0, "Exit");
}

void IssueTrackerContainer::deselected()
{
  LOG_PERF(0, "Enter");

  _selected = false;

  LOG_PERF(0, "Exit");
}

void IssueTrackerContainer::selected() {
  LOG_PERF(0, "Enter");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);
  LOG_DEBUG("Title bar height retrieved: {} px", titleBarHeight);

  _toastHost.setParent(this);
  _toastHost.setYOffset(titleBarHeight);
  _toastHost.raise();
  LOG_DEBUG("Toast host configured and raised");

  _selected = true;

  LOG_PERF(0, "Exit");
}

void IssueTrackerContainer::updateIssueUrl() {
  LOG_PERF(0, "Enter");

  _issuesUrl = buildIssueTrackerUrl(_serviceType, _repositoryName);

  LOG_PERF(0, "Exit");
}

void IssueTrackerContainer::loadIssuePage() {
  LOG_PERF(0, "Enter");

  updateIssueUrl();

  if (_issuesUrl.isValid()) {
#if GUI_MANAGER_HAS_WEBENGINE
    _browserView->load(_issuesUrl);
#else
    _browserView->setHtml(QStringLiteral(
        "<html><body style='font-family:sans-serif;padding:16px;'>"
        "<h2>Issue tracker</h2>"
        "<p>WebEngine is not available in this build, but you can still open the issue page:</p>"
        "<p><a href='%1'>%1</a></p>"
        "</body></html>")
                             .arg(_issuesUrl.toString()));
#endif
  } else {
    showUnsupportedMessage(unsupportedMessage(_serviceType, _repositoryName));
  }

  LOG_PERF(0, "Exit");
}

void IssueTrackerContainer::showUnsupportedMessage(const QString &message) {
  LOG_PERF(0, "Enter");

#if GUI_MANAGER_HAS_WEBENGINE
  _browserView->setHtml(QStringLiteral(
      "<html><body style='font-family:sans-serif;padding:16px;'>"
      "<h2>Issue tracker</h2>"
      "<p>%1</p>"
      "</body></html>")
                           .arg(message.toHtmlEscaped()));
#else
  _browserView->setHtml(QStringLiteral(
      "<html><body style='font-family:sans-serif;padding:16px;'>"
      "<h2>Issue tracker</h2>"
      "<p>%1</p>"
      "</body></html>")
                           .arg(message.toHtmlEscaped()));
#endif

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Containers
