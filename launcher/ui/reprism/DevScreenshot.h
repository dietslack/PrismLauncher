// SPDX-License-Identifier: GPL-3.0-only
#pragma once

namespace DevScreenshot {
// Development aid: when REPRISM_SCREENSHOT_DIR is set, saves every visible top-level window as a PNG
// after REPRISM_SCREENSHOT_DELAY ms (default 3000) and exits. REPRISM_SCREENSHOT_TRIGGER may name a
// QAction (by objectName) to trigger first, e.g. "actionSettings", to capture a dialog, or "wizard:next"
// to advance/finish the setup wizard. REPRISM_SCREENSHOT_SIZE (e.g. "1200x760") resizes the main window,
// REPRISM_SCREENSHOT_HOVER ("objectName:x,y") simulates hovering a widget, and REPRISM_SCREENSHOT_TYPE
// ("objectName:text") sets the text of a line edit (or the index of a combo box). REPRISM_SCREENSHOT_CONTEXT ("objectName:x,y") opens a
// context menu and REPRISM_SCREENSHOT_CLICK ("objectName") clicks a button, e.g. to open its menu.
// REPRISM_SCREENSHOT_PRESS ("objectName:x,y;...") clicks points in order, e.g. a card and then a tab.
void install();
}  // namespace DevScreenshot
