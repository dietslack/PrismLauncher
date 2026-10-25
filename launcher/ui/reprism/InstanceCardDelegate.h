// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QHash>
#include <QPersistentModelIndex>

#include "ui/instanceview/InstanceDelegate.h"

class QAbstractItemView;
class QVariantAnimation;

/// Paints instances as tall cards: a large icon on a tinted backdrop, with an info panel below (name,
/// loader/version, creator or last played). On hover the info panel slides up over the icon and reveals a
/// Play button. Keeps ListViewDelegate's inline renaming.
class InstanceCardDelegate : public ListViewDelegate {
    Q_OBJECT
   public:
    explicit InstanceCardDelegate(QAbstractItemView* view);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    static constexpr int MinimumWidth = 200;
    /// Area of the Play button of a fully hovered card occupying `itemRect`.
    static QRect playButtonRect(const QRect& itemRect);

   private:
    /// Hover progress (0..1) for `index`, animating towards `hovered`.
    qreal hoverProgress(const QModelIndex& index, bool hovered) const;

    QAbstractItemView* m_view;
    mutable QHash<QPersistentModelIndex, QVariantAnimation*> m_animations;
    mutable QHash<QPersistentModelIndex, qreal> m_progress;
};
