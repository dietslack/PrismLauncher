// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QFrame>
#include <QList>

class QAction;
class QMenu;
class QToolButton;
class QVBoxLayout;

/// Narrow, Modrinth-style navigation rail: app logo, page/action buttons, quick-launch tiles for recent
/// instances, and pinned buttons at the bottom.
class NavRail : public QFrame {
    Q_OBJECT
   public:
    explicit NavRail(QWidget* parent = nullptr);

    /// Adds a button with a tinted Lucide icon that triggers `action` (and shows its menu, if any).
    QToolButton* addActionButton(const QString& iconName, QAction* action, bool bottom = false);
    /// Adds a button that pops up `menu`.
    QToolButton* addMenuButton(const QString& iconName, const QString& toolTip, QMenu* menu, bool bottom = false);
    /// Adds a button that mirrors `action` entirely, including its icon (e.g. the account avatar).
    QToolButton* addMirrorButton(QAction* action, bool bottom = false);
    void addSeparator();

    struct RecentInstance {
        QString id;
        QString name;
        QString iconKey;
    };
    void setRecentInstances(const QList<RecentInstance>& instances);

    /// Re-tints icons after a theme change.
    void refreshIcons();

   signals:
    void instanceClicked(const QString& id);

   private:
    QToolButton* makeButton(bool bottom);

    QVBoxLayout* m_top;
    QVBoxLayout* m_recent;
    QVBoxLayout* m_bottom;
    QList<std::pair<QToolButton*, QString>> m_tintedButtons;
};
