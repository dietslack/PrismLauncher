// SPDX-License-Identifier: GPL-3.0-only
#pragma once

namespace DevScreenshot {
// Development aid: when REPRISM_SCREENSHOT_DIR is set, saves every visible top-level window as a PNG
// after REPRISM_SCREENSHOT_DELAY ms (default 3000) and exits. REPRISM_SCREENSHOT_TRIGGER may name a
// QAction (by objectName) to trigger first, e.g. "actionSettings", to capture a dialog.
void install();
}  // namespace DevScreenshot
