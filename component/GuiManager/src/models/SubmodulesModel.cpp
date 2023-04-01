
#include "models/SubmodulesModel.h"

namespace GuiManager {

namespace Models {

static const char *ICON_PATH = ":/icons/git_submodule.png";

SubmodulesModel::SubmodulesModel(QObject *parent) : BranchesModel{parent} {}

SubmodulesModel::~SubmodulesModel() {}

QString SubmodulesModel::iconPath() const { return ICON_PATH; }

} // namespace Models
} // namespace GuiManager
