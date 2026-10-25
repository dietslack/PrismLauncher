// SPDX-License-Identifier: GPL-3.0-only
#include "InstanceCardDelegate.h"

#include <QApplication>
#include <QPainter>
#include <QPainterPath>

#include "Application.h"
#include "InstanceList.h"
#include "icons/IconList.h"
#include "minecraft/MinecraftInstance.h"
#include "ui/reprism/InstanceSummary.h"
#include "ui/reprism/ReprismIcons.h"

namespace {
constexpr int CardHeight = 80;
constexpr int Padding = 12;
constexpr int IconSize = CardHeight - 2 * Padding;
constexpr qreal CardRadius = 12.0;
constexpr qreal IconRadius = 10.0;

QRect iconRect(const QRect& card)
{
    return QRect(card.left() + Padding, card.top() + Padding, IconSize, IconSize);
}

QColor mix(const QColor& a, const QColor& b, qreal t)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t);
}
}  // namespace

InstanceCardDelegate::InstanceCardDelegate(QObject* parent) : ListViewDelegate(parent) {}

QSize InstanceCardDelegate::sizeHint([[maybe_unused]] const QStyleOptionViewItem& option, [[maybe_unused]] const QModelIndex& index) const
{
    return QSize(MinimumWidth, CardHeight);
}

QRect InstanceCardDelegate::playButtonRect(const QRect& itemRect)
{
    return iconRect(itemRect);
}

void InstanceCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    auto* instance = static_cast<MinecraftInstance*>(index.data(InstanceList::InstancePointerRole).value<void*>());
    // Use the application palette: the view itself is styled transparent, which blanks its own palette.
    const QPalette pal = QApplication::palette();
    const bool hovered = option.state & QStyle::State_MouseOver;
    const bool selected = option.state & QStyle::State_Selected;
    const QRect card = option.rect;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // Card background: raised surface, a little lighter on hover; selected cards get an accent outline.
    QColor background = pal.color(QPalette::Button);
    if (hovered || selected)
        background = pal.color(QPalette::Light);
    painter->setPen(Qt::NoPen);
    painter->setBrush(background);
    painter->drawRoundedRect(QRectF(card).adjusted(0.5, 0.5, -0.5, -0.5), CardRadius, CardRadius);
    if (selected) {
        painter->setPen(QPen(pal.color(QPalette::Highlight), 1.5));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(QRectF(card).adjusted(1, 1, -1, -1), CardRadius, CardRadius);
    }

    // Icon, clipped to a rounded square.
    const QRect icon = iconRect(card);
    {
        painter->save();
        QPainterPath clip;
        clip.addRoundedRect(icon, IconRadius, IconRadius);
        painter->setClipPath(clip);
        painter->fillRect(icon, pal.color(QPalette::Window));
        const QIcon instanceIcon = APPLICATION->icons()->getIcon(instance ? instance->iconKey() : QStringLiteral("default"));
        instanceIcon.paint(painter, icon.adjusted(4, 4, -4, -4));

        if (hovered) {
            // Dim the icon and show a round play (or stop, while running) button over it.
            painter->fillRect(icon, QColor(0, 0, 0, 110));
            const int buttonSize = 34;
            const QRect button(icon.center().x() - buttonSize / 2 + 1, icon.center().y() - buttonSize / 2 + 1, buttonSize, buttonSize);
            painter->setPen(Qt::NoPen);
            painter->setBrush(pal.color(QPalette::Highlight));
            painter->drawEllipse(button);
            const bool running = instance && instance->isRunning();
            const int glyph = 16;
            const QPixmap pm = ReprismIcons::pixmap(running ? "square" : "play", pal.color(QPalette::HighlightedText), glyph,
                                                    painter->device()->devicePixelRatioF(), true);
            painter->drawPixmap(button.center().x() - glyph / 2 + (running ? 1 : 2), button.center().y() - glyph / 2 + 1, pm);
        }
        painter->restore();
    }

    // Text block: name, then version and last-played lines in a muted color.
    const int textLeft = icon.right() + Padding;
    const int textWidth = card.right() - Padding - textLeft;
    QFont nameFont = option.font;
    nameFont.setBold(true);
    nameFont.setPointSizeF(nameFont.pointSizeF() * 1.05);
    const QFontMetrics nameMetrics(nameFont);
    const QFontMetrics metrics(option.font);
    const int lineGap = 2;
    const int blockHeight = nameMetrics.height() + 2 * (metrics.height() + lineGap);
    int y = card.top() + (card.height() - blockHeight) / 2;

    const QColor text = pal.color(QPalette::Text);
    const QColor muted = mix(text, background, 0.42);

    painter->setFont(nameFont);
    painter->setPen(text);
    painter->drawText(QRect(textLeft, y, textWidth, nameMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                      nameMetrics.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, textWidth));
    y += nameMetrics.height() + lineGap;

    painter->setFont(option.font);
    painter->setPen(muted);
    for (const QString& line : { InstanceSummary::versionLine(instance), InstanceSummary::lastPlayedLine(instance) }) {
        painter->drawText(QRect(textLeft, y, textWidth, metrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                          metrics.elidedText(line, Qt::ElideRight, textWidth));
        y += metrics.height() + lineGap;
    }

    painter->restore();
}

void InstanceCardDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, [[maybe_unused]] const QModelIndex& index) const
{
    const QRect card = option.rect;
    const int left = iconRect(card).right() + Padding - 4;
    const int height = editor->fontMetrics().height() + 10;
    editor->setGeometry(left, card.top() + Padding - 2, card.right() - Padding - left, height);
}
