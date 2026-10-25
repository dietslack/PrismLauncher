// SPDX-License-Identifier: GPL-3.0-only
#include "InstanceCardDelegate.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

#include "Application.h"
#include "InstanceList.h"
#include "icons/IconList.h"
#include "minecraft/MinecraftInstance.h"
#include "ui/instanceview/InstanceView.h"
#include "ui/reprism/InstanceSummary.h"
#include "ui/reprism/ReprismIcons.h"

namespace {
constexpr int Padding = 12;
constexpr int LineGap = 2;
constexpr int PlayHeight = 34;
constexpr int PlayGap = 10;
constexpr qreal CardRadius = 14.0;

QFont nameFont()
{
    QFont font = QApplication::font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 1.05);
    return font;
}

int textBlockHeight()
{
    return QFontMetrics(nameFont()).height() + QFontMetrics(QApplication::font()).height() + LineGap;
}

int infoHeight()
{
    return Padding + textBlockHeight() + Padding;
}

int expandHeight()
{
    return PlayGap + PlayHeight;
}

QColor mix(const QColor& a, const QColor& b, qreal t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

/// Average opaque color of an icon, used to tint the card's backdrop.
QColor iconTint(const QString& key, const QIcon& icon)
{
    static QHash<QString, QColor> s_cache;
    if (auto it = s_cache.constFind(key); it != s_cache.constEnd())
        return *it;
    const QImage image = icon.pixmap(32, 32).toImage().convertToFormat(QImage::Format_ARGB32);
    double r = 0, g = 0, b = 0, weight = 0;
    for (int y = 0; y < image.height(); y++) {
        for (int x = 0; x < image.width(); x++) {
            const QColor c = image.pixelColor(x, y);
            const double a = c.alphaF();
            r += c.redF() * a;
            g += c.greenF() * a;
            b += c.blueF() * a;
            weight += a;
        }
    }
    QColor tint = weight > 0 ? QColor::fromRgbF(r / weight, g / weight, b / weight) : QColor(Qt::gray);
    s_cache.insert(key, tint);
    return tint;
}

/// The icon's artwork with any fully transparent margin trimmed off, rendered for `size` physical pixels.
QImage trimmedIcon(const QString& key, const QIcon& icon, int size)
{
    static QHash<QString, QImage> s_cache;
    const QString cacheKey = QString("%1|%2").arg(key).arg(size);
    if (auto it = s_cache.constFind(cacheKey); it != s_cache.constEnd())
        return *it;
    // Vector icons render at the requested size; raster icons come back at their native size.
    const QImage image = icon.pixmap(size, size).toImage().convertToFormat(QImage::Format_ARGB32);
    int left = image.width(), top = image.height(), right = -1, bottom = -1;
    for (int y = 0; y < image.height(); y++) {
        const auto* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); x++) {
            if (qAlpha(line[x]) > 8) {
                left = qMin(left, x);
                right = qMax(right, x);
                top = qMin(top, y);
                bottom = qMax(bottom, y);
            }
        }
    }
    QImage trimmed = right < 0 ? image : image.copy(QRect(QPoint(left, top), QPoint(right, bottom)));
    s_cache.insert(cacheKey, trimmed);
    return trimmed;
}

/// Draws `image` as large as fits in `area` without cropping, centered, using nearest-neighbour scaling.
void drawFit(QPainter* painter, const QImage& image, const QRect& area)
{
    if (image.isNull())
        return;
    const qreal scale = qMin(qreal(area.width()) / image.width(), qreal(area.height()) / image.height());
    const QSizeF size(image.width() * scale, image.height() * scale);
    const QRectF target(area.center().x() + 0.5 - size.width() / 2, area.center().y() + 0.5 - size.height() / 2, size.width(), size.height());
    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter->drawImage(target, image);
    painter->restore();
}

/// The icon area is square: as tall as the card is wide.
int cardWidth(QAbstractItemView* view)
{
    auto* instanceView = qobject_cast<InstanceView*>(view);
    return instanceView ? instanceView->itemWidth() : InstanceCardDelegate::MinimumWidth;
}

QRect playRectFor(const QRect& card, qreal progress)
{
    const int panelTop = card.bottom() + 1 - infoHeight() - qRound(expandHeight() * progress);
    return QRect(card.left() + Padding, panelTop + Padding + textBlockHeight() + PlayGap, card.width() - 2 * Padding, PlayHeight);
}
}  // namespace

InstanceCardDelegate::InstanceCardDelegate(QAbstractItemView* view) : ListViewDelegate(view), m_view(view) {}

QSize InstanceCardDelegate::sizeHint([[maybe_unused]] const QStyleOptionViewItem& option, [[maybe_unused]] const QModelIndex& index) const
{
    const int width = cardWidth(m_view);
    return QSize(width, width + infoHeight());
}

QRect InstanceCardDelegate::playButtonRect(const QRect& itemRect)
{
    return playRectFor(itemRect, 1.0);
}

qreal InstanceCardDelegate::hoverProgress(const QModelIndex& index, bool hovered) const
{
    const QPersistentModelIndex key(index);
    const qreal current = m_progress.value(key, 0.0);
    const qreal target = hovered ? 1.0 : 0.0;
    auto* animation = m_animations.value(key);
    const bool heading = animation && animation->state() == QAbstractAnimation::Running && animation->endValue().toReal() == target;
    if (current == target || heading)
        return current;

    if (!animation) {
        animation = new QVariantAnimation(const_cast<InstanceCardDelegate*>(this));
        animation->setEasingCurve(QEasingCurve::OutCubic);
        connect(animation, &QVariantAnimation::valueChanged, this, [this, key](const QVariant& value) {
            m_progress[key] = value.toReal();
            if (m_view)
                m_view->viewport()->update();
        });
        m_animations.insert(key, animation);
    }
    animation->stop();
    animation->setStartValue(current);
    animation->setEndValue(target);
    animation->setDuration(qMax(1, qRound(180 * qAbs(target - current))));
    animation->start();
    return current;
}

void InstanceCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    auto* instance = static_cast<MinecraftInstance*>(index.data(InstanceList::InstancePointerRole).value<void*>());
    // Use the application palette: the view itself is styled transparent, which blanks its own palette.
    const QPalette pal = QApplication::palette();
    const bool selected = option.state & QStyle::State_Selected;
    const qreal progress = hoverProgress(index, option.state & QStyle::State_MouseOver);
    const QRect card = option.rect;
    const qreal dpr = painter->device()->devicePixelRatioF();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);

    QPainterPath cardPath;
    cardPath.addRoundedRect(QRectF(card).adjusted(0.5, 0.5, -0.5, -0.5), CardRadius, CardRadius);
    painter->setClipPath(cardPath);

    const QColor surface = pal.color(QPalette::Button);
    painter->fillRect(card, surface);

    // Icon area: backdrop tinted from the icon's colors (shows through transparent icons), icon filling the area.
    const QString iconKey = instance ? instance->iconKey() : QStringLiteral("default");
    const QIcon icon = APPLICATION->icons()->getIcon(iconKey);
    const QRect iconArea(card.left(), card.top(), card.width(), card.width() + qRound(CardRadius));
    {
        const QColor base = pal.color(QPalette::Window);
        const QColor tint = iconTint(iconKey, icon);
        QLinearGradient gradient(iconArea.topLeft(), iconArea.bottomLeft());
        gradient.setColorAt(0.0, mix(base, tint, 0.38));
        gradient.setColorAt(1.0, mix(base, tint, 0.16));
        painter->fillRect(iconArea, gradient);

        // The icon's artwork (transparent margins trimmed) fills the square icon area without cropping.
        const QRect square(card.left(), card.top(), card.width(), card.width());
        drawFit(painter, trimmedIcon(iconKey, icon, qRound(square.width() * dpr)), square);
    }

    // Chips over the icon's top-right corner: mod loader icon, then the pack version (or Minecraft version).
    {
        constexpr int ChipHeight = 26, ChipMargin = 10, ChipGap = 6, ChipRadius = 8, LoaderIcon = 16;
        QColor chipColor = pal.color(QPalette::Button);
        chipColor.setAlphaF(0.88);
        QFont chipFont = QApplication::font();
        chipFont.setBold(true);
        chipFont.setPointSizeF(chipFont.pointSizeF() * 0.9);
        const QFontMetrics chipMetrics(chipFont);

        int right = card.right() - ChipMargin + 1;
        const int top = card.top() + ChipMargin;
        const QString version = InstanceSummary::versionChip(instance);
        if (!version.isEmpty()) {
            const int maxText = card.width() / 2;
            const QString text = chipMetrics.elidedText(version, Qt::ElideRight, maxText);
            const QRect chip(right - (chipMetrics.horizontalAdvance(text) + 20), top, chipMetrics.horizontalAdvance(text) + 20, ChipHeight);
            painter->setPen(Qt::NoPen);
            painter->setBrush(chipColor);
            painter->drawRoundedRect(chip, ChipRadius, ChipRadius);
            painter->setFont(chipFont);
            painter->setPen(pal.color(QPalette::Text));
            painter->drawText(chip, Qt::AlignCenter, text);
            right = chip.left() - ChipGap;
        }
        const QString loaderIcon = InstanceSummary::components(instance).loaderIconKey;
        if (!loaderIcon.isEmpty()) {
            const QRect chip(right - ChipHeight, top, ChipHeight, ChipHeight);
            painter->setPen(Qt::NoPen);
            painter->setBrush(chipColor);
            painter->drawRoundedRect(chip, ChipRadius, ChipRadius);
            const QImage image = ReprismIcons::nearestNeighbor(APPLICATION->icons()->getIcon(loaderIcon), LoaderIcon, dpr);
            painter->drawImage(QRect(chip.center().x() - LoaderIcon / 2 + 1, chip.center().y() - LoaderIcon / 2 + 1, LoaderIcon, LoaderIcon), image);
        }
    }

    // Info panel, sliding up over the icon as the card is hovered.
    const int panelHeight = infoHeight() + qRound(expandHeight() * progress);
    const QRect panel(card.left(), card.bottom() + 1 - panelHeight, card.width(), panelHeight);
    QColor panelColor = pal.color(QPalette::Light);
    panelColor = mix(surface, panelColor, progress * 0.6);
    if (progress > 0) {
        QPainterPath panelPath;
        panelPath.addRoundedRect(QRectF(panel), CardRadius * progress, CardRadius * progress);
        painter->fillPath(panelPath, panelColor);
    } else {
        painter->fillRect(panel, surface);
    }

    const QColor text = pal.color(QPalette::Text);
    const QColor muted = mix(text, panelColor, 0.42);
    const int textLeft = card.left() + Padding;
    const int textWidth = card.width() - 2 * Padding;
    int y = panel.top() + Padding;

    const QFont boldFont = nameFont();
    const QFontMetrics nameMetrics(boldFont);
    const QFontMetrics metrics(QApplication::font());
    painter->setFont(boldFont);
    painter->setPen(text);
    painter->drawText(QRect(textLeft, y, textWidth, nameMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                      nameMetrics.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, textWidth));
    y += nameMetrics.height() + LineGap;

    QString secondary = InstanceSummary::creatorLine(instance);
    if (secondary.isEmpty())
        secondary = InstanceSummary::lastPlayedLine(instance);
    painter->setFont(QApplication::font());
    painter->setPen(muted);
    painter->drawText(QRect(textLeft, y, textWidth, metrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                      metrics.elidedText(secondary, Qt::ElideRight, textWidth));

    // Play / Stop button, revealed by the hover expansion.
    if (progress > 0) {
        const bool running = instance && instance->isRunning();
        const QRect play = playRectFor(card, progress);
        auto* instanceView = qobject_cast<InstanceView*>(m_view);
        const bool playHovered = progress >= 1.0 && instanceView && play.contains(instanceView->hoverPosition());
        painter->setOpacity(progress);
        painter->setPen(Qt::NoPen);
        QColor fill = running ? pal.color(QPalette::Mid) : pal.color(QPalette::Highlight);
        if (playHovered) {
            // Clear hover feedback: a soft ring around the button and a lighter (dark theme) / darker (light theme) fill.
            QColor ring = fill;
            ring.setAlphaF(0.35);
            painter->setBrush(ring);
            painter->drawRoundedRect(QRectF(play).adjusted(-3, -3, 3, 3), 13, 13);
            const bool darkTheme = pal.color(QPalette::Window).lightnessF() < 0.5;
            fill = darkTheme ? fill.lighter(running ? 140 : 108) : fill.lighter(running ? 90 : 190);
            if (!running && darkTheme)
                fill = Qt::white;
        }
        painter->setBrush(fill);
        painter->drawRoundedRect(play, 10, 10);
        const QColor fg = running ? text : pal.color(QPalette::HighlightedText);
        const QString label = running ? tr("Stop") : tr("Play");
        QFont buttonFont = QApplication::font();
        buttonFont.setBold(true);
        const int glyph = 14, gap = 6;
        const int labelWidth = QFontMetrics(buttonFont).horizontalAdvance(label);
        const int contentLeft = play.center().x() - (glyph + gap + labelWidth) / 2;
        painter->drawPixmap(contentLeft, play.center().y() - glyph / 2 + 1,
                            ReprismIcons::pixmap(running ? "square" : "play", fg, glyph, dpr, true));
        painter->setFont(buttonFont);
        painter->setPen(fg);
        painter->drawText(QRect(contentLeft + glyph + gap, play.top(), labelWidth + 2, play.height()), Qt::AlignLeft | Qt::AlignVCenter, label);
        painter->setOpacity(1.0);
    }

    painter->setClipping(false);
    if (selected) {
        painter->setPen(QPen(pal.color(QPalette::Highlight), 1.5));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(QRectF(card).adjusted(1, 1, -1, -1), CardRadius, CardRadius);
    }
    painter->restore();
}

void InstanceCardDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, [[maybe_unused]] const QModelIndex& index) const
{
    const QRect card = option.rect;
    const int top = card.bottom() + 1 - infoHeight() + Padding - 4;
    editor->setGeometry(card.left() + Padding - 4, top, card.width() - 2 * Padding + 8, editor->fontMetrics().height() + 10);
}
