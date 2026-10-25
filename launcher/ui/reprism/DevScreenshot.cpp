// SPDX-License-Identifier: GPL-3.0-only
#include "DevScreenshot.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QTimer>
#include <QWidget>
#include <QAbstractButton>
#include <QWizard>

#include <cstdlib>

namespace DevScreenshot {

static QAction* findAction(const QString& name)
{
    for (auto* window : QApplication::topLevelWidgets()) {
        if (auto* action = window->findChild<QAction*>(name))
            return action;
    }
    return nullptr;
}

void install()
{
    const QString dir = qEnvironmentVariable("REPRISM_SCREENSHOT_DIR");
    if (dir.isEmpty())
        return;
    QDir().mkpath(dir);

    bool ok = false;
    int delay = qEnvironmentVariableIntValue("REPRISM_SCREENSHOT_DELAY", &ok);
    if (!ok)
        delay = 3000;

    const QString trigger = qEnvironmentVariable("REPRISM_SCREENSHOT_TRIGGER");
    if (!trigger.isEmpty()) {
        QTimer::singleShot(delay / 2, qApp, [trigger] {
            if (trigger == "wizard:next") {
                for (auto* window : QApplication::topLevelWidgets()) {
                    if (auto* wizard = qobject_cast<QWizard*>(window); wizard && wizard->isVisible()) {
                        if (wizard->nextId() == -1)
                            wizard->button(QWizard::FinishButton)->click();
                        else
                            wizard->next();
                        return;
                    }
                }
                qWarning() << "DevScreenshot: no visible wizard";
            } else if (auto* action = findAction(trigger))
                action->trigger();
            else
                qWarning() << "DevScreenshot: no action named" << trigger;
        });
    }

    QTimer::singleShot(delay, qApp, [dir] {
        int index = 0;
        for (auto* window : QApplication::topLevelWidgets()) {
            if (!window->isVisible())
                continue;
            QString name = window->objectName().isEmpty() ? window->metaObject()->className() : window->objectName();
            window->grab().save(QDir(dir).filePath(QString("%1-%2.png").arg(index++).arg(name)));
        }
        // Exit immediately; modal dialogs may be running nested event loops.
        std::quick_exit(0);
    });
}

}  // namespace DevScreenshot
