// SPDX-License-Identifier: GPL-3.0-only
#include "DevScreenshot.h"

#include <QAction>
#include <QApplication>
#include <QAbstractScrollArea>
#include <QDir>
#include <QLineEdit>
#include <QMouseEvent>
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

    const QString size = qEnvironmentVariable("REPRISM_SCREENSHOT_SIZE");  // e.g. "1200x760"
    if (const auto parts = size.split('x'); parts.size() == 2) {
        QTimer::singleShot(delay / 4, qApp, [w = parts[0].toInt(), h = parts[1].toInt()] {
            for (auto* window : QApplication::topLevelWidgets())
                if (window->inherits("QMainWindow"))
                    window->resize(w, h);
        });
    }

    const QString hover = qEnvironmentVariable("REPRISM_SCREENSHOT_HOVER");  // e.g. "reprismInstanceView:200,80"
    if (const auto target = hover.split(':'); target.size() == 2) {
        QTimer::singleShot(delay * 3 / 4, qApp, [name = target[0], coords = target[1].split(',')] {
            for (auto* window : QApplication::topLevelWidgets()) {
                QWidget* widget = window->findChild<QWidget*>(name);
                if (!widget || coords.size() != 2)
                    continue;
                if (auto* area = qobject_cast<QAbstractScrollArea*>(widget))
                    widget = area->viewport();
                const QPointF pos(coords[0].toInt(), coords[1].toInt());
                QMouseEvent enter(QEvent::Enter, pos, widget->mapToGlobal(pos), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(widget, &enter);
                QMouseEvent move(QEvent::MouseMove, pos, widget->mapToGlobal(pos), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(widget, &move);
                return;
            }
        });
    }

    const QString type = qEnvironmentVariable("REPRISM_SCREENSHOT_TYPE");  // e.g. "reprismSearch:sky"
    if (const int colon = type.indexOf(':'); colon > 0) {
        QTimer::singleShot(delay / 2, qApp, [name = type.left(colon), text = type.mid(colon + 1)] {
            for (auto* window : QApplication::topLevelWidgets())
                if (auto* edit = window->findChild<QLineEdit*>(name))
                    edit->setText(text);
        });
    }

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
