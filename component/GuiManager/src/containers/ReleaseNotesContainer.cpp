#include "containers/ReleaseNotesContainer.h"
#include "ui_ReleaseNotesContainer.h"

#include <Log.h>

#define THREAD_ID "ReleaseNotesContainer"

namespace GuiManager::Containers {

ReleaseNotesContainer::ReleaseNotesContainer(const QString &title,
                                             const QString &markdown,
                                             QWidget *parent)
    : ContainerWidget(ContainerType::ReleaseNotes, parent),
      ui(new Ui::ReleaseNotesContainer) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  ui->labelHeader->setText(title);
  ui->textBrowserNotes->setMarkdown(markdown);

  connect(ui->pushButtonClose, &QPushButton::clicked, this,
          &ReleaseNotesContainer::onCloseClicked);

  LOG_PERF(0, "Exit");
}

ReleaseNotesContainer::~ReleaseNotesContainer() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void ReleaseNotesContainer::onCloseClicked() {
  LOG_PERF(0, "Enter");

  emit close();

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Containers
