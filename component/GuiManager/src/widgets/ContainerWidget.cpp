#include "widgets/ContainerWidget.h"

namespace GuiManager {

namespace Widgets {

ContainerWidget::ContainerWidget(ContainerType type, QWidget *parent)
    : QWidget{parent}, _type(type) {}

ContainerWidget::~ContainerWidget() {}

} // namespace Widgets
} // namespace GuiManager
