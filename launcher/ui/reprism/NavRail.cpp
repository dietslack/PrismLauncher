// SPDX-License-Identifier: GPL-3.0-only
#include "NavRail.h"

#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>

#include "Application.h"
#include "BuildConfig.h"
#include "icons/IconList.h"
#include "ui/reprism/ReprismIcons.h"

namespace {
constexpr int RailWidth = 64;
constexpr int ButtonSize = 44;
constexpr int IconSize = 22;
}  // namespace

NavRail::NavRail(QWidget* parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("reprismNavRail"));
    setFixedWidth(RailWidth);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 14, 10, 14);
    layout->setSpacing(6);

    auto* logo = new QLabel(this);
    logo->setObjectName(QStringLiteral("reprismLogo"));
    logo->setPixmap(QIcon(":/" + BuildConfig.LAUNCHER_SVGFILENAME).pixmap(32, 32));
    logo->setAlignment(Qt::AlignCenter);
    logo->setToolTip(BuildConfig.LAUNCHER_DISPLAYNAME);
    layout->addWidget(logo);
    layout->addSpacing(10);

    m_top = new QVBoxLayout();
    m_top->setSpacing(6);
    layout->addLayout(m_top);

    m_recent = new QVBoxLayout();
    m_recent->setSpacing(6);
    layout->addLayout(m_recent);

    layout->addStretch(1);

    m_bottom = new QVBoxLayout();
    m_bottom->setSpacing(6);
    layout->addLayout(m_bottom);
}

QToolButton* NavRail::makeButton(bool bottom)
{
    auto* button = new QToolButton(this);
    button->setObjectName(QStringLiteral("reprismNavButton"));
    button->setFixedSize(ButtonSize, ButtonSize);
    button->setIconSize(QSize(IconSize, IconSize));
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setAutoRaise(true);
    (bottom ? m_bottom : m_top)->addWidget(button, 0, Qt::AlignHCenter);
    return button;
}

QToolButton* NavRail::addActionButton(const QString& iconName, QAction* action, bool bottom)
{
    auto* button = makeButton(bottom);
    button->setToolTip(action->text().remove('&'));
    button->setEnabled(action->isEnabled());
    connect(action, &QAction::changed, button, [button, action] {
        button->setEnabled(action->isEnabled());
        button->setVisible(action->isVisible());
        button->setToolTip(action->text().remove('&'));
    });
    connect(button, &QToolButton::clicked, action, &QAction::trigger);
    m_tintedButtons.append({ button, iconName });
    button->setIcon(ReprismIcons::icon(iconName, palette().color(QPalette::WindowText)));
    return button;
}

QToolButton* NavRail::addMenuButton(const QString& iconName, const QString& toolTip, QMenu* menu, bool bottom)
{
    auto* button = makeButton(bottom);
    button->setToolTip(toolTip);
    button->setMenu(menu);
    button->setPopupMode(QToolButton::InstantPopup);
    button->setProperty("hideMenuIndicator", true);
    m_tintedButtons.append({ button, iconName });
    button->setIcon(ReprismIcons::icon(iconName, palette().color(QPalette::WindowText)));
    return button;
}

QToolButton* NavRail::addMirrorButton(QAction* action, bool bottom)
{
    auto* button = makeButton(bottom);
    button->setDefaultAction(action);
    button->setIconSize(QSize(28, 28));
    button->setPopupMode(QToolButton::InstantPopup);
    button->setProperty("hideMenuIndicator", true);
    return button;
}

void NavRail::addSeparator()
{
    auto* line = new QFrame(this);
    line->setObjectName(QStringLiteral("reprismNavSeparator"));
    line->setFixedHeight(1);
    m_top->addSpacing(4);
    m_top->addWidget(line);
    m_top->addSpacing(4);
}

void NavRail::setRecentInstances(const QList<RecentInstance>& instances)
{
    while (auto* item = m_recent->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    for (const auto& instance : instances) {
        auto* button = new QToolButton(this);
        button->setObjectName(QStringLiteral("reprismNavInstance"));
        button->setFixedSize(ButtonSize, ButtonSize);
        button->setIconSize(QSize(32, 32));
        button->setAutoRaise(true);
        button->setIcon(ReprismIcons::crispIcon(APPLICATION->icons()->getIcon(instance.iconKey), 32));
        button->setToolTip(instance.name);
        connect(button, &QToolButton::clicked, this, [this, id = instance.id] { emit instanceClicked(id); });
        m_recent->addWidget(button, 0, Qt::AlignHCenter);
    }
}

void NavRail::refreshIcons()
{
    const QColor color = palette().color(QPalette::WindowText);
    for (const auto& [button, iconName] : m_tintedButtons)
        button->setIcon(ReprismIcons::icon(iconName, color));
}
