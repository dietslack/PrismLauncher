// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QColor>
#include <QMap>

#include "FusionTheme.h"

/// The default RePrism theme: Fusion with a modern palette and stylesheet, in dark and light variants.
class ReprismTheme : public FusionTheme {
   public:
    explicit ReprismTheme(bool dark);
    ~ReprismTheme() override = default;

    QString id() override;
    QString name() override;
    QString tooltip() override;
    bool hasStyleSheet() override;
    QString appStyleSheet() override;
    QPalette colorScheme() override;
    double fadeAmount() override;
    QColor fadeColor() override;
    QStringList searchPaths() override;

    /// Named colors of this variant, as used for the @name@ placeholders in the stylesheet.
    const QMap<QString, QColor>& tokens() const { return m_tokens; }

   private:
    bool m_dark;
    QMap<QString, QColor> m_tokens;
};
