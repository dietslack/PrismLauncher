// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QDir>
#include <QFileIconProvider>
#include <QWidget>

#include <memory>

class MinecraftInstance;
class QFileSystemModel;
class QHBoxLayout;
class QToolButton;
class QTreeView;

/// "Files" tab of the instance page: a small file browser rooted at the instance's game folder.
class FilesTab : public QWidget {
    Q_OBJECT
   public:
    explicit FilesTab(QWidget* parent = nullptr);
    void setInstance(MinecraftInstance* instance);

   private:
    void navigate(const QString& path);
    void rebuildBreadcrumbs();
    void showContextMenu(const QPoint& pos);

    QString m_root;
    QString m_current;
    std::unique_ptr<QFileIconProvider> m_iconProvider;  // not owned by QFileSystemModel
    QFileSystemModel* m_model;
    QTreeView* m_view;
    QToolButton* m_upButton;
    QHBoxLayout* m_breadcrumbs;
};
