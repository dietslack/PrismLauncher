// SPDX-License-Identifier: GPL-3.0-only
#include "ReprismTheme.h"

#include <QFile>
#include <QObject>

ReprismTheme::ReprismTheme(bool dark) : m_dark(dark)
{
    if (dark) {
        m_tokens = {
            { "window", QColor(0x16, 0x16, 0x16) },        { "surface", QColor(0x1e, 0x1e, 0x1e) },
            { "surfaceAlt", QColor(0x22, 0x22, 0x22) },    { "surface2", QColor(0x2a, 0x2a, 0x2a) },
            { "surface3", QColor(0x34, 0x34, 0x34) },      { "input", QColor(0x12, 0x12, 0x12) },
            { "border", QColor(0x2f, 0x2f, 0x2f) },        { "borderStrong", QColor(0x42, 0x42, 0x42) },
            { "text", QColor(0xe8, 0xe8, 0xe8) },          { "muted", QColor(0x9a, 0x9a, 0x9a) },
            { "accent", QColor(0xec, 0xec, 0xec) },        { "accentHover", QColor(0xff, 0xff, 0xff) },
            { "accentPressed", QColor(0xcc, 0xcc, 0xcc) }, { "accentText", QColor(0x16, 0x16, 0x16) },
            { "selected", QColor(0x3c, 0x3c, 0x3c) }, { "scroll", QColor(0x3d, 0x3d, 0x3d) },
            { "scrollHover", QColor(0x56, 0x56, 0x56) },   { "link", QColor(0xf4, 0xf4, 0xf4) },
            { "focus", QColor(0x8c, 0x8c, 0x8c) },          { "progress", QColor(0x70, 0x70, 0x70) },
            { "accentDivider", QColor(0x00, 0x00, 0x00, 0x30) },
        };
    } else {
        m_tokens = {
            { "window", QColor(0xf4, 0xf4, 0xf4) },        { "surface", QColor(0xff, 0xff, 0xff) },
            { "surfaceAlt", QColor(0xf8, 0xf8, 0xf8) },    { "surface2", QColor(0xed, 0xed, 0xed) },
            { "surface3", QColor(0xe1, 0xe1, 0xe1) },      { "input", QColor(0xff, 0xff, 0xff) },
            { "border", QColor(0xdd, 0xdd, 0xdd) },        { "borderStrong", QColor(0xc6, 0xc6, 0xc6) },
            { "text", QColor(0x1c, 0x1c, 0x1c) },          { "muted", QColor(0x68, 0x68, 0x68) },
            { "accent", QColor(0x1c, 0x1c, 0x1c) },        { "accentHover", QColor(0x36, 0x36, 0x36) },
            { "accentPressed", QColor(0x00, 0x00, 0x00) }, { "accentText", QColor(0xff, 0xff, 0xff) },
            { "selected", QColor(0xdc, 0xdc, 0xdc) }, { "scroll", QColor(0xc8, 0xc8, 0xc8) },
            { "scrollHover", QColor(0xab, 0xab, 0xab) },   { "link", QColor(0x00, 0x00, 0x00) },
            { "focus", QColor(0x6e, 0x6e, 0x6e) },          { "progress", QColor(0xa8, 0xa8, 0xa8) },
            { "accentDivider", QColor(0xff, 0xff, 0xff, 0x40) },
        };
    }
}

QString ReprismTheme::id()
{
    return m_dark ? "reprism_dark" : "reprism_light";
}

QString ReprismTheme::name()
{
    return m_dark ? QObject::tr("RePrism Dark") : QObject::tr("RePrism Light");
}

QString ReprismTheme::tooltip()
{
    return QObject::tr("The default RePrism Launcher theme");
}

QPalette ReprismTheme::colorScheme()
{
    const auto& t = m_tokens;
    QPalette palette;
    palette.setColor(QPalette::Window, t["window"]);
    palette.setColor(QPalette::WindowText, t["text"]);
    palette.setColor(QPalette::Base, t["surface"]);
    palette.setColor(QPalette::AlternateBase, t["surfaceAlt"]);
    palette.setColor(QPalette::ToolTipBase, t["surface2"]);
    palette.setColor(QPalette::ToolTipText, t["text"]);
    palette.setColor(QPalette::Text, t["text"]);
    palette.setColor(QPalette::Button, t["surface2"]);
    palette.setColor(QPalette::ButtonText, t["text"]);
    palette.setColor(QPalette::BrightText, QColor(0xff, 0x5c, 0x5c));
    palette.setColor(QPalette::Link, t["link"]);
    palette.setColor(QPalette::LinkVisited, t["link"]);
    palette.setColor(QPalette::Highlight, t["accent"]);
    palette.setColor(QPalette::HighlightedText, t["accentText"]);
    palette.setColor(QPalette::PlaceholderText, t["muted"]);
    palette.setColor(QPalette::Light, t["surface3"]);
    palette.setColor(QPalette::Midlight, t["surface2"]);
    palette.setColor(QPalette::Mid, t["border"]);
    palette.setColor(QPalette::Dark, t["input"]);
    palette.setColor(QPalette::Shadow, QColor(0, 0, 0, m_dark ? 160 : 60));
    palette.setColor(QPalette::Accent, t["accent"]);
    return fadeInactive(palette, fadeAmount(), fadeColor());
}

double ReprismTheme::fadeAmount()
{
    return 0.5;
}

QColor ReprismTheme::fadeColor()
{
    return m_tokens["window"];
}

bool ReprismTheme::hasStyleSheet()
{
    return true;
}

QString ReprismTheme::appStyleSheet()
{
    QFile file(":/reprism/reprism.qss");
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QString styleSheet = QString::fromUtf8(file.readAll());
    for (auto it = m_tokens.cbegin(); it != m_tokens.cend(); ++it) {
        const QColor& c = it.value();
        const QString value = c.alpha() == 255 ? c.name() : QString("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
        styleSheet.replace('@' + it.key() + '@', value);
    }
    return styleSheet;
}

QStringList ReprismTheme::searchPaths()
{
    return { m_dark ? ":/reprism/dark" : ":/reprism/light" };
}
