// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QWidget>

#include <functional>

#include "ui/pages/BasePage.h"

class MinecraftInstance;
class QAction;
class QLineEdit;
class QMenu;
class QToolButton;

/// "General" page of the instance settings: icon and name, plus instance-level actions.
class InstanceGeneralPage : public QWidget, public BasePage {
    Q_OBJECT
   public:
    struct Actions {
        QAction* changeIcon;
        QAction* duplicate;
        QAction* shortcut;
        QMenu* exportMenu;
        QAction* folder;
        QAction* remove;
        std::function<void(MinecraftInstance*, const QString&)> rename;
    };
    InstanceGeneralPage(MinecraftInstance* instance, const Actions& actions, bool focusName = false, QWidget* parent = nullptr);
    void openedImpl() override;

    QString id() const override { return "reprism-general"; }
    QString displayName() const override { return tr("General"); }
    QIcon icon() const override;
    bool apply() override;

   private:
    void refreshIcon();

    MinecraftInstance* m_instance;
    Actions m_actions;
    QToolButton* m_iconButton;
    QLineEdit* m_name;
    bool m_focusName;
};

/// RePrism's instance settings: General, Installation, Game settings, Notes, Screenshots and Old logs, shown inside
/// the main window. (Content, worlds, servers and the live log are on the instance page itself.)
namespace InstanceSettingsWindow {
/// `focusName` opens the General page with the name selected, ready to rename.
void show(MinecraftInstance* instance, const InstanceGeneralPage::Actions& actions, QWidget* parent, const QString& page = {},
          bool focusName = false);
}
