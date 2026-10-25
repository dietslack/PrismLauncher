// SPDX-License-Identifier: GPL-3.0-only
#include "InstancePage.h"

#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

#include "Application.h"
#include "MMCTime.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "net/Mode.h"
#include "settings/SettingsObject.h"
#include "ui/pages/instance/LogPage.h"
#include "ui/reprism/ContentTab.h"
#include "ui/reprism/FilesTab.h"
#include "ui/reprism/InstanceSummary.h"
#include "ui/reprism/PlayButton.h"
#include "ui/reprism/ReprismIcons.h"
#include "ui/reprism/WorldsTab.h"
#include "ui/widgets/EditableLabel.h"

namespace {
enum Tab { ContentIndex, FilesIndex, WorldsIndex, LogsIndex, TabCount };
}

InstancePage::InstancePage(const Actions& actions, QWidget* iconButton, QWidget* nameLabel, QWidget* parent)
    : QFrame(parent), m_actions(actions), m_loaded(TabCount, false)
{
    setObjectName(QStringLiteral("reprismInstancePage"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    m_backButton = new QToolButton(this);
    m_backButton->setObjectName(QStringLiteral("reprismBackButton"));
    m_backButton->setText(tr("Library"));
    m_backButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_backButton->setCursor(Qt::PointingHandCursor);
    connect(m_backButton, &QToolButton::clicked, this, &InstancePage::backRequested);
    layout->addWidget(m_backButton, 0, Qt::AlignLeft);

    // Header: icon, title + facts, Play and settings
    auto* header = new QHBoxLayout();
    header->setSpacing(18);
    iconButton->setParent(this);
    iconButton->setObjectName(QStringLiteral("reprismHeaderIcon"));
    iconButton->setFixedSize(96, 96);
    static_cast<QToolButton*>(iconButton)->setIconSize(QSize(80, 80));
    header->addWidget(iconButton);

    auto* titleBlock = new QVBoxLayout();
    titleBlock->setSpacing(4);
    titleBlock->addStretch(1);
    nameLabel->setParent(this);
    nameLabel->setObjectName(QStringLiteral("reprismHeaderName"));
    titleBlock->addWidget(nameLabel);
    m_subtitle = new QLabel(this);
    m_subtitle->setObjectName(QStringLiteral("reprismMuted"));
    m_subtitle->setIndent(5);  // line up with the title's text, which has a small document margin
    titleBlock->addWidget(m_subtitle);
    titleBlock->addStretch(1);
    header->addLayout(titleBlock, 1);

    m_playButton = new PlayButton(m_actions.launch, m_actions.kill, this);
    m_playButton->setFixedWidth(170);
    header->addWidget(m_playButton, 0, Qt::AlignVCenter);
    m_settingsButton = new QToolButton(this);
    m_settingsButton->setObjectName(QStringLiteral("reprismSecondaryButton"));
    m_settingsButton->setFixedSize(46, 46);
    m_settingsButton->setIconSize(QSize(20, 20));
    m_settingsButton->setToolTip(tr("Instance settings"));
    m_settingsButton->setCursor(Qt::PointingHandCursor);
    connect(m_settingsButton, &QToolButton::clicked, m_actions.settings, &QAction::trigger);
    header->addWidget(m_settingsButton, 0, Qt::AlignVCenter);
    layout->addLayout(header);

    // Tabs
    m_tabs = new QTabBar(this);
    m_tabs->setObjectName(QStringLiteral("reprismTabs"));
    m_tabs->setDrawBase(false);
    m_tabs->setExpanding(false);
    for (const QString& name : { tr("Content"), tr("Files"), tr("Worlds"), tr("Logs") })
        m_tabs->addTab(name);
    layout->addWidget(m_tabs);

    m_stack = new QStackedWidget(this);
    m_content = new ContentTab(m_stack);
    m_files = new FilesTab(m_stack);
    m_worlds = new WorldsTab(m_stack);
    m_logsHolder = new QWidget(m_stack);
    auto* logsLayout = new QVBoxLayout(m_logsHolder);
    logsLayout->setContentsMargins(0, 0, 0, 0);
    for (QWidget* page : { static_cast<QWidget*>(m_content), static_cast<QWidget*>(m_files), static_cast<QWidget*>(m_worlds), m_logsHolder })
        m_stack->addWidget(page);
    layout->addWidget(m_stack, 1);
    connect(m_tabs, &QTabBar::currentChanged, this, &InstancePage::showTab);

    refreshIcons();
}

void InstancePage::setInstance(MinecraftInstance* instance)
{
    if (m_instance == instance) {
        refreshHeader();
        return;
    }
    m_instance = instance;
    m_loaded.fill(false);
    // Make sure the component list (Minecraft version, loaders) is loaded, as Prism's own pages do;
    // the content tab's version picker filters by it.
    if (instance && instance->getPackProfile()->getComponentVersion("net.minecraft").isEmpty()) {
        if (auto res = instance->getPackProfile()->reload(Net::Mode::Offline); !res)
            qWarning() << "Failed to load components of" << instance->name() << res.error();
    }
    delete m_logPage;
    m_logPage = nullptr;
    m_playButton->setInstance(instance);
    refreshHeader();
    m_tabs->setCurrentIndex(ContentIndex);
    showTab(ContentIndex);
}

void InstancePage::refreshHeader()
{
    if (!m_instance)
        return;
    const auto components = InstanceSummary::components(m_instance);
    QStringList facts;
    facts << QString("%1 %2").arg(components.loader.isEmpty() ? tr("Minecraft") : components.loader, components.minecraft).trimmed();
    if (m_instance->totalTimePlayed() > 0) {
        const bool noDays = APPLICATION->settings()->get("ShowGameTimeWithoutDays").toBool();
        facts << tr("%1 played").arg(Time::prettifyDuration(m_instance->totalTimePlayed(), noDays));
    } else {
        facts << tr("Not played yet");
    }
    m_subtitle->setText(facts.join(QStringLiteral("  •  ")));
    m_playButton->refresh();
}

void InstancePage::showTab(int index)
{
    m_stack->setCurrentIndex(index);
    if (!m_instance)
        return;
    if (m_loaded.value(index)) {
        if (index == WorldsIndex)
            m_worlds->reload();  // cheap, and catches anything the file watchers missed
        return;
    }
    m_loaded[index] = true;
    switch (index) {
        case ContentIndex:
            m_content->setInstance(m_instance);
            break;
        case FilesIndex:
            m_files->setInstance(m_instance);
            break;
        case WorldsIndex:
            m_worlds->setInstance(m_instance);
            break;
        case LogsIndex: {
            auto* page = new LogPage(m_instance, m_logsHolder);
            m_logsHolder->layout()->addWidget(page);
            page->opened();
            m_logPage = page;
            break;
        }
        default:
            break;
    }
}

void InstancePage::refreshIcons()
{
    m_backButton->setIcon(ReprismIcons::icon("chevron-left", palette().color(QPalette::PlaceholderText)));
    m_settingsButton->setIcon(ReprismIcons::icon("settings", palette().color(QPalette::ButtonText)));
}

void InstancePage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange)
        refreshIcons();
    QFrame::changeEvent(event);
}
