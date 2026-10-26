// SPDX-License-Identifier: GPL-3.0-only
#include "InstanceSettingsWindow.h"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include "Application.h"
#include "FileSystem.h"
#include "InstanceList.h"
#include "icons/IconList.h"
#include "minecraft/MinecraftInstance.h"
#include "ui/pagedialog/PageDialog.h"
#include "ui/pages/BasePageProvider.h"
#include "ui/pages/instance/InstanceSettingsPage.h"
#include "ui/pages/instance/ManagedPackPage.h"
#include "ui/pages/instance/NotesPage.h"
#include "ui/pages/instance/OtherLogsPage.h"
#include "ui/pages/instance/ScreenshotsPage.h"
#include "ui/pages/instance/VersionPage.h"
#include "ui/reprism/InAppModal.h"
#include "ui/reprism/ReprismIcons.h"

namespace {
QIcon lineIcon(const char* name)
{
    return ReprismIcons::icon(name, QApplication::palette().color(QPalette::WindowText));
}

// Prism's pages under RePrism names and line icons.
class InstallationPage : public VersionPage {
   public:
    using VersionPage::VersionPage;
    QString displayName() const override { return QObject::tr("Installation"); }
    QIcon icon() const override { return lineIcon("wrench"); }
};
class GameSettingsPage : public InstanceSettingsPage {
   public:
    using InstanceSettingsPage::InstanceSettingsPage;
    QString displayName() const override { return QObject::tr("Game settings"); }
    QIcon icon() const override { return lineIcon("cpu"); }
};
class ReprismNotesPage : public NotesPage {
   public:
    using NotesPage::NotesPage;
    QIcon icon() const override { return lineIcon("notebook-pen"); }
};
class ReprismScreenshotsPage : public ScreenshotsPage {
   public:
    using ScreenshotsPage::ScreenshotsPage;
    QIcon icon() const override { return lineIcon("image"); }
};
class ReprismOtherLogsPage : public OtherLogsPage {
   public:
    using OtherLogsPage::OtherLogsPage;
    QString displayName() const override { return QObject::tr("Old logs"); }
    QIcon icon() const override { return lineIcon("scroll-text"); }
};

class Provider : public BasePageProvider {
   public:
    Provider(MinecraftInstance* instance, InstanceGeneralPage::Actions actions, bool focusName)
        : m_instance(instance), m_actions(std::move(actions)), m_focusName(focusName)
    {}
    QList<BasePage*> getPages() override
    {
        QList<BasePage*> pages;
        pages.append(new InstanceGeneralPage(m_instance, m_actions, m_focusName));
        pages.append(new InstallationPage(m_instance));
        if (m_instance->isManagedPack())
            pages.append(ManagedPackPage::createPage(m_instance));
        pages.append(new GameSettingsPage(m_instance));
        pages.append(new ReprismNotesPage(m_instance));
        pages.append(new ReprismScreenshotsPage(FS::PathCombine(m_instance->gameRoot(), "screenshots")));
        pages.append(new ReprismOtherLogsPage("logs", QObject::tr("Old logs"), "Other-Logs", m_instance));
        return pages;
    }
    QString dialogTitle() override { return QObject::tr("%1 settings").arg(m_instance->name()); }

   private:
    MinecraftInstance* m_instance;
    InstanceGeneralPage::Actions m_actions;
    bool m_focusName;
};

/// A row of the General page: title and description on the left, a button on the right.
QWidget* actionRow(QWidget* parent, const QString& title, const QString& description, QPushButton* button, bool danger = false)
{
    auto* row = new QFrame(parent);
    row->setObjectName(QStringLiteral("reprismSettingsRow"));
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(16, 12, 12, 12);
    auto* text = new QVBoxLayout();
    text->setSpacing(2);
    auto* titleLabel = new QLabel(title, row);
    QFont bold = titleLabel->font();
    bold.setBold(true);
    titleLabel->setFont(bold);
    if (danger)
        titleLabel->setObjectName(QStringLiteral("reprismDangerText"));
    auto* descriptionLabel = new QLabel(description, row);
    descriptionLabel->setObjectName(QStringLiteral("reprismMuted"));
    descriptionLabel->setWordWrap(true);
    text->addWidget(titleLabel);
    text->addWidget(descriptionLabel);
    layout->addLayout(text, 1);
    button->setObjectName(danger ? QStringLiteral("reprismDangerButton") : QStringLiteral("reprismToolbarButton"));
    button->setMinimumWidth(150);
    layout->addWidget(button, 0, Qt::AlignVCenter);
    return row;
}
}  // namespace

InstanceGeneralPage::InstanceGeneralPage(MinecraftInstance* instance, const Actions& actions, bool focusName, QWidget* parent)
    : QWidget(parent), m_instance(instance), m_actions(actions), m_focusName(focusName)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    // Icon and name
    auto* identity = new QHBoxLayout();
    identity->setSpacing(18);
    m_iconButton = new QToolButton(this);
    m_iconButton->setObjectName(QStringLiteral("reprismHeaderIcon"));
    m_iconButton->setFixedSize(96, 96);
    m_iconButton->setIconSize(QSize(80, 80));
    m_iconButton->setToolTip(tr("Change icon"));
    m_iconButton->setCursor(Qt::PointingHandCursor);
    connect(m_iconButton, &QToolButton::clicked, this, [this] {
        m_actions.changeIcon->trigger();
        refreshIcon();
    });
    identity->addWidget(m_iconButton);
    auto* nameBlock = new QVBoxLayout();
    nameBlock->addStretch(1);
    auto* nameLabel = new QLabel(tr("Name"), this);
    nameLabel->setObjectName(QStringLiteral("reprismMuted"));
    nameBlock->addWidget(nameLabel);
    m_name = new QLineEdit(instance->name(), this);
    m_name->setMaxLength(128);
    nameBlock->addWidget(m_name);
    nameBlock->addStretch(1);
    identity->addLayout(nameBlock, 1);
    layout->addLayout(identity);
    layout->addSpacing(10);

    // Actions
    auto* duplicate = new QPushButton(lineIcon("copy"), tr("Duplicate"), this);
    connect(duplicate, &QPushButton::clicked, m_actions.duplicate, &QAction::trigger);
    layout->addWidget(actionRow(this, tr("Duplicate instance"), tr("Make a copy of this instance, including its worlds and content."), duplicate));

    auto* shortcut = new QPushButton(lineIcon("external-link"), tr("Create shortcut"), this);
    connect(shortcut, &QPushButton::clicked, m_actions.shortcut, &QAction::trigger);
    layout->addWidget(actionRow(this, tr("Shortcut"), tr("Launch this instance straight from your desktop or applications menu."), shortcut));

    auto* exportButton = new QPushButton(lineIcon("share"), tr("Export"), this);
    exportButton->setMenu(m_actions.exportMenu);
    layout->addWidget(actionRow(this, tr("Export"), tr("Save this instance as a zip, Modrinth pack or CurseForge pack to share it."), exportButton));

    auto* folder = new QPushButton(lineIcon("folder-open"), tr("Open folder"), this);
    connect(folder, &QPushButton::clicked, m_actions.folder, &QAction::trigger);
    layout->addWidget(actionRow(this, tr("Instance folder"), tr("Open this instance's files in your file manager."), folder));

    auto* remove = new QPushButton(ReprismIcons::icon("trash-2", QColor(0xff, 0x5c, 0x5c)), tr("Delete"), this);
    connect(remove, &QPushButton::clicked, this, [this] {
        const QString id = m_instance->id();
        m_actions.remove->trigger();
        // Close the settings once the instance is gone.
        if (!APPLICATION->instances()->getInstanceById(id)) {
            for (QWidget* w = parentWidget(); w; w = w->parentWidget())
                if (auto* dialog = qobject_cast<QDialog*>(w)) {
                    dialog->reject();
                    break;
                }
        }
    });
    layout->addWidget(actionRow(this, tr("Delete instance"), tr("Permanently delete this instance and all of its worlds and content."), remove, true));
    layout->addStretch(1);
    refreshIcon();
}

void InstanceGeneralPage::openedImpl()
{
    if (!m_focusName)
        return;
    m_focusName = false;
    QMetaObject::invokeMethod(m_name, [this] {
        m_name->setFocus();
        m_name->selectAll();
    }, Qt::QueuedConnection);
}

QIcon InstanceGeneralPage::icon() const
{
    return lineIcon("info");
}

void InstanceGeneralPage::refreshIcon()
{
    m_iconButton->setIcon(ReprismIcons::crispIcon(APPLICATION->icons()->getIcon(m_instance->iconKey()), 80));
}

bool InstanceGeneralPage::apply()
{
    const QString name = m_name->text().trimmed();
    if (!name.isEmpty() && name != m_instance->name() && m_actions.rename)
        m_actions.rename(m_instance, name);
    return true;
}

namespace InstanceSettingsWindow {

void show(MinecraftInstance* instance, const InstanceGeneralPage::Actions& actions, QWidget* parent, const QString& page, bool focusName)
{
    Provider provider(instance, actions, focusName);
    PageDialog dialog(&provider, page, parent);
    InAppModal::exec(&dialog, InAppModal::Size::Large);
}

}  // namespace InstanceSettingsWindow
