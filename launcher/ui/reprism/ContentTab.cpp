// SPDX-License-Identifier: GPL-3.0-only
#include "ContentTab.h"

#include <QApplication>
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
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolTip>
#include <QPushButton>
#include <QTreeView>
#include <QUrl>

#include "Application.h"
#include "DesktopServices.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "modplatform/flame/FlameCheckUpdate.h"
#include "modplatform/modrinth/ModrinthCheckUpdate.h"
#include "settings/SettingsObject.h"
#include "tasks/ConcurrentTask.h"
#include "tasks/SequentialTask.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/ResourceDownloadDialog.h"
#include "minecraft/mod/DataPack.h"
#include "minecraft/mod/DataPackFolderModel.h"
#include "minecraft/mod/Mod.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ResourcePackFolderModel.h"
#include "minecraft/mod/ShaderPackFolderModel.h"
#include "ui/reprism/InstanceSummary.h"
#include "ui/reprism/ReprismIcons.h"

namespace {
QColor withAlpha(QColor color, qreal alpha)
{
    color.setAlphaF(alpha);
    return color;
}

/// Rounded checkbox matching the RePrism style, drawn directly so it looks the same in every theme.
void drawCheckBox(QPainter* painter, const QRect& rect, Qt::CheckState state, bool hovered)
{
    const QPalette pal = QApplication::palette();
    const QRectF box(rect.center().x() - 8 + 0.5, rect.center().y() - 8 + 0.5, 16, 16);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (state == Qt::Unchecked) {
        painter->setPen(QPen(withAlpha(pal.color(QPalette::PlaceholderText), hovered ? 1.0 : 0.6), 1));
        painter->setBrush(pal.color(QPalette::Dark));
        painter->drawRoundedRect(box, 5, 5);
    } else {
        painter->setPen(Qt::NoPen);
        painter->setBrush(pal.color(QPalette::Highlight));
        painter->drawRoundedRect(box, 5, 5);
        painter->setPen(QPen(pal.color(QPalette::HighlightedText), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (state == Qt::Checked) {
            QPainterPath check;
            check.moveTo(box.left() + 4, box.top() + 8.4);
            check.lineTo(box.left() + 6.9, box.top() + 11.2);
            check.lineTo(box.left() + 12.2, box.top() + 5.2);
            painter->drawPath(check);
        } else {
            painter->drawLine(QPointF(box.left() + 4.5, box.center().y()), QPointF(box.right() - 4.5, box.center().y()));
        }
    }
    painter->restore();
}

/// Checkbox column: checks a row for bulk actions.
class SelectDelegate : public QStyledItemDelegate {
   public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        drawCheckBox(painter, option.rect, static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt()),
                     option.state & QStyle::State_MouseOver);
    }
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem&, const QModelIndex& index) override
    {
        if (event->type() == QEvent::MouseButtonRelease) {
            const bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
            model->setData(index, checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
            return true;
        }
        return event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick;
    }
};

/// "Actions" column: version button (update / change version), enable switch and delete button.
class ActionsDelegate : public QStyledItemDelegate {
   public:
    enum Action { VersionAction, ToggleAction, DeleteAction };
    using Handler = std::function<void(const QModelIndex&, Action)>;

    ActionsDelegate(QAbstractItemView* view, Handler handler) : QStyledItemDelegate(view), m_view(view), m_handler(std::move(handler)) {}

    static constexpr int Width = 132;
    static QRect versionRect(const QRect& cell) { return QRect(cell.right() - 120, cell.center().y() - 15, 30, 30); }
    static QRectF switchRect(const QRect& cell) { return QRectF(cell.right() - 82 + 0.5, cell.center().y() - 9 + 0.5, 32, 18); }
    static QRect deleteRect(const QRect& cell) { return QRect(cell.right() - 38, cell.center().y() - 15, 30, 30); }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const QPalette pal = QApplication::palette();
        const qreal dpr = painter->device()->devicePixelRatioF();
        const QPoint cursor = m_view->viewport()->mapFromGlobal(QCursor::pos());
        const bool rowHovered = option.state & QStyle::State_MouseOver;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        auto iconButton = [&](const QRect& rect, const QString& icon, bool filled) {
            const bool hovered = rowHovered && rect.contains(cursor);
            painter->setPen(Qt::NoPen);
            if (filled)
                painter->setBrush(hovered ? pal.color(QPalette::Highlight).lighter(110) : pal.color(QPalette::Highlight));
            else
                painter->setBrush(hovered ? pal.color(QPalette::Light) : Qt::transparent);
            painter->drawRoundedRect(rect, 8, 8);
            const QColor fg = filled ? pal.color(QPalette::HighlightedText) : withAlpha(pal.color(QPalette::Text), hovered ? 1.0 : 0.75);
            painter->drawPixmap(rect.center().x() - 8 + 1, rect.center().y() - 8 + 1, ReprismIcons::pixmap(icon, fg, 16, dpr));
        };

        // Version: update when one is available, otherwise change version for content from Modrinth/CurseForge.
        if (!index.data(ContentModel::UpdateRole).toString().isEmpty())
            iconButton(versionRect(option.rect), "download", true);
        else if (index.data(ContentModel::HasSourceRole).toBool())
            iconButton(versionRect(option.rect), "history", false);

        // Enable switch
        const bool on = index.data(ContentModel::EnabledRole).toBool();
        const QRectF t = switchRect(option.rect);
        painter->setPen(Qt::NoPen);
        painter->setBrush(on ? pal.color(QPalette::Highlight) : pal.color(QPalette::Light));
        painter->drawRoundedRect(t, 9, 9);
        const qreal knob = 12;
        painter->setBrush(on ? pal.color(QPalette::HighlightedText) : withAlpha(pal.color(QPalette::Text), 0.7));
        painter->drawEllipse(QRectF(on ? t.right() - 3 - knob : t.left() + 3, t.center().y() - knob / 2, knob, knob));

        iconButton(deleteRect(option.rect), "trash-2", false);
        painter->restore();
    }

    bool editorEvent(QEvent* event, QAbstractItemModel*, const QStyleOptionViewItem& option, const QModelIndex& index) override
    {
        if (event->type() != QEvent::MouseButtonRelease)
            return event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick;
        const QPointF pos = static_cast<QMouseEvent*>(event)->position();
        const bool hasVersionButton =
            !index.data(ContentModel::UpdateRole).toString().isEmpty() || index.data(ContentModel::HasSourceRole).toBool();
        if (hasVersionButton && versionRect(option.rect).contains(pos.toPoint()))
            m_handler(index, VersionAction);
        else if (switchRect(option.rect).adjusted(-6, -6, 6, 6).contains(pos))
            m_handler(index, ToggleAction);
        else if (deleteRect(option.rect).contains(pos.toPoint()))
            m_handler(index, DeleteAction);
        return true;
    }

    bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option, const QModelIndex& index) override
    {
        const QString update = index.data(ContentModel::UpdateRole).toString();
        QString tip;
        if (versionRect(option.rect).contains(event->pos())) {
            if (!update.isEmpty())
                tip = QObject::tr("Update to %1").arg(update);
            else if (index.data(ContentModel::HasSourceRole).toBool())
                tip = QObject::tr("Change version");
        } else if (switchRect(option.rect).adjusted(-6, -6, 6, 6).contains(event->pos())) {
            tip = index.data(ContentModel::EnabledRole).toBool() ? QObject::tr("Disable") : QObject::tr("Enable");
        } else if (deleteRect(option.rect).contains(event->pos())) {
            tip = QObject::tr("Delete");
        }
        if (tip.isEmpty())
            QToolTip::hideText();
        else
            QToolTip::showText(event->globalPos(), tip, view);
        return true;
    }

   private:
    QAbstractItemView* m_view;
    Handler m_handler;
};

/// Repaints a view's viewport as the mouse moves, so cell buttons can highlight under the cursor.
class HoverRepaint : public QObject {
   public:
    using QObject::QObject;
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::MouseMove || event->type() == QEvent::Leave)
            static_cast<QWidget*>(watched)->update();
        return false;
    }
};

/// Header with a select-all checkbox in the first section.
class SelectAllHeader : public QHeaderView {
   public:
    SelectAllHeader(std::function<Qt::CheckState()> state, QWidget* parent) : QHeaderView(Qt::Horizontal, parent), m_state(std::move(state))
    {
        setSectionsClickable(true);
    }

   protected:
    void paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const override
    {
        painter->save();
        QHeaderView::paintSection(painter, rect, logicalIndex);
        painter->restore();
        if (logicalIndex == ContentModel::SelectColumn)
            drawCheckBox(painter, rect, m_state(), false);
    }

   private:
    std::function<Qt::CheckState()> m_state;
};
}  // namespace

// ---------------------------------------------------------------- ContentModel

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
    QSet<QString> present;
    for (const auto& [model, type] : m_folders) {
        for (int row = 0; row < model->size(); row++) {
            m_entries.append({ model, row, type });
            present.insert(model->at(row).fileinfo().absoluteFilePath());
        }
    }
    m_checked.intersect(present);  // forget checks of files that are gone
    endResetModel();
    emit checksChanged();
}

QString ContentModel::pathOf(int row) const
{
    Resource* res = resource(row);
    return res ? res->fileinfo().absoluteFilePath() : QString();
}

void ContentModel::setUpdates(const QHash<QString, QString>& updates)
{
    m_updates = updates;
    if (rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, ColumnCount - 1));
}

int ContentModel::rowOfPath(const QString& path) const
{
    for (int row = 0; row < m_entries.size(); row++)
        if (pathOf(row) == path)
            return row;
    return -1;
}

bool ContentModel::isChecked(int row) const
{
    return m_checked.contains(pathOf(row));
}

void ContentModel::setChecked(const QList<int>& rows, bool checked)
{
    for (int row : rows) {
        if (checked)
            m_checked.insert(pathOf(row));
        else
            m_checked.remove(pathOf(row));
    }
    if (rowCount() > 0)
        emit dataChanged(index(0, 0), index(rowCount() - 1, ColumnCount - 1));
    emit checksChanged();
}

QList<int> ContentModel::checkedRows() const
{
    QList<int> rows;
    for (int row = 0; row < m_entries.size(); row++)
        if (isChecked(row))
            rows.append(row);
    return rows;
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
    if (role == EnabledRole)
        return res->enabled();
    if (role == UpdateRole)
        return m_updates.value(res->fileinfo().absoluteFilePath());
    if (role == HasSourceRole)
        return res->metadata() != nullptr;

    switch (index.column()) {
        case SelectColumn:
            if (role == Qt::CheckStateRole)
                return isChecked(index.row()) ? Qt::Checked : Qt::Unchecked;
            break;
        case NameColumn:
            if (role == Qt::DisplayRole)
                return res->name();
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
            if (role == Qt::DisplayRole) {
                const QString update = m_updates.value(res->fileinfo().absoluteFilePath());
                return update.isEmpty() ? res->version() : QString("%1  →  %2").arg(res->version(), update);
            }
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
    if (!index.isValid())
        return false;
    if (index.column() == SelectColumn && role == Qt::CheckStateRole) {
        setChecked({ index.row() }, value.toInt() == Qt::Checked);
        return true;
    }
    if (index.column() == ActionsColumn && role == EnabledRole) {
        auto [model, sourceIndex] = source(index.row());
        return model->setResourceEnabled({ sourceIndex }, value.toBool() ? EnableAction::ENABLE : EnableAction::DISABLE);
    }
    return false;
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
        case ActionsColumn:
            return tr("Actions");
        default:
            return {};
    }
}

Qt::ItemFlags ContentModel::flags(const QModelIndex& index) const
{
    return QAbstractTableModel::flags(index);
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

    // Type filters, sort, search and actions
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

    m_updateButton = new QPushButton(ReprismIcons::icon("refresh-cw", palette().color(QPalette::ButtonText)), tr("Check for updates"), this);
    m_updateButton->setObjectName(QStringLiteral("reprismToolbarButton"));
    connect(m_updateButton, &QPushButton::clicked, this, [this] {
        if (m_pendingUpdates.isEmpty()) {
            checkForUpdates();
            return;
        }
        QList<int> rows;
        for (auto it = m_pendingUpdates.cbegin(); it != m_pendingUpdates.cend(); ++it)
            if (int row = m_model->rowOfPath(it.key()); row >= 0)
                rows.append(row);
        updateRows(rows);
    });
    toolbar->addWidget(m_updateButton);

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
    m_proxy->setDynamicSortFilter(true);
    m_view = new QTreeView(this);
    m_view->setObjectName(QStringLiteral("reprismContentList"));
    auto* header = new SelectAllHeader(
        [this] {
            const QList<int> rows = visibleRows();
            const auto checked = std::count_if(rows.begin(), rows.end(), [this](int row) { return m_model->isChecked(row); });
            return checked == 0 ? Qt::Unchecked : checked == rows.size() ? Qt::Checked : Qt::PartiallyChecked;
        },
        m_view);
    header->setObjectName(QStringLiteral("reprismContentHeader"));
    m_view->setHeader(header);
    m_view->setModel(m_proxy);
    m_view->setRootIsDecorated(false);
    m_view->setUniformRowHeights(true);
    m_view->setAlternatingRowColors(true);
    m_view->setSortingEnabled(false);  // the Sort dropdown drives sorting
    m_view->setSelectionMode(QAbstractItemView::NoSelection);  // rows are picked with the checkboxes
    m_view->setMouseTracking(true);
    m_view->setIconSize(QSize(28, 28));
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->setItemDelegateForColumn(ContentModel::SelectColumn, new SelectDelegate(m_view));
    m_view->setItemDelegateForColumn(
        ContentModel::ActionsColumn, new ActionsDelegate(m_view, [this](const QModelIndex& index, ActionsDelegate::Action action) {
            const int row = m_proxy->mapToSource(index).row();
            switch (action) {
                case ActionsDelegate::VersionAction:
                    if (!m_model->index(row, 0).data(ContentModel::UpdateRole).toString().isEmpty())
                        updateRows({ row });
                    else
                        changeVersion(row);
                    break;
                case ActionsDelegate::ToggleAction:
                    setEnabled({ row }, !m_model->index(row, 0).data(ContentModel::EnabledRole).toBool());
                    break;
                case ActionsDelegate::DeleteAction:
                    deleteRows({ row });
                    break;
            }
        }));
    m_view->viewport()->installEventFilter(new HoverRepaint(m_view));
    header->setSortIndicatorShown(false);
    header->setSectionsClickable(true);  // setSortingEnabled(false) turned this off; the select-all box needs it
    header->setStretchLastSection(false);
    header->setMinimumSectionSize(36);
    header->setSectionResizeMode(ContentModel::SelectColumn, QHeaderView::Fixed);
    header->resizeSection(ContentModel::SelectColumn, 40);
    header->setSectionResizeMode(ContentModel::NameColumn, QHeaderView::Stretch);
    for (int c = ContentModel::VersionColumn; c < ContentModel::ActionsColumn; c++)
        header->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ContentModel::ActionsColumn, QHeaderView::Fixed);
    header->resizeSection(ContentModel::ActionsColumn, ActionsDelegate::Width);
    layout->addWidget(m_view, 1);

    // Footer: counts, plus bulk actions while rows are checked
    auto* footer = new QHBoxLayout();
    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("reprismMuted"));
    footer->addWidget(m_summary);
    footer->addStretch(1);
    m_bulkBar = new QWidget(this);
    auto* bulk = new QHBoxLayout(m_bulkBar);
    bulk->setContentsMargins(0, 0, 0, 0);
    bulk->setSpacing(6);
    m_bulkLabel = new QLabel(m_bulkBar);
    bulk->addWidget(m_bulkLabel);
    bulk->addSpacing(6);
    const QColor iconColor = palette().color(QPalette::ButtonText);
    auto addBulk = [this, bulk](const QIcon& icon, const QString& text, auto handler) {
        auto* button = new QPushButton(icon, text, m_bulkBar);
        button->setObjectName(QStringLiteral("reprismToolbarButton"));
        connect(button, &QPushButton::clicked, this, handler);
        bulk->addWidget(button);
    };
    addBulk(QIcon(), tr("Enable"), [this] { setEnabled(m_model->checkedRows(), true); });
    addBulk(QIcon(), tr("Disable"), [this] { setEnabled(m_model->checkedRows(), false); });
    addBulk(ReprismIcons::icon("trash-2", iconColor), tr("Delete"), [this] { deleteRows(m_model->checkedRows()); });
    addBulk(ReprismIcons::icon("x", iconColor), tr("Clear"), [this] { m_model->setChecked(m_model->checkedRows(), false); });
    footer->addWidget(m_bulkBar);
    layout->addLayout(footer);

    connect(header, &QHeaderView::sectionClicked, this, [this](int section) {
        if (section != ContentModel::SelectColumn)
            return;
        // Select every visible row, or clear them if they are all selected already.
        const QList<int> rows = visibleRows();
        const bool all = !rows.isEmpty() && std::all_of(rows.begin(), rows.end(), [this](int row) { return m_model->isChecked(row); });
        m_model->setChecked(rows, !all);
    });
    connect(m_filters, &QButtonGroup::idClicked, this, [this](int id) { m_proxy->setTypeFilter(id - 1); });
    connect(m_search, &QLineEdit::textChanged, m_proxy, &ContentFilterProxy::setSearch);
    connect(m_sort, &QComboBox::currentIndexChanged, this, &ContentTab::applySort);
    connect(m_model, &ContentModel::checksChanged, this, &ContentTab::updateCounts);
    connect(m_model, &QAbstractItemModel::dataChanged, this, &ContentTab::updateCounts);
    connect(m_proxy, &QAbstractItemModel::rowsInserted, this, &ContentTab::updateCounts);
    connect(m_proxy, &QAbstractItemModel::rowsRemoved, this, &ContentTab::updateCounts);
    connect(m_proxy, &QAbstractItemModel::layoutChanged, this, &ContentTab::updateCounts);
    connect(m_proxy, &QAbstractItemModel::modelReset, this, &ContentTab::updateCounts);
    connect(m_view, &QWidget::customContextMenuRequested, this, &ContentTab::showContextMenu);

    // Check for updates automatically once the content has finished loading.
    m_updateCheckTimer = new QTimer(this);
    m_updateCheckTimer->setSingleShot(true);
    m_updateCheckTimer->setInterval(1500);
    connect(m_updateCheckTimer, &QTimer::timeout, this, [this] {
        m_autoChecked = true;
        checkForUpdates();
    });
    connect(m_model, &QAbstractItemModel::modelReset, this, [this] {
        if (!m_autoChecked)
            m_updateCheckTimer->start();
    });
    applySort(0);
    updateCounts();
}

void ContentTab::setInstance(MinecraftInstance* instance)
{
    m_instance = instance;
    m_autoChecked = false;
    m_pendingUpdates.clear();
    m_model->setUpdates({});
    m_updateButton->setText(tr("Check for updates"));
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

QList<int> ContentTab::visibleRows() const
{
    QList<int> rows;
    for (int row = 0; row < m_proxy->rowCount(); row++)
        rows.append(m_proxy->mapToSource(m_proxy->index(row, 0)).row());
    return rows;
}

void ContentTab::updateCounts()
{
    for (auto* button : m_filters->buttons()) {
        const int type = m_filters->id(button) - 1;
        const int count = type < 0 ? m_model->rowCount() : m_model->count(static_cast<ContentModel::Type>(type));
        button->setText(QString("%1  %2").arg(button->property("label").toString()).arg(count));
    }
    int enabled = 0;
    for (int row : visibleRows())
        if (m_model->index(row, 0).data(ContentModel::EnabledRole).toBool())
            enabled++;
    m_summary->setText(tr("%1 shown, %2 enabled").arg(m_proxy->rowCount()).arg(enabled));

    const int checked = static_cast<int>(m_model->checkedRows().size());
    m_bulkBar->setVisible(checked > 0);
    m_bulkLabel->setText(tr("%1 selected").arg(checked));
    m_view->header()->viewport()->update();  // refresh the select-all checkbox
}

void ContentTab::setEnabled(const QList<int>& rows, bool enabled)
{
    QHash<ResourceFolderModel*, QModelIndexList> byModel;
    for (int row : rows) {
        auto [model, index] = m_model->source(row);
        byModel[model].append(index);
    }
    for (auto it = byModel.begin(); it != byModel.end(); ++it)
        it.key()->setResourceEnabled(it.value(), enabled ? EnableAction::ENABLE : EnableAction::DISABLE);
}

void ContentTab::deleteRows(const QList<int>& rows)
{
    if (rows.isEmpty())
        return;
    const QString question = rows.size() == 1 ? tr("Delete this item? This cannot be undone.")
                                              : tr("Delete %1 items? This cannot be undone.").arg(rows.size());
    if (QMessageBox::question(this, tr("Delete content"), question) != QMessageBox::Yes)
        return;
    // each folder model deletes its own files
    QHash<ResourceFolderModel*, QModelIndexList> byModel;
    for (int row : rows) {
        auto [model, index] = m_model->source(row);
        byModel[model].append(index);
    }
    for (auto it = byModel.begin(); it != byModel.end(); ++it)
        it.key()->deleteResources(it.value());
}

void ContentTab::showContextMenu(const QPoint& pos)
{
    const QModelIndex clicked = m_view->indexAt(pos);
    if (!clicked.isValid())
        return;
    // Act on the checked rows when right-clicking one of them, otherwise on the clicked row.
    const int clickedRow = m_proxy->mapToSource(clicked).row();
    const QList<int> rows = m_model->isChecked(clickedRow) ? m_model->checkedRows() : QList<int>{ clickedRow };

    const QColor iconColor = palette().color(QPalette::WindowText);
    QMenu menu(this);
    const bool anyUpdate = std::any_of(rows.begin(), rows.end(), [this](int row) { return m_pendingUpdates.contains(m_model->pathOf(row)); });
    if (anyUpdate)
        menu.addAction(ReprismIcons::icon("download", iconColor), tr("Update"), this, [this, rows] { updateRows(rows); });
    if (rows.size() == 1 && m_model->index(rows.first(), 0).data(ContentModel::HasSourceRole).toBool())
        menu.addAction(ReprismIcons::icon("history", iconColor), tr("Change version…"), this, [this, rows] { changeVersion(rows.first()); });
    if (anyUpdate || rows.size() == 1)
        menu.addSeparator();
    menu.addAction(tr("Enable"), this, [this, rows] { setEnabled(rows, true); });
    menu.addAction(tr("Disable"), this, [this, rows] { setEnabled(rows, false); });
    menu.addSeparator();
    if (Resource* first = m_model->resource(rows.first())) {
        menu.addAction(ReprismIcons::icon("folder-open", iconColor), tr("Show in folder"), this,
                       [first] { DesktopServices::openPath(first->fileinfo().absolutePath(), true); });
        if (!first->homepage().isEmpty())
            menu.addAction(ReprismIcons::icon("external-link", iconColor), tr("Open homepage"), this,
                           [first] { QDesktopServices::openUrl(QUrl(first->homepage())); });
    }
    menu.addSeparator();
    menu.addAction(ReprismIcons::icon("trash-2", iconColor), tr("Delete"), this, [this, rows] { deleteRows(rows); });
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

bool ContentTab::confirmWhileRunning(const QString& title)
{
    if (!m_instance || !m_instance->isRunning())
        return true;
    const auto response = CustomMessageBox::selectable(this, title,
                                                       tr("The game is running. Changing content now may cause duplicated files and crashes, "
                                                          "and old files may not be removed while they are in use.\n\nContinue anyway?"),
                                                       QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                              ->exec();
    return response == QMessageBox::Yes;
}

void ContentTab::checkForUpdates()
{
    if (!m_instance || m_checkTask || APPLICATION->settings()->get("ModMetadataDisabled").toBool())
        return;
    // Read the Minecraft version and loader from mmc-pack.json: the pack profile may not be loaded yet.
    const auto components = InstanceSummary::components(m_instance);
    if (components.minecraft.isEmpty()) {
        qDebug() << "Content update check skipped: Minecraft version of" << m_instance->name() << "is not known";
        return;
    }
    static const QHash<QString, ModPlatform::ModLoaderType> loaderTypes = {
        { "Fabric", ModPlatform::Fabric }, { "Quilt", ModPlatform::Quilt },           { "Forge", ModPlatform::Forge },
        { "NeoForge", ModPlatform::NeoForge }, { "LiteLoader", ModPlatform::LiteLoader },
    };
    QList<ModPlatform::ModLoaderType> modLoaders;
    if (loaderTypes.contains(components.loader))
        modLoaders.append(loaderTypes.value(components.loader));

    // Only content that came from Modrinth or CurseForge (i.e. has metadata) can be checked.
    m_checks.clear();
    auto all = makeShared<SequentialTask>(tr("Checking for updates"));
    const bool canUseFlame = !APPLICATION->getFlameAPIKey().isEmpty();
    for (const auto& [folder, type] : m_model->folders()) {
        QList<Resource*> modrinth, flame;
        for (int i = 0; i < folder->size(); i++) {
            Resource& resource = folder->at(i);
            if (!resource.metadata())
                continue;
            (resource.metadata()->provider == ModPlatform::ResourceProvider::MODRINTH ? modrinth : flame).append(&resource);
        }
        QList<ModPlatform::ModLoaderType> loaders;
        if (type == ContentModel::Type::Mod)
            loaders = modLoaders;
        else if (type == ContentModel::Type::DataPack)
            loaders = { ModPlatform::ModLoaderType::DataPack };
        for (bool isModrinth : { true, false }) {
            QList<Resource*>& resources = isModrinth ? modrinth : flame;
            if (resources.isEmpty() || (!isModrinth && !canUseFlame))
                continue;
            auto check = std::make_unique<UpdateCheck>();
            check->resources = resources;
            check->versions = { Version(components.minecraft) };
            check->folder = folder;
            if (isModrinth)
                check->task = makeShared<ModrinthCheckUpdate>(check->resources, check->versions, loaders, folder);
            else
                check->task = makeShared<FlameCheckUpdate>(check->resources, check->versions, loaders, folder);
            all->addTask(check->task);
            m_checks.push_back(std::move(check));
        }
    }
    if (m_checks.empty())
        return;
    qDebug() << "Checking" << m_instance->name() << "content for updates," << m_checks.size() << "check task(s)";

    m_updateButton->setText(tr("Checking…"));
    m_updateButton->setEnabled(false);
    m_checkTask = all;
    connect(all.get(), &Task::finished, this, [this] {
        m_pendingUpdates.clear();
        QHash<QString, QString> versions;
        for (const auto& check : m_checks) {
            auto updates = static_cast<CheckUpdateTask*>(check->task.get())->getUpdates();
            for (auto& update : updates) {
                for (Resource* resource : check->resources) {
                    if (resource->name() != update.name)
                        continue;
                    const QString path = resource->fileinfo().absoluteFilePath();
                    m_pendingUpdates.insert(path, { update.newVersion, update.download });
                    versions.insert(path, update.newVersion);
                }
            }
        }
        m_model->setUpdates(versions);
        m_updateButton->setEnabled(true);
        m_updateButton->setText(m_pendingUpdates.isEmpty() ? tr("Check for updates") : tr("Update all (%1)").arg(m_pendingUpdates.size()));
        // Release the finished task once its signal has returned.
        QTimer::singleShot(0, this, [this] {
            m_checkTask.reset();
            m_checks.clear();
        });
    });
    all->start();
}

void ContentTab::updateRows(const QList<int>& rows)
{
    QStringList paths;
    for (int row : rows)
        if (m_pendingUpdates.contains(m_model->pathOf(row)))
            paths.append(m_model->pathOf(row));
    if (paths.isEmpty() || !confirmWhileRunning(tr("Confirm update")))
        return;

    ConcurrentTask tasks(tr("Updating content"), APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
    connect(&tasks, &Task::failed, this,
            [this](const QString& reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show(); });
    for (const QString& path : paths)
        tasks.addTask(m_pendingUpdates.take(path).download);
    ProgressDialog progress(this);
    progress.showSkipButton();
    progress.execWithTask(&tasks);

    m_model->setUpdates({});
    m_updateButton->setText(tr("Check for updates"));
    m_model->reload();
    m_autoChecked = false;  // re-check once the folders have reloaded
}

void ContentTab::changeVersion(int row)
{
    Resource* resource = m_model->resource(row);
    if (!resource || !resource->metadata() || !confirmWhileRunning(tr("Confirm version change")))
        return;
    auto [folder, index] = m_model->source(row);
    using ResourceDownload::ResourceDownloadDialog;
    ResourceDownloadDialog* dialog = nullptr;
    switch (static_cast<ContentModel::Type>(m_model->index(row, 0).data(ContentModel::TypeRole).toInt())) {
        case ContentModel::Type::Mod:
            dialog = ResourceDownloadDialog::createMod(this, folder, m_instance, true);
            break;
        case ContentModel::Type::ResourcePack:
            dialog = ResourceDownloadDialog::createResourcePack(this, folder, m_instance, true);
            break;
        case ContentModel::Type::DataPack:
            dialog = ResourceDownloadDialog::createDataPack(this, folder, m_instance, true);
            break;
        case ContentModel::Type::Shader:
            dialog = ResourceDownloadDialog::createShaderPack(this, folder, m_instance, true);
            break;
    }
    if (!dialog)
        return;
    dialog->setResourceMetadata(resource->metadata());
    connect(dialog, &QDialog::finished, this, [this, dialog, folder = folder](int result) {
        if (result != 0) {
            ConcurrentTask tasks(tr("Changing version"), APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
            connect(&tasks, &Task::failed, this,
                    [this](const QString& reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show(); });
            for (auto& task : dialog->getTasks())
                tasks.addTask(task);
            ProgressDialog progress(this);
            progress.showSkipButton();
            progress.execWithTask(&tasks);
            folder->update();
        }
        dialog->deleteLater();
    });
    dialog->open();
}
