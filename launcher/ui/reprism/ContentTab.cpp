// SPDX-License-Identifier: GPL-3.0-only
#include "ContentTab.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeView>
#include <QUrl>

#include "Application.h"
#include "DesktopServices.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/mod/DataPack.h"
#include "minecraft/mod/DataPackFolderModel.h"
#include "minecraft/mod/Mod.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ResourcePackFolderModel.h"
#include "minecraft/mod/ShaderPackFolderModel.h"
#include "ui/reprism/ReprismIcons.h"

// ---------------------------------------------------------------- ContentModel

ContentModel::ContentModel(QObject* parent) : QAbstractTableModel(parent) {}

void ContentModel::setInstance(MinecraftInstance* instance)
{
    for (const auto& [model, type] : m_folders)
        disconnect(model, nullptr, this, nullptr);
    m_folders.clear();
    m_worldNames.clear();
    m_ownedFolders.clear();
    if (instance) {
        m_folders = { { instance->loaderModList(), Type::Mod },
                      { instance->resourcePackList(), Type::ResourcePack },
                      { instance->dataPackList(), Type::DataPack },
                      { instance->shaderPackList(), Type::Shader } };
        m_folders.removeIf([](const auto& folder) { return folder.first == nullptr; });
        if (!instance->dataPackList()) {
            // Without instance-wide data packs, data packs live in each world's own datapacks folder.
            const QDir saves(instance->worldDir());
            for (const QString& world : saves.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                const QString dir = saves.filePath(world + "/datapacks");
                if (!QDir(dir).exists())
                    continue;
                auto model = std::make_unique<DataPackFolderModel>(dir, instance, false, false);
                m_worldNames.insert(model.get(), world);
                m_folders.append({ model.get(), Type::DataPack });
                m_ownedFolders.push_back(std::move(model));
            }
        }
    }
    for (const auto& [model, type] : m_folders) {
        connect(model, &QAbstractItemModel::modelReset, this, &ContentModel::rebuild);
        connect(model, &QAbstractItemModel::rowsInserted, this, &ContentModel::rebuild);
        connect(model, &QAbstractItemModel::rowsRemoved, this, &ContentModel::rebuild);
        connect(model, &QAbstractItemModel::layoutChanged, this, &ContentModel::rebuild);
        connect(model, &QAbstractItemModel::dataChanged, this, [this] {
            if (rowCount() > 0)
                emit dataChanged(index(0, 0), index(rowCount() - 1, ColumnCount - 1));
        });
    }
    rebuild();
}

void ContentModel::reload()
{
    for (const auto& [model, type] : m_folders)
        model->update();
}

void ContentModel::rebuild()
{
    beginResetModel();
    m_entries.clear();
    for (const auto& [model, type] : m_folders)
        for (int row = 0; row < model->size(); row++)
            m_entries.append({ model, row, type });
    endResetModel();
}

int ContentModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

int ContentModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

std::pair<ResourceFolderModel*, QModelIndex> ContentModel::source(int row) const
{
    const Entry& entry = m_entries.at(row);
    return { entry.model, entry.model->index(entry.row, 0) };
}

Resource* ContentModel::resource(int row) const
{
    const Entry& entry = m_entries.at(row);
    return entry.row < entry.model->size() ? &entry.model->at(entry.row) : nullptr;
}

ResourceFolderModel* ContentModel::folder(Type type) const
{
    for (const auto& [model, folderType] : m_folders)
        if (folderType == type)
            return model;
    return nullptr;
}

int ContentModel::count(Type type) const
{
    return static_cast<int>(std::count_if(m_entries.begin(), m_entries.end(), [type](const Entry& e) { return e.type == type; }));
}

QString ContentModel::typeName(Type type)
{
    switch (type) {
        case Type::Mod:
            return tr("Mod");
        case Type::ResourcePack:
            return tr("Resource pack");
        case Type::DataPack:
            return tr("Data pack");
        case Type::Shader:
            return tr("Shader");
    }
    return {};
}

QVariant ContentModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    const Entry& entry = m_entries.at(index.row());
    Resource* res = resource(index.row());
    if (!res)
        return {};

    if (role == TypeRole)
        return static_cast<int>(entry.type);
    if (role == FileNameRole)
        return res->fileinfo().fileName();
    if (role == DateRole)
        return res->dateTimeChanged();
    if (role == Qt::ToolTipRole)
        return res->fileinfo().fileName();
    if (role == Qt::ForegroundRole && !res->enabled())
        return QApplication::palette().color(QPalette::PlaceholderText);

    switch (index.column()) {
        case NameColumn:
            if (role == Qt::DisplayRole)
                return res->name();
            if (role == Qt::CheckStateRole)
                return res->enabled() ? Qt::Checked : Qt::Unchecked;
            if (role == Qt::DecorationRole) {
                QPixmap image;
                if (auto* mod = dynamic_cast<Mod*>(res))
                    image = mod->icon({ 32, 32 }, Qt::KeepAspectRatio);
                else if (auto* pack = dynamic_cast<DataPack*>(res))
                    image = pack->image({ 32, 32 }, Qt::KeepAspectRatio);
                if (!image.isNull())
                    return image;
                static const char* fallback[] = { "package", "image", "database", "sun" };
                return ReprismIcons::icon(fallback[static_cast<int>(entry.type)], QApplication::palette().color(QPalette::PlaceholderText));
            }
            break;
        case VersionColumn:
            if (role == Qt::DisplayRole)
                return res->version();
            break;
        case TypeColumn:
            if (role == Qt::DisplayRole) {
                const QString world = m_worldNames.value(entry.model);
                return world.isEmpty() ? typeName(entry.type) : tr("%1 (%2)").arg(typeName(entry.type), world);
            }
            break;
        case SourceColumn:
            if (role == Qt::DisplayRole)
                return res->metadata() ? res->provider() : tr("Local file");
            break;
        default:
            break;
    }
    return {};
}

bool ContentModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.column() != NameColumn || role != Qt::CheckStateRole)
        return false;
    auto [model, sourceIndex] = source(index.row());
    const bool enable = value.toInt() == Qt::Checked;
    return model->setResourceEnabled({ sourceIndex }, enable ? EnableAction::ENABLE : EnableAction::DISABLE);
}

QVariant ContentModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
        case NameColumn:
            return tr("Name");
        case VersionColumn:
            return tr("Version");
        case TypeColumn:
            return tr("Type");
        case SourceColumn:
            return tr("Source");
        default:
            return {};
    }
}

Qt::ItemFlags ContentModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags f = QAbstractTableModel::flags(index);
    if (index.isValid() && index.column() == NameColumn)
        f |= Qt::ItemIsUserCheckable;
    return f;
}

// ---------------------------------------------------------------- ContentFilterProxy

void ContentFilterProxy::setTypeFilter(int type)
{
    m_type = type;
    invalidateFilter();
}

void ContentFilterProxy::setSearch(const QString& text)
{
    m_search = text.trimmed();
    invalidateFilter();
}

bool ContentFilterProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    if (sortRole() == ContentModel::DateRole)
        return left.data(ContentModel::DateRole).toDateTime() < right.data(ContentModel::DateRole).toDateTime();
    // Alphabetical, case-insensitive, following the user's locale.
    return QString::localeAwareCompare(left.data(Qt::DisplayRole).toString().toLower(), right.data(Qt::DisplayRole).toString().toLower()) < 0;
}

bool ContentFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, ContentModel::NameColumn, sourceParent);
    if (m_type >= 0 && index.data(ContentModel::TypeRole).toInt() != m_type)
        return false;
    if (m_search.isEmpty())
        return true;
    return index.data(Qt::DisplayRole).toString().contains(m_search, Qt::CaseInsensitive) ||
           index.data(ContentModel::FileNameRole).toString().contains(m_search, Qt::CaseInsensitive);
}

// ---------------------------------------------------------------- ContentTab

ContentTab::ContentTab(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    // Type filters, search and actions
    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(6);
    m_filters = new QButtonGroup(this);
    m_filters->setExclusive(true);
    const std::pair<int, QString> filters[] = { { -1, tr("All") },
                                                { int(ContentModel::Type::Mod), tr("Mods") },
                                                { int(ContentModel::Type::ResourcePack), tr("Resource packs") },
                                                { int(ContentModel::Type::DataPack), tr("Data packs") },
                                                { int(ContentModel::Type::Shader), tr("Shaders") } };
    for (const auto& [type, label] : filters) {
        auto* button = new QPushButton(label, this);
        button->setObjectName(QStringLiteral("reprismFilterChip"));
        button->setCheckable(true);
        button->setProperty("label", label);
        m_filters->addButton(button, type + 1);  // QButtonGroup reserves -1 for "assign an id"
        toolbar->addWidget(button);
    }
    m_filters->button(0)->setChecked(true);
    toolbar->addStretch(1);

    m_sort = new QComboBox(this);
    m_sort->setObjectName(QStringLiteral("reprismSort"));
    m_sort->setToolTip(tr("Sort content"));
    m_sort->addItems({ tr("Name (A–Z)"), tr("Name (Z–A)"), tr("Newest first"), tr("Oldest first") });
    toolbar->addWidget(m_sort);

    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("reprismSearch"));
    m_search->setPlaceholderText(tr("Search content…"));
    m_search->setClearButtonEnabled(true);
    m_search->setFixedWidth(220);
    m_search->addAction(ReprismIcons::icon("search", palette().color(QPalette::PlaceholderText)), QLineEdit::LeadingPosition);
    toolbar->addWidget(m_search);

    auto* addButton = new QPushButton(ReprismIcons::icon("plus", palette().color(QPalette::ButtonText)), tr("Add"), this);
    addButton->setObjectName(QStringLiteral("reprismToolbarButton"));
    auto* addMenu = new QMenu(addButton);
    addMenu->addAction(ReprismIcons::icon("search", palette().color(QPalette::WindowText)), tr("Browse Modrinth / CurseForge…"), this, [this] {
        if (m_instance)
            APPLICATION->showInstanceWindow(m_instance, "mods");
    });
    addMenu->addSeparator();
    for (auto type : { ContentModel::Type::Mod, ContentModel::Type::ResourcePack, ContentModel::Type::DataPack, ContentModel::Type::Shader })
        addMenu->addAction(tr("Add %1 files…").arg(ContentModel::typeName(type).toLower()), this, [this, type] { addFiles(type); });
    addButton->setMenu(addMenu);
    toolbar->addWidget(addButton);
    layout->addLayout(toolbar);

    // List
    m_model = new ContentModel(this);
    m_proxy = new ContentFilterProxy(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_view = new QTreeView(this);
    m_view->setObjectName(QStringLiteral("reprismContentList"));
    m_view->setModel(m_proxy);
    m_view->setRootIsDecorated(false);
    m_view->setUniformRowHeights(true);
    m_view->setSortingEnabled(false);  // the Sort dropdown drives sorting
    m_view->header()->setSortIndicatorShown(false);
    m_proxy->setDynamicSortFilter(true);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setIconSize(QSize(28, 28));
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->header()->setStretchLastSection(false);
    m_view->header()->setSectionResizeMode(ContentModel::NameColumn, QHeaderView::Stretch);
    for (int c = ContentModel::VersionColumn; c < ContentModel::ColumnCount; c++)
        m_view->header()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    layout->addWidget(m_view, 1);

    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("reprismMuted"));
    layout->addWidget(m_summary);

    connect(m_filters, &QButtonGroup::idClicked, this, [this](int id) { m_proxy->setTypeFilter(id - 1); });
    connect(m_search, &QLineEdit::textChanged, m_proxy, &ContentFilterProxy::setSearch);
    connect(m_sort, &QComboBox::currentIndexChanged, this, &ContentTab::applySort);
    applySort(0);
    connect(m_model, &QAbstractItemModel::modelReset, this, &ContentTab::updateCounts);
    connect(m_model, &QAbstractItemModel::dataChanged, this, &ContentTab::updateCounts);
    connect(m_proxy, &QAbstractItemModel::rowsInserted, this, &ContentTab::updateCounts);
    connect(m_proxy, &QAbstractItemModel::rowsRemoved, this, &ContentTab::updateCounts);
    connect(m_proxy, &QAbstractItemModel::layoutChanged, this, &ContentTab::updateCounts);
    connect(m_view, &QWidget::customContextMenuRequested, this, &ContentTab::showContextMenu);
    connect(m_view, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
        const QModelIndex name = index.siblingAtColumn(ContentModel::NameColumn);
        const bool enabled = name.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        m_proxy->setData(name, enabled ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
    });
    updateCounts();
}

void ContentTab::setInstance(MinecraftInstance* instance)
{
    m_instance = instance;
    m_model->setInstance(instance);
    m_model->reload();
}

void ContentTab::applySort(int option)
{
    // 0: name A–Z, 1: name Z–A, 2: newest first, 3: oldest first
    const bool byDate = option >= 2;
    m_proxy->setSortRole(byDate ? ContentModel::DateRole : Qt::DisplayRole);
    const bool ascending = option == 0 || option == 3;
    m_proxy->sort(ContentModel::NameColumn, ascending ? Qt::AscendingOrder : Qt::DescendingOrder);
}

void ContentTab::updateCounts()
{
    for (auto* button : m_filters->buttons()) {
        const int type = m_filters->id(button) - 1;
        const int count = type < 0 ? m_model->rowCount() : m_model->count(static_cast<ContentModel::Type>(type));
        button->setText(QString("%1  %2").arg(button->property("label").toString()).arg(count));
    }
    int enabled = 0;
    for (int row = 0; row < m_proxy->rowCount(); row++)
        if (m_proxy->index(row, ContentModel::NameColumn).data(Qt::CheckStateRole).toInt() == Qt::Checked)
            enabled++;
    m_summary->setText(tr("%1 shown, %2 enabled").arg(m_proxy->rowCount()).arg(enabled));
}

QList<int> ContentTab::selectedRows() const
{
    QList<int> rows;
    for (const QModelIndex& index : m_view->selectionModel()->selectedRows())
        rows.append(m_proxy->mapToSource(index).row());
    return rows;
}

void ContentTab::showContextMenu(const QPoint& pos)
{
    const QList<int> rows = selectedRows();
    if (rows.isEmpty())
        return;
    const QColor iconColor = palette().color(QPalette::WindowText);
    QMenu menu(this);
    auto setEnabled = [this, rows](EnableAction action) {
        for (int row : rows) {
            auto [model, index] = m_model->source(row);
            model->setResourceEnabled({ index }, action);
        }
    };
    menu.addAction(tr("Enable"), this, [setEnabled] { setEnabled(EnableAction::ENABLE); });
    menu.addAction(tr("Disable"), this, [setEnabled] { setEnabled(EnableAction::DISABLE); });
    menu.addSeparator();
    Resource* first = m_model->resource(rows.first());
    if (first) {
        menu.addAction(ReprismIcons::icon("folder-open", iconColor), tr("Show in folder"), this,
                       [first] { DesktopServices::openPath(first->fileinfo().absolutePath(), true); });
        if (!first->homepage().isEmpty())
            menu.addAction(ReprismIcons::icon("external-link", iconColor), tr("Open homepage"), this,
                           [first] { QDesktopServices::openUrl(QUrl(first->homepage())); });
    }
    menu.addSeparator();
    menu.addAction(ReprismIcons::icon("trash-2", iconColor), tr("Delete"), this, [this, rows] {
        if (QMessageBox::question(this, tr("Delete content"), tr("Delete %n item(s)? This cannot be undone.", nullptr, rows.size())) !=
            QMessageBox::Yes)
            return;
        // group by folder model, since each model deletes its own files
        QHash<ResourceFolderModel*, QModelIndexList> byModel;
        for (int row : rows) {
            auto [model, index] = m_model->source(row);
            byModel[model].append(index);
        }
        for (auto it = byModel.begin(); it != byModel.end(); ++it)
            it.key()->deleteResources(it.value());
    });
    menu.exec(m_view->viewport()->mapToGlobal(pos));
}

void ContentTab::addFiles(ContentModel::Type type)
{
    ResourceFolderModel* folder = m_model->folder(type);
    if (!folder)
        return;
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Add %1 files").arg(ContentModel::typeName(type).toLower()));
    for (const QString& file : files)
        folder->installResource(file);
    folder->update();
}
