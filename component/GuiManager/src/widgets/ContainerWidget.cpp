// MIT License
// Copyright (c) 2026 Sourcerer

#include "widgets/ContainerWidget.h"

namespace GuiManager::Widgets {

ContainerWidget::ContainerWidget(const ContainerType type, QWidget *parent)
    : QWidget{parent}, _type(type) {}

ContainerWidget::~ContainerWidget() {}

} // namespace GuiManager::Widgets