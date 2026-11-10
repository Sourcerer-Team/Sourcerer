// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <Qt>

class QWidget;

namespace GuiManager {

// Starts an interactive resize of a frameless top-level window by dragging the
// given edge(s) with the left mouse button, which must currently be pressed.
//
// QWindow::startSystemResize() is used where the platform implements it.
// Cocoa has no API for that (a borderless NSWindow cannot be resized by the
// user at all), so on macOS the resize is driven by polling the cursor until
// the button is released.
//
// Returns false if no resize could be started.
bool beginInteractiveResize(QWidget *window, Qt::Edges edges);

} // namespace GuiManager
