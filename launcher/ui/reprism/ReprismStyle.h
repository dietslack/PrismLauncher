// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QProxyStyle>

/// Proxy style used by the RePrism themes. Gives popup menus and combo box popups a translucent window so the stylesheet's
/// rounded corners render cleanly instead of showing square window edges.
class ReprismStyle : public QProxyStyle {
    Q_OBJECT
   public:
    explicit ReprismStyle(QStyle* base);

    void polish(QWidget* widget) override;
    void unpolish(QWidget* widget) override;
    using QProxyStyle::polish;
    using QProxyStyle::unpolish;
};
