// SPDX-License-Identifier: GPL-3.0-only
#pragma once

class QDialog;
class QWidget;

/// Shows dialogs inside the main window, Modrinth-style: the window dims and the dialog appears in a rounded card
/// with a title and close button, instead of as a separate OS window.
namespace InAppModal {
/// Runs `dialog` like QDialog::exec(), but embedded in `host`. By default that is the main window when the dialog
/// belongs to it; dialogs of other windows run normally. Returns the dialog's result code.
int exec(QDialog* dialog, QWidget* host = nullptr);
}  // namespace InAppModal
