// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>

/// Lucide line icons bundled in resources/reprism/icons, tinted to any color.
namespace ReprismIcons {
/// Renders icon `name` at `size` logical pixels. `filled` also fills closed shapes (e.g. a solid play triangle).
QPixmap pixmap(const QString& name, const QColor& color, int size, qreal devicePixelRatio = 1.0, bool filled = false);
/// A QIcon tinted with `color` (and a faded variant for the disabled state).
QIcon icon(const QString& name, const QColor& color, bool filled = false);
}  // namespace ReprismIcons
