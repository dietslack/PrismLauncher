// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QFrame>

class ContentTab;
class FilesTab;
class MinecraftInstance;
class PlayButton;
class QAction;
class QLabel;
class QStackedWidget;
class QTabBar;
class QToolButton;
class WorldsTab;

/// Full-page view of one instance: header (icon, name, facts, Play and settings) and Content / Files /
/// Worlds / Logs tabs.
class InstancePage : public QFrame {
    Q_OBJECT
   public:
    struct Actions {
        QAction* launch;
        QAction* kill;
        QAction* settings;
    };
    /// `iconButton` and `nameLabel` are the main window's existing widgets (change icon, rename); they are re-parented here.
    InstancePage(const Actions& actions, QWidget* iconButton, QWidget* nameLabel, QWidget* parent = nullptr);

    void setInstance(MinecraftInstance* instance);
    MinecraftInstance* instance() const { return m_instance; }
    /// Re-reads the header facts (name, versions, playtime, running state).
    void refreshHeader();

   signals:
    void backRequested();

   protected:
    void changeEvent(QEvent* event) override;

   private:
    void showTab(int index);
    void refreshIcons();

    Actions m_actions;
    MinecraftInstance* m_instance = nullptr;
    QToolButton* m_backButton;
    QLabel* m_subtitle;
    PlayButton* m_playButton;
    QToolButton* m_settingsButton;
    QTabBar* m_tabs;
    QStackedWidget* m_stack;
    ContentTab* m_content;
    FilesTab* m_files;
    WorldsTab* m_worlds;
    QWidget* m_logsHolder;
    QWidget* m_logPage = nullptr;
    QList<bool> m_loaded;
};
