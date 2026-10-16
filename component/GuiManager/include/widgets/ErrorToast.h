#pragma once

#include "widgets/InfoToast.h"

namespace GuiManager::Widgets {

class ErrorToast final : public InfoToast {
  Q_OBJECT
public:
  explicit ErrorToast(const QString &message, const QString &title = QString(),
                      QWidget *parent = nullptr);
  ~ErrorToast() override;
};

} // namespace GuiManager::Widgets
