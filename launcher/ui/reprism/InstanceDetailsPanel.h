// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QFrame>

class MinecraftInstance;
class QAction;
class QLabel;
class QMenu;
class QPushButton;
class QStackedLayout;
class QToolButton;

/// Right-hand panel for the selected instance: icon, name, version facts, a large Play/Stop button and
/// secondary actions. All behavior goes through the main window's existing QActions.
class InstanceDetailsPanel : public QFrame {
    Q_OBJECT
   public:
    struct Actions {
        QAction* launch;
        QAction* kill;
        QAction* edit;
        QAction* folder;
        QMenu* more;
    };
    /// `iconButton` and `nameLabel` are the main window's existing widgets; they are re-parented here.
    InstanceDetailsPanel(const Actions& actions, QWidget* iconButton, QWidget* nameLabel, QWidget* parent = nullptr);

    void setInstance(MinecraftInstance* instance);
    void refreshIcons();

   private:
    void updatePlayButton();

    Actions m_actions;
    MinecraftInstance* m_instance = nullptr;
    QStackedLayout* m_stack;
    QLabel* m_versionLabel;
    QLabel* m_creatorLabel;
    QLabel* m_lastPlayedLabel;
    QLabel* m_playtimeLabel;
    QPushButton* m_playButton;
    QToolButton* m_launchMenuButton;
    QToolButton* m_editButton;
    QToolButton* m_folderButton;
    QToolButton* m_moreButton;
};
