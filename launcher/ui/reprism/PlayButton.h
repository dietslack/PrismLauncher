// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QWidget>

class MinecraftInstance;
class QAction;
class QPushButton;
class QToolButton;

/// Large Play/Stop split button: the main part launches (or stops) the instance, the attached dropdown shows
/// the launch options menu (offline, profilers, ...). Driven by the main window's launch/kill actions.
class PlayButton : public QWidget {
    Q_OBJECT
   public:
    PlayButton(QAction* launch, QAction* kill, QWidget* parent = nullptr);

    void setInstance(MinecraftInstance* instance);
    void refresh();

   protected:
    void changeEvent(QEvent* event) override;

   private:
    QAction* m_launch;
    QAction* m_kill;
    MinecraftInstance* m_instance = nullptr;
    QPushButton* m_button;
    QToolButton* m_menuButton;
};
