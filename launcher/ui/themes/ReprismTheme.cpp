// SPDX-License-Identifier: GPL-3.0-only
#include "ReprismTheme.h"

#include <QFile>
#include <QObject>

ReprismTheme::ReprismTheme(bool dark) : m_dark(dark)
{
    if (dark) {
        m_tokens = {
            { "window", QColor(0x16, 0x18, 0x1d) },        { "surface", QColor(0x1d, 0x20, 0x27) },
            { "surfaceAlt", QColor(0x21, 0x24, 0x2c) },    { "surface2", QColor(0x26, 0x2a, 0x33) },
            { "surface3", QColor(0x30, 0x35, 0x40) },      { "input", QColor(0x12, 0x14, 0x18) },
            { "border", QColor(0x2c, 0x31, 0x3b) },        { "borderStrong", QColor(0x3d, 0x43, 0x50) },
            { "text", QColor(0xe7, 0xe9, 0xee) },          { "muted", QColor(0x8d, 0x94, 0xa1) },
            { "accent", QColor(0x7c, 0x6c, 0xf2) },        { "accentHover", QColor(0x8f, 0x81, 0xff) },
            { "accentPressed", QColor(0x6a, 0x5a, 0xdd) }, { "accentText", QColor(0xff, 0xff, 0xff) },
            { "accentSoft", QColor(0x7c, 0x6c, 0xf2, 0x4d) }, { "scroll", QColor(0x3a, 0x40, 0x4c) },
            { "scrollHover", QColor(0x52, 0x59, 0x68) },   { "link", QColor(0x9d, 0x91, 0xff) },
        };
    } else {
        m_tokens = {
            { "window", QColor(0xf3, 0xf4, 0xf7) },        { "surface", QColor(0xff, 0xff, 0xff) },
            { "surfaceAlt", QColor(0xf7, 0xf8, 0xfa) },    { "surface2", QColor(0xec, 0xee, 0xf2) },
            { "surface3", QColor(0xe0, 0xe3, 0xe9) },      { "input", QColor(0xff, 0xff, 0xff) },
            { "border", QColor(0xdc, 0xdf, 0xe6) },        { "borderStrong", QColor(0xc3, 0xc8, 0xd2) },
            { "text", QColor(0x1b, 0x1e, 0x24) },          { "muted", QColor(0x66, 0x6d, 0x7a) },
            { "accent", QColor(0x66, 0x52, 0xe6) },        { "accentHover", QColor(0x75, 0x62, 0xf0) },
            { "accentPressed", QColor(0x55, 0x42, 0xd0) }, { "accentText", QColor(0xff, 0xff, 0xff) },
            { "accentSoft", QColor(0x66, 0x52, 0xe6, 0x33) }, { "scroll", QColor(0xc6, 0xcb, 0xd4) },
            { "scrollHover", QColor(0xa9, 0xb0, 0xbc) },   { "link", QColor(0x55, 0x42, 0xd0) },
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
