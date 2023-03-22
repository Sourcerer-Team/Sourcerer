#include "containers/ContainerWidget.h"

namespace GuiManager {

namespace Containers {

ContainerWidget::ContainerWidget(ContainerType type, QWidget *parent)
    : QWidget{parent}, _type(type) {}

ContainerWidget::~ContainerWidget() {}

} // namespace Containers
} // namespace GuiManager
