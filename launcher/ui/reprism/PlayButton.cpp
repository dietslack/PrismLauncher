// SPDX-License-Identifier: GPL-3.0-only
#include "PlayButton.h"

#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>

#include "minecraft/MinecraftInstance.h"
#include "ui/reprism/ReprismIcons.h"

namespace {
constexpr int IconSize = 16;
constexpr int IconGap = 6;
}  // namespace

PlayButton::PlayButton(QAction* launch, QAction* kill, QWidget* parent) : QWidget(parent), m_launch(launch), m_kill(kill)
{
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);  // one split button

    m_button = new QPushButton(this);
    m_button->setObjectName(QStringLiteral("reprismPlayButton"));
    m_button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_button->setIconSize(QSize(IconSize + IconGap, IconSize));
    m_button->setCursor(Qt::PointingHandCursor);
    connect(m_button, &QPushButton::clicked, this, [this] {
        if (m_instance && m_instance->isRunning())
            m_kill->trigger();
        else
            m_launch->trigger();
    });

    m_menuButton = new QToolButton(this);
    m_menuButton->setObjectName(QStringLiteral("reprismPlayMenuButton"));
    m_menuButton->setFixedWidth(40);
    m_menuButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);  // match the Play button's height
    m_menuButton->setPopupMode(QToolButton::InstantPopup);
    m_menuButton->setProperty("hideMenuIndicator", true);
    m_menuButton->setToolTip(tr("Launch options"));

    row->addWidget(m_button, 1);
    row->addWidget(m_menuButton);

    for (auto* action : { m_launch, m_kill })
        connect(action, &QAction::changed, this, &PlayButton::refresh);
    refresh();
}

void PlayButton::setInstance(MinecraftInstance* instance)
{
    m_instance = instance;
    refresh();
}

void PlayButton::refresh()
{
    const bool running = m_instance && m_instance->isRunning();
    const bool hasMenu = !running && m_launch->menu();
    m_button->setText(running ? tr("Stop") : tr("Play"));
    m_button->setProperty("running", running);
    m_button->setProperty("joined", hasMenu);
    m_button->setEnabled(running ? m_kill->isEnabled() : m_launch->isEnabled());
    m_button->style()->unpolish(m_button);
    m_button->style()->polish(m_button);
    m_menuButton->setMenu(m_launch->menu());
    m_menuButton->setVisible(hasMenu);
    m_menuButton->setEnabled(m_launch->isEnabled());

    // Pad the glyph on the right: QPushButton has no icon/text spacing of its own.
    const QPalette pal = palette();
    const QColor glyphColor = running ? pal.color(QPalette::ButtonText) : pal.color(QPalette::HighlightedText);
    QIcon playIcon;
    for (qreal dpr : { 1.0, 2.0 }) {
        QPixmap padded(qRound((IconSize + IconGap) * dpr), qRound(IconSize * dpr));
        padded.setDevicePixelRatio(dpr);
        padded.fill(Qt::transparent);
        QPainter painter(&padded);
        painter.drawPixmap(0, 0, ReprismIcons::pixmap(running ? "square" : "play", glyphColor, IconSize, dpr, true));
        painter.end();
        playIcon.addPixmap(padded);
    }
    m_button->setIcon(playIcon);
    m_menuButton->setIcon(ReprismIcons::icon("chevron-down", pal.color(QPalette::HighlightedText)));
}

void PlayButton::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange)
        refresh();
    QWidget::changeEvent(event);
}
