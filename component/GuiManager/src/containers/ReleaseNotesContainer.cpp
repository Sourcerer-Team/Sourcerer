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

  // Paint the whole container in the color of the text viewer, which follows
  // the application palette, so it also follows theme changes.
  setAutoFillBackground(true);
  setBackgroundRole(QPalette::Base);

  Q_UNUSED(title)

  // Larger text is easier to read in the wide viewer. The font has to be set
  // before the markdown, which picks it up as the document's default.
  QFont notesFont = ui->textBrowserNotes->font();
  if (notesFont.pointSizeF() > 0) {
    notesFont.setPointSizeF(notesFont.pointSizeF() * 1.25);
  } else if (notesFont.pixelSize() > 0) {
    notesFont.setPixelSize(qRound(notesFont.pixelSize() * 1.25));
  }
  ui->textBrowserNotes->setFont(notesFont);
  ui->textBrowserNotes->document()->setDefaultFont(notesFont);
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
