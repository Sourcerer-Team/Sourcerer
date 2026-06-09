
#include "models/TagsModel.h"

namespace GuiManager {

namespace Models {

static const char *ICON_PATH = ":/icons/git_tag.png";

TagsModel::TagsModel(QObject *parent) : BranchesModel{parent} {}

Models::TagsModel::~TagsModel() {}

QString TagsModel::iconPath() const {
  return ICON_PATH;
}

} // namespace Models
} // namespace GuiManager
