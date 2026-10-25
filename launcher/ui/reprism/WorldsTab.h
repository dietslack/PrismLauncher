// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QWidget>

class MinecraftInstance;
class QFileSystemWatcher;
class QLabel;
class QListWidget;
class QListWidgetItem;

/// "Worlds" tab of the instance page: singleplayer worlds and saved servers, with their icons.
class WorldsTab : public QWidget {
    Q_OBJECT
   public:
    explicit WorldsTab(QWidget* parent = nullptr);
    void setInstance(MinecraftInstance* instance);
    void reload();

   private slots:
    void populate();

   private:
    void queuePopulate();
    void watchFiles();

   private:
    void addSection(const QString& title);
    void join(QListWidgetItem* item);
    void showContextMenu(const QPoint& pos);

    MinecraftInstance* m_instance = nullptr;
    bool m_populateQueued = false;
    QFileSystemWatcher* m_watcher;  ///< instance folder, saves and servers.dat
    QListWidget* m_list;
    QLabel* m_hint;
};
