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
#include "ui/reprism/InstanceSummary.h"
#include "ui/reprism/ReprismIcons.h"

namespace {
constexpr int IconAreaHeight = 150;
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
    return QFontMetrics(nameFont()).height() + 2 * (QFontMetrics(QApplication::font()).height() + LineGap);
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

/// Icon pixmap at `size`, upscaled without smoothing when the source is small pixel art.
QPixmap iconPixmap(const QIcon& icon, int size, qreal dpr)
{
    const QSize target(qRound(size * dpr), qRound(size * dpr));
    QPixmap pm = icon.pixmap(target);
    if (pm.width() < target.width())
        pm = pm.scaled(target, Qt::KeepAspectRatio, pm.width() * 2 <= target.width() ? Qt::FastTransformation : Qt::SmoothTransformation);
    pm.setDevicePixelRatio(dpr);
    return pm;
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
    return QSize(MinimumWidth, IconAreaHeight + infoHeight());
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

    // Icon area: backdrop tinted from the icon's colors, with the icon large in the middle.
    const QString iconKey = instance ? instance->iconKey() : QStringLiteral("default");
    const QIcon icon = APPLICATION->icons()->getIcon(iconKey);
    const QRect iconArea(card.left(), card.top(), card.width(), IconAreaHeight + qRound(CardRadius));
    {
        const QColor base = pal.color(QPalette::Window);
        const QColor tint = iconTint(iconKey, icon);
        QLinearGradient gradient(iconArea.topLeft(), iconArea.bottomLeft());
        gradient.setColorAt(0.0, mix(base, tint, 0.38));
        gradient.setColorAt(1.0, mix(base, tint, 0.16));
        painter->fillRect(iconArea, gradient);

        const int size = qMin(96, IconAreaHeight - 2 * Padding - 16);
        const QPixmap pm = iconPixmap(icon, size, dpr);
        painter->drawPixmap(QRect(card.center().x() - size / 2, card.top() + (IconAreaHeight - size) / 2, size, size), pm);
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
    for (const QString& line : { InstanceSummary::versionLine(instance), secondary }) {
        painter->drawText(QRect(textLeft, y, textWidth, metrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                          metrics.elidedText(line, Qt::ElideRight, textWidth));
        y += metrics.height() + LineGap;
    }

    // Play / Stop button, revealed by the hover expansion.
    if (progress > 0) {
        const bool running = instance && instance->isRunning();
        const QRect play = playRectFor(card, progress);
        painter->setOpacity(progress);
        painter->setPen(Qt::NoPen);
        painter->setBrush(running ? pal.color(QPalette::Mid) : pal.color(QPalette::Highlight));
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
