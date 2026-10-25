// SPDX-License-Identifier: GPL-3.0-only
#include "ReprismStyle.h"

#include <QMenu>

ReprismStyle::ReprismStyle(QStyle* base) : QProxyStyle(base) {}

static bool isPopup(QWidget* widget)
{
    return qobject_cast<QMenu*>(widget) || widget->inherits("QComboBoxPrivateContainer");
}

void ReprismStyle::polish(QWidget* widget)
{
    if (isPopup(widget)) {
        widget->setAttribute(Qt::WA_TranslucentBackground, true);
        widget->setWindowFlag(Qt::NoDropShadowWindowHint, true);
    }
    QProxyStyle::polish(widget);
}

void ReprismStyle::unpolish(QWidget* widget)
{
    if (isPopup(widget)) {
        widget->setAttribute(Qt::WA_TranslucentBackground, false);
        widget->setWindowFlag(Qt::NoDropShadowWindowHint, false);
    }
    QProxyStyle::unpolish(widget);
}
