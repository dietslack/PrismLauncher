// SPDX-License-Identifier: GPL-3.0-only
#include "FilesTab.h"

#include <QApplication>
#include <QDesktopServices>
#include <QFileIconProvider>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>

#include "DesktopServices.h"
#include "minecraft/MinecraftInstance.h"
#include "ui/reprism/ReprismIcons.h"

namespace {
/// Folder and file icons matching the rest of the RePrism UI, independent of the desktop's icon theme.
class LineIconProvider : public QFileIconProvider {
   public:
    QIcon icon(const QFileInfo& info) const override
    {
        const QColor color = QApplication::palette().color(QPalette::PlaceholderText);
        return ReprismIcons::icon(info.isDir() ? "folder" : "file", color);
    }
    QIcon icon(IconType type) const override
    {
        return ReprismIcons::icon(type == Folder ? "folder" : "file", QApplication::palette().color(QPalette::PlaceholderText));
    }
};
}  // namespace

FilesTab::FilesTab(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(6);
    m_upButton = new QToolButton(this);
    m_upButton->setObjectName(QStringLiteral("reprismSecondaryButton"));
    m_upButton->setFixedSize(34, 34);
    m_upButton->setToolTip(tr("Up one folder"));
    m_upButton->setIcon(ReprismIcons::icon("arrow-up", palette().color(QPalette::ButtonText)));
    connect(m_upButton, &QToolButton::clicked, this, [this] { navigate(QFileInfo(m_current).absolutePath()); });
    toolbar->addWidget(m_upButton);

    m_breadcrumbs = new QHBoxLayout();
    m_breadcrumbs->setSpacing(2);
    toolbar->addLayout(m_breadcrumbs);
    toolbar->addStretch(1);

    auto* openButton = new QPushButton(ReprismIcons::icon("folder-open", palette().color(QPalette::ButtonText)), tr("Open in file manager"), this);
    openButton->setObjectName(QStringLiteral("reprismToolbarButton"));
    connect(openButton, &QPushButton::clicked, this, [this] { DesktopServices::openPath(m_current, true); });
    toolbar->addWidget(openButton);
    layout->addLayout(toolbar);

    m_model = new QFileSystemModel(this);
    m_model->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    m_model->setReadOnly(true);
    m_iconProvider = std::make_unique<LineIconProvider>();
    m_model->setIconProvider(m_iconProvider.get());
    m_view = new QTreeView(this);
    m_view->setObjectName(QStringLiteral("reprismFilesList"));
    m_view->setModel(m_model);
    m_view->setRootIsDecorated(false);
    m_view->setItemsExpandable(false);
    m_view->setSortingEnabled(true);
    m_view->sortByColumn(0, Qt::AscendingOrder);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->header()->setStretchLastSection(false);
    m_view->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c < 4; c++)
        m_view->header()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    layout->addWidget(m_view, 1);

    // Folders open in place; files open with their default application.
    connect(m_view, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
        const QFileInfo info = m_model->fileInfo(index);
        if (info.isDir())
            navigate(info.absoluteFilePath());
        else
            QDesktopServices::openUrl(QUrl::fromLocalFile(info.absoluteFilePath()));
    });
    connect(m_view, &QWidget::customContextMenuRequested, this, &FilesTab::showContextMenu);
}

void FilesTab::setInstance(MinecraftInstance* instance)
{
    m_root = instance ? QDir(instance->gameRoot()).absolutePath() : QString();
    m_model->setRootPath(m_root);
    navigate(m_root);
}

void FilesTab::navigate(const QString& path)
{
    // Stay inside the instance folder.
    const QString clean = QDir::cleanPath(path);
    m_current = (clean == m_root || clean.startsWith(m_root + '/')) ? clean : m_root;
    QDir().mkpath(m_current);
    m_view->setRootIndex(m_model->index(m_current));
    m_upButton->setEnabled(m_current != m_root);
    rebuildBreadcrumbs();
}

void FilesTab::rebuildBreadcrumbs()
{
    while (auto* item = m_breadcrumbs->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    QStringList parts = QDir(m_root).relativeFilePath(m_current).split('/', Qt::SkipEmptyParts);
    parts.removeAll(".");
    QString path = m_root;
    auto addCrumb = [this](const QString& label, const QString& target, bool current) {
        auto* crumb = new QPushButton(label, this);
        crumb->setObjectName(QStringLiteral("reprismBreadcrumb"));
        crumb->setFlat(true);
        crumb->setProperty("current", current);
        crumb->setCursor(Qt::PointingHandCursor);
        connect(crumb, &QPushButton::clicked, this, [this, target] { navigate(target); });
        m_breadcrumbs->addWidget(crumb);
    };
    addCrumb(QFileInfo(m_root).fileName(), m_root, parts.isEmpty());
    for (int i = 0; i < parts.size(); i++) {
        auto* separator = new QLabel(QStringLiteral("›"), this);
        separator->setObjectName(QStringLiteral("reprismMuted"));
        m_breadcrumbs->addWidget(separator);
        path += '/' + parts[i];
        addCrumb(parts[i], path, i == parts.size() - 1);
    }
}

void FilesTab::showContextMenu(const QPoint& pos)
{
    const QModelIndex index = m_view->indexAt(pos);
    const QColor iconColor = palette().color(QPalette::WindowText);
    QMenu menu(this);
    if (index.isValid()) {
        const QFileInfo info = m_model->fileInfo(index);
        menu.addAction(ReprismIcons::icon("external-link", iconColor), info.isDir() ? tr("Open folder") : tr("Open"), this, [this, info] {
            if (info.isDir())
                navigate(info.absoluteFilePath());
            else
                QDesktopServices::openUrl(QUrl::fromLocalFile(info.absoluteFilePath()));
        });
        menu.addAction(ReprismIcons::icon("folder-open", iconColor), tr("Show in file manager"), this,
                       [info] { DesktopServices::openPath(info.absolutePath(), true); });
    } else {
        menu.addAction(ReprismIcons::icon("folder-open", iconColor), tr("Open in file manager"), this,
                       [this] { DesktopServices::openPath(m_current, true); });
    }
    menu.exec(m_view->viewport()->mapToGlobal(pos));
}
