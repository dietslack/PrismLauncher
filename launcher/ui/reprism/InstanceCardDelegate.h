// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "ui/instanceview/InstanceDelegate.h"

/// Paints instances as Modrinth-style cards: rounded tile, icon on the left, name and version/last-played
/// lines on the right, and a play button over the icon on hover. Keeps ListViewDelegate's inline renaming.
class InstanceCardDelegate : public ListViewDelegate {
    Q_OBJECT
   public:
    explicit InstanceCardDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    static constexpr int MinimumWidth = 250;
    /// Area of the hover play button for a card occupying `itemRect`.
    static QRect playButtonRect(const QRect& itemRect);
};
