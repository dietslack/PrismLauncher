// SPDX-License-Identifier: GPL-3.0-only
#include "ReprismIcons.h"

#include <QBuffer>
#include <QFile>
#include <QHash>
#include <QImageReader>

namespace ReprismIcons {

static QByteArray tintedSvg(const QString& name, const QColor& color, bool filled)
{
    static QHash<QString, QByteArray> s_sources;
    auto it = s_sources.find(name);
    if (it == s_sources.end()) {
        QFile file(QString(":/reprism/icons/%1.svg").arg(name));
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "ReprismIcons: missing icon" << name;
            return {};
        }
        it = s_sources.insert(name, file.readAll());
    }
    QByteArray svg = *it;
    const QByteArray colorName = color.name(QColor::HexRgb).toLatin1();
    svg.replace("currentColor", colorName);
    if (color.alpha() < 255)
        svg.replace("<svg", "<svg opacity=\"" + QByteArray::number(color.alphaF(), 'f', 3) + "\"");
    if (filled)
        svg.replace("fill=\"none\"", "fill=\"" + colorName + "\"");
    return svg;
}

QPixmap pixmap(const QString& name, const QColor& color, int size, qreal devicePixelRatio, bool filled)
{
    static QHash<QString, QPixmap> s_cache;
    const QString key = QString("%1|%2|%3|%4|%5").arg(name, color.name(QColor::HexArgb)).arg(size).arg(devicePixelRatio).arg(filled);
    if (auto it = s_cache.constFind(key); it != s_cache.constEnd())
        return *it;

    QByteArray svg = tintedSvg(name, color, filled);
    QBuffer buffer(&svg);
    QImageReader reader(&buffer, "svg");
    const int physical = qRound(size * devicePixelRatio);
    reader.setScaledSize(QSize(physical, physical));
    QPixmap result = QPixmap::fromImage(reader.read());
    result.setDevicePixelRatio(devicePixelRatio);
    s_cache.insert(key, result);
    return result;
}

QIcon icon(const QString& name, const QColor& color, bool filled)
{
    QIcon result;
    QColor disabled = color;
    disabled.setAlphaF(0.4);
    for (int size : { 16, 20, 24, 32, 48 }) {
        for (qreal dpr : { 1.0, 2.0 }) {
            result.addPixmap(pixmap(name, color, size, dpr, filled), QIcon::Normal);
            result.addPixmap(pixmap(name, disabled, size, dpr, filled), QIcon::Disabled);
        }
    }
    return result;
}

}  // namespace ReprismIcons
