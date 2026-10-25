// SPDX-License-Identifier: GPL-3.0-only
#include "InAppModal.h"

#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QEventLoop>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPainter>
#include <QPointer>
#include <QToolButton>
#include <QVBoxLayout>

#include "BuildConfig.h"
#include "ui/reprism/ReprismIcons.h"

namespace {
constexpr int HostMargin = 32;

/// Dimmed backdrop covering the host, holding the dialog's card in the middle.
class Overlay : public QWidget {
   public:
    Overlay(QWidget* host, QWidget* card) : QWidget(host), m_host(host), m_card(card)
    {
        setObjectName(QStringLiteral("reprismModalOverlay"));
        card->setParent(this);
        host->installEventFilter(this);
        relayout();
    }

    void relayout()
    {
        setGeometry(m_host->rect());
        // As large as the dialog wants, but never beyond the host.
        const QSize wanted = m_card->sizeHint().expandedTo(m_preferred);
        const QSize available = size() - QSize(2 * HostMargin, 2 * HostMargin);
        const QSize cardSize = wanted.boundedTo(available).expandedTo(m_card->minimumSizeHint().boundedTo(available));
        m_card->setGeometry(QRect(QPoint((width() - cardSize.width()) / 2, (height() - cardSize.height()) / 2), cardSize));
    }

    void setPreferredSize(const QSize& size)
    {
        m_preferred = size;
        relayout();
    }

   protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_host && event->type() == QEvent::Resize)
            relayout();
        return false;
    }
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(0, 0, 0, 140));
    }
    // Swallow clicks on the backdrop so nothing behind the dialog reacts.
    void mousePressEvent(QMouseEvent* event) override { event->accept(); }
    void mouseReleaseEvent(QMouseEvent* event) override { event->accept(); }
    void wheelEvent(QWheelEvent* event) override { event->accept(); }

   private:
    QWidget* m_host;
    QWidget* m_card;
    QSize m_preferred;
};

/// The main window, if `dialog` belongs to it (directly, or via a dialog already embedded in it).
QWidget* defaultHost(QDialog* dialog)
{
    QWidget* parent = dialog->parentWidget();
    QWidget* window = parent ? parent->window() : nullptr;
    if (window && window->objectName() == "MainWindow" && qobject_cast<QMainWindow*>(window) && window->isVisible())
        return window;
    return nullptr;
}

QString cleanTitle(QString title)
{
    // Window titles often end in the app name ("New Instance - RePrism Launcher"); the card doesn't need it.
    for (const QString& suffix : { QString(" - %1").arg(BuildConfig.LAUNCHER_DISPLAYNAME), QString(" — %1").arg(BuildConfig.LAUNCHER_DISPLAYNAME) })
        if (title.endsWith(suffix))
            title.chop(suffix.size());
    return title;
}
}  // namespace

namespace InAppModal {

int exec(QDialog* dialog, QWidget* host)
{
    if (!host)
        host = defaultHost(dialog);
    if (!host)
        return dialog->exec();  // not a main-window dialog (e.g. from the instance window or the setup wizard)

    // Card: title row with a close button, then the dialog itself.
    auto* card = new QFrame();
    card->setObjectName(QStringLiteral("reprismModalCard"));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 14, 14, 16);
    layout->setSpacing(10);
    auto* header = new QHBoxLayout();
    auto* title = new QLabel(cleanTitle(dialog->windowTitle()), card);
    title->setObjectName(QStringLiteral("reprismModalTitle"));
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.25);
    title->setFont(titleFont);
    header->addWidget(title, 1);
    auto* close = new QToolButton(card);
    close->setObjectName(QStringLiteral("reprismModalClose"));
    close->setFixedSize(32, 32);
    close->setIconSize(QSize(18, 18));
    close->setIcon(ReprismIcons::icon("x", card->palette().color(QPalette::WindowText)));
    close->setToolTip(QObject::tr("Close"));
    close->setCursor(Qt::PointingHandCursor);
    QObject::connect(close, &QToolButton::clicked, dialog, &QDialog::reject);
    QObject::connect(dialog, &QWidget::windowTitleChanged, title, [title](const QString& text) { title->setText(cleanTitle(text)); });
    header->addWidget(close, 0, Qt::AlignTop);
    layout->addLayout(header);

    // Embed the dialog as a plain child widget.
    QPointer<QWidget> oldParent = dialog->parentWidget();
    const Qt::WindowFlags oldFlags = dialog->windowFlags();
    const QSize dialogSize = dialog->size();
    dialog->setParent(card, Qt::Widget);
    dialog->setObjectName(dialog->objectName().isEmpty() ? QStringLiteral("reprismEmbeddedDialog") : dialog->objectName());
    dialog->setProperty("reprismEmbedded", true);
    layout->addWidget(dialog, 1);

    auto* overlay = new Overlay(host, card);
    overlay->setPreferredSize(dialogSize + QSize(34, 70));  // keep the size the dialog asked for, plus the card chrome
    overlay->show();
    overlay->raise();
    dialog->show();
    dialog->setFocus();

    QEventLoop loop;
    QObject::connect(dialog, &QDialog::finished, &loop, [&loop](int result) { loop.exit(result); });
    const int result = loop.exec(QEventLoop::DialogExec);

    // Hand the dialog back before the card is destroyed: callers usually own it (often on the stack).
    dialog->hide();
    layout->removeWidget(dialog);
    dialog->setParent(oldParent, oldFlags);
    dialog->setProperty("reprismEmbedded", QVariant());
    overlay->hide();
    overlay->deleteLater();
    return result;
}

}  // namespace InAppModal
