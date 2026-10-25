// SPDX-License-Identifier: GPL-3.0-only
#include "InstanceDetailsPanel.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QMenu>
#include <QPushButton>
#include <QStackedLayout>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include "minecraft/MinecraftInstance.h"
#include "ui/reprism/InstanceSummary.h"
#include "ui/reprism/ReprismIcons.h"

namespace {
constexpr int PlayIconSize = 16;
constexpr int PlayIconGap = 6;
}  // namespace

InstanceDetailsPanel::InstanceDetailsPanel(const Actions& actions, QWidget* iconButton, QWidget* nameLabel, QWidget* parent)
    : QFrame(parent), m_actions(actions)
{
    setObjectName(QStringLiteral("reprismDetails"));
    setFixedWidth(300);
    m_stack = new QStackedLayout(this);

    // Empty state
    auto* empty = new QLabel(tr("Select an instance to see its details"), this);
    empty->setObjectName(QStringLiteral("reprismMuted"));
    empty->setAlignment(Qt::AlignCenter);
    empty->setWordWrap(true);
    m_stack->addWidget(empty);

    // Details
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 24, 20, 20);
    layout->setSpacing(6);

    iconButton->setParent(page);
    iconButton->setObjectName(QStringLiteral("reprismDetailsIcon"));
    iconButton->setFixedSize(112, 112);
    static_cast<QToolButton*>(iconButton)->setIconSize(QSize(88, 88));
    layout->addWidget(iconButton, 0, Qt::AlignHCenter);
    layout->addSpacing(8);

    nameLabel->setParent(page);
    nameLabel->setObjectName(QStringLiteral("reprismDetailsName"));
    QFont nameFont = nameLabel->font();
    nameFont.setBold(true);
    nameFont.setPointSizeF(nameFont.pointSizeF() * 1.35);
    nameLabel->setFont(nameFont);
    layout->addWidget(nameLabel);

    auto makeFact = [this, layout] {
        auto* label = new QLabel(this);
        label->setObjectName(QStringLiteral("reprismMuted"));
        label->setAlignment(Qt::AlignHCenter);
        layout->addWidget(label);
        return label;
    };
    m_versionLabel = makeFact();
    m_lastPlayedLabel = makeFact();
    m_playtimeLabel = makeFact();
    layout->addSpacing(16);

    // Play / Stop, with the launch options menu (offline, profilers, ...) on a small attached button.
    auto* playRow = new QHBoxLayout();
    playRow->setSpacing(0);  // Play and the launch options form one split button
    m_playButton = new QPushButton(this);
    m_playButton->setObjectName(QStringLiteral("reprismPlayButton"));
    m_playButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_playButton->setIconSize(QSize(PlayIconSize + PlayIconGap, PlayIconSize));
    m_playButton->setCursor(Qt::PointingHandCursor);
    connect(m_playButton, &QPushButton::clicked, this, [this] {
        if (m_instance && m_instance->isRunning())
            m_actions.kill->trigger();
        else
            m_actions.launch->trigger();
    });
    m_launchMenuButton = new QToolButton(this);
    m_launchMenuButton->setObjectName(QStringLiteral("reprismPlayMenuButton"));
    m_launchMenuButton->setFixedWidth(40);
    m_launchMenuButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);  // match the Play button's height
    m_launchMenuButton->setPopupMode(QToolButton::InstantPopup);
    m_launchMenuButton->setProperty("hideMenuIndicator", true);
    m_launchMenuButton->setToolTip(tr("Launch options"));
    playRow->addWidget(m_playButton, 1);
    playRow->addWidget(m_launchMenuButton);
    layout->addLayout(playRow);
    layout->addSpacing(6);

    // Secondary actions
    auto* row = new QHBoxLayout();
    row->setSpacing(6);
    auto makeSecondary = [this, row](const QString& toolTip) {
        auto* button = new QToolButton(this);
        button->setObjectName(QStringLiteral("reprismSecondaryButton"));
        button->setMinimumHeight(38);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setIconSize(QSize(18, 18));
        button->setToolTip(toolTip);
        row->addWidget(button);
        return button;
    };
    m_editButton = makeSecondary(tr("Edit instance"));
    connect(m_editButton, &QToolButton::clicked, m_actions.edit, &QAction::trigger);
    m_folderButton = makeSecondary(tr("Open folder"));
    connect(m_folderButton, &QToolButton::clicked, m_actions.folder, &QAction::trigger);
    m_moreButton = makeSecondary(tr("More actions"));
    m_moreButton->setObjectName(QStringLiteral("reprismMoreButton"));
    m_moreButton->setMenu(m_actions.more);
    m_moreButton->setPopupMode(QToolButton::InstantPopup);
    m_moreButton->setProperty("hideMenuIndicator", true);
    layout->addLayout(row);
    layout->addStretch(1);

    m_stack->addWidget(page);

    for (auto* action : { m_actions.launch, m_actions.kill })
        connect(action, &QAction::changed, this, &InstanceDetailsPanel::updatePlayButton);

    refreshIcons();
    setInstance(nullptr);
}

void InstanceDetailsPanel::setInstance(MinecraftInstance* instance)
{
    m_instance = instance;
    m_stack->setCurrentIndex(instance ? 1 : 0);
    if (!instance)
        return;
    m_versionLabel->setText(InstanceSummary::versionLine(instance));
    m_lastPlayedLabel->setText(InstanceSummary::lastPlayedLine(instance));
    const QString playtime = InstanceSummary::playtimeLine(instance);
    m_playtimeLabel->setText(playtime);
    m_playtimeLabel->setVisible(!playtime.isEmpty());
    updatePlayButton();
}

void InstanceDetailsPanel::updatePlayButton()
{
    const bool running = m_instance && m_instance->isRunning();
    m_playButton->setText(running ? tr("Stop") : tr("Play"));
    m_playButton->setProperty("running", running);
    m_playButton->setProperty("joined", !running && m_actions.launch->menu());
    m_playButton->setEnabled(running ? m_actions.kill->isEnabled() : m_actions.launch->isEnabled());
    m_playButton->style()->unpolish(m_playButton);
    m_playButton->style()->polish(m_playButton);
    m_launchMenuButton->setMenu(m_actions.launch->menu());
    m_launchMenuButton->setVisible(!running && m_actions.launch->menu());
    m_launchMenuButton->setEnabled(m_actions.launch->isEnabled());
    refreshIcons();
}

void InstanceDetailsPanel::refreshIcons()
{
    const QPalette pal = palette();
    const bool running = m_instance && m_instance->isRunning();
    // Pad the glyph on the right: QPushButton has no icon/text spacing of its own.
    const QColor glyphColor = running ? pal.color(QPalette::ButtonText) : pal.color(QPalette::HighlightedText);
    QIcon playIcon;
    for (qreal dpr : { 1.0, 2.0 }) {
        QPixmap padded(qRound((PlayIconSize + PlayIconGap) * dpr), qRound(PlayIconSize * dpr));
        padded.setDevicePixelRatio(dpr);
        padded.fill(Qt::transparent);
        QPainter painter(&padded);
        painter.drawPixmap(0, 0, ReprismIcons::pixmap(running ? "square" : "play", glyphColor, PlayIconSize, dpr, true));
        painter.end();
        playIcon.addPixmap(padded);
    }
    m_playButton->setIcon(playIcon);
    m_launchMenuButton->setIcon(ReprismIcons::icon("chevron-down", pal.color(QPalette::HighlightedText)));
    m_editButton->setIcon(ReprismIcons::icon("pencil", pal.color(QPalette::ButtonText)));
    m_folderButton->setIcon(ReprismIcons::icon("folder-open", pal.color(QPalette::ButtonText)));
    m_moreButton->setIcon(ReprismIcons::icon("ellipsis", pal.color(QPalette::ButtonText)));
}
