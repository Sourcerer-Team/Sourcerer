
#pragma once

#include "BranchesModel.h"

#include <QObject>

namespace GuiManager {

namespace Models {

class TagsModel : public BranchesModel
{
    Q_OBJECT
public:
    explicit TagsModel(QObject *parent = nullptr);
    virtual ~TagsModel();

    // BranchesModel interface
public:
    QString iconPath() const override;
};

}
}
