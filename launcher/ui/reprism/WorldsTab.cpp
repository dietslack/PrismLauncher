// SPDX-License-Identifier: GPL-3.0-only
#include "WorldsTab.h"

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QFileSystemWatcher>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <sstream>

#include <io/stream_reader.h>
#include <tag_compound.h>
#include <tag_list.h>
#include <tag_string.h>

#include "Application.h"
#include "DesktopServices.h"
#include "FileSystem.h"
#include "LaunchMode.h"
#include "icons/IconList.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/World.h"
#include "minecraft/WorldList.h"
#include "minecraft/launch/MinecraftTarget.h"
#include "ui/reprism/ReprismIcons.h"

namespace {
enum Roles { KindRole = Qt::UserRole + 1, SubtitleRole, TargetRole, PathRole };
enum Kind { Section, WorldItem, ServerItem };

struct ServerEntry {
    QString name;
    QString address;
    QByteArray icon;
};

/// Reads the server list (name, address, icon) from servers.dat.
QList<ServerEntry> readServers(const QString& path)
{
    QList<ServerEntry> servers;
    try {
        auto data = FS::read(path);
        if (!data)
            return servers;
        std::istringstream stream(std::string(data->constData(), data->size()));
        auto root = nbt::io::read_compound(stream).second;
        if (!root || !root->has_key("servers", nbt::tag_type::List))
            return servers;
        for (auto& value : root->at("servers").as<nbt::tag_list>()) {
            auto& server = value.as<nbt::tag_compound>();
            ServerEntry entry;
            if (server.has_key("name", nbt::tag_type::String))
                entry.name = QString::fromStdString(server.at("name").as<nbt::tag_string>().get());
            if (server.has_key("ip", nbt::tag_type::String))
                entry.address = QString::fromStdString(server.at("ip").as<nbt::tag_string>().get());
            if (server.has_key("icon", nbt::tag_type::String))
                entry.icon = QByteArray::fromBase64(QByteArray::fromStdString(server.at("icon").as<nbt::tag_string>().get()));
            servers.append(entry);
        }
    } catch (...) {
        qWarning() << "Could not read" << path;
    }
    return servers;
}

/// Rounded, nearest-neighbour scaled icon for a world or server.
QIcon roundedIcon(const QImage& source, const QIcon& fallback)
{
    constexpr int Size = 48;
    QIcon result;
    for (qreal dpr : { 1.0, 2.0 }) {
        const int physical = qRound(Size * dpr);
        QImage image = source.isNull() ? ReprismIcons::nearestNeighbor(fallback, Size, dpr)
                                       : source.scaled(physical, physical, Qt::KeepAspectRatio, Qt::FastTransformation);
        QPixmap rounded(physical, physical);
        rounded.fill(Qt::transparent);
        QPainter painter(&rounded);
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(0, 0, physical, physical), 8 * dpr, 8 * dpr);
        painter.setClipPath(clip);
        image.setDevicePixelRatio(1.0);
        painter.drawImage(QRect(0, 0, physical, physical), image);
        painter.end();
        rounded.setDevicePixelRatio(dpr);
        result.addPixmap(rounded);
    }
    return result;
}

QString ago(const QDateTime& when)
{
    if (!when.isValid())
        return {};
    const qint64 days = when.daysTo(QDateTime::currentDateTime());
    if (days <= 0)
        return QObject::tr("today");
    if (days == 1)
        return QObject::tr("yesterday");
    return QObject::tr("%1 days ago").arg(days);
}

/// Two-line rows (title + muted subtitle) and bold section headers.
class WorldsDelegate : public QStyledItemDelegate {
   public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (index.data(KindRole).toInt() == Section)
            return { option.rect.width(), 40 };
        return { option.rect.width(), 64 };
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const QPalette pal = QApplication::palette();
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRect r = option.rect;
        if (index.data(KindRole).toInt() == Section) {
            QFont font = option.font;
            font.setBold(true);
            painter->setFont(font);
            painter->setPen(pal.color(QPalette::Text));
            painter->drawText(r.adjusted(4, 12, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, index.data(Qt::DisplayRole).toString());
            painter->restore();
            return;
        }
        if (option.state & (QStyle::State_Selected | QStyle::State_MouseOver)) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(pal.color(option.state & QStyle::State_Selected ? QPalette::Light : QPalette::Button));
            painter->drawRoundedRect(QRectF(r).adjusted(0, 2, 0, -2), 10, 10);
        }
        const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        icon.paint(painter, QRect(r.left() + 8, r.top() + 8, 48, 48));
        const int textLeft = r.left() + 68;
        QFont bold = option.font;
        bold.setBold(true);
        const QFontMetrics boldMetrics(bold);
        const QFontMetrics metrics(option.font);
        const int top = r.top() + (r.height() - boldMetrics.height() - metrics.height() - 2) / 2;
        painter->setFont(bold);
        painter->setPen(pal.color(QPalette::Text));
        painter->drawText(QRect(textLeft, top, r.right() - textLeft - 8, boldMetrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                          boldMetrics.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, r.right() - textLeft - 8));
        painter->setFont(option.font);
        painter->setPen(pal.color(QPalette::PlaceholderText));
        painter->drawText(QRect(textLeft, top + boldMetrics.height() + 2, r.right() - textLeft - 8, metrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                          metrics.elidedText(index.data(SubtitleRole).toString(), Qt::ElideRight, r.right() - textLeft - 8));
        painter->restore();
    }
};
}  // namespace

WorldsTab::WorldsTab(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("reprismWorldsList"));
    m_list->setItemDelegate(new WorldsDelegate(m_list));
    m_list->setMouseTracking(true);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_list, 1);
    m_hint = new QLabel(this);
    m_hint->setObjectName(QStringLiteral("reprismMuted"));
    layout->addWidget(m_hint);

    // Worlds and servers change while the launcher is open (mostly while playing): watch the files.
    m_watcher = new QFileSystemWatcher(this);
    auto changed = [this] {
        watchFiles();  // folders/files may have been created or replaced
        reload();
    };
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, changed);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, changed);

    connect(m_list, &QListWidget::itemDoubleClicked, this, &WorldsTab::join);
    connect(m_list, &QWidget::customContextMenuRequested, this, &WorldsTab::showContextMenu);
}

void WorldsTab::setInstance(MinecraftInstance* instance)
{
    if (m_instance) {
        disconnect(m_instance->worldList(), nullptr, this, nullptr);
        disconnect(m_instance, nullptr, this, nullptr);
        m_instance->worldList()->stopWatching();
    }
    m_instance = instance;
    if (m_instance) {
        // World details load after the list itself; repopulate (once per event-loop pass) as they arrive.
        WorldList* worlds = m_instance->worldList();
        connect(worlds, &QAbstractItemModel::modelReset, this, &WorldsTab::queuePopulate);
        connect(worlds, &QAbstractItemModel::dataChanged, this, &WorldsTab::queuePopulate);
        connect(worlds, &QAbstractItemModel::rowsInserted, this, &WorldsTab::queuePopulate);
        connect(worlds, &QAbstractItemModel::rowsRemoved, this, &WorldsTab::queuePopulate);
        worlds->startWatching();
        // New worlds and servers usually appear while playing: refresh when the game closes.
        connect(m_instance, &BaseInstance::runningStatusChanged, this, [this](bool running) {
            if (!running)
                reload();
        });
    }
    watchFiles();
    reload();
}

void WorldsTab::queuePopulate()
{
    if (m_populateQueued)
        return;
    m_populateQueued = true;
    QMetaObject::invokeMethod(this, &WorldsTab::populate, Qt::QueuedConnection);
}

void WorldsTab::watchFiles()
{
    if (!m_watcher->files().isEmpty())
        m_watcher->removePaths(m_watcher->files());
    if (!m_watcher->directories().isEmpty())
        m_watcher->removePaths(m_watcher->directories());
    if (!m_instance)
        return;
    QStringList paths;
    for (const QString& path : { m_instance->gameRoot(), m_instance->worldDir(), FS::PathCombine(m_instance->gameRoot(), "servers.dat") })
        if (QFileInfo::exists(path))
            paths.append(path);
    if (!paths.isEmpty())
        m_watcher->addPaths(paths);
}

void WorldsTab::addSection(const QString& title)
{
    auto* item = new QListWidgetItem(title, m_list);
    item->setData(KindRole, Section);
    item->setFlags(Qt::NoItemFlags);
}

void WorldsTab::reload()
{
    if (m_instance)
        m_instance->worldList()->update();
    queuePopulate();
}

void WorldsTab::populate()
{
    m_populateQueued = false;
    m_list->clear();
    if (!m_instance)
        return;
    const QIcon worldFallback = APPLICATION->icons()->getIcon("grass");
    const QIcon serverFallback = APPLICATION->icons()->getIcon("default");

    WorldList* worlds = m_instance->worldList();
    addSection(tr("Worlds  %1").arg(worlds->rowCount()));
    for (int row = 0; row < worlds->rowCount(); row++) {
        auto* world = static_cast<World*>(worlds->data(worlds->index(row), WorldList::ObjectRole).value<void*>());
        if (!world)
            continue;
        auto* item = new QListWidgetItem(world->name().isEmpty() ? world->folderName() : world->name(), m_list);
        item->setData(KindRole, WorldItem);
        item->setData(TargetRole, world->folderName());
        item->setData(PathRole, FS::PathCombine(m_instance->worldDir(), world->folderName()));
        QStringList subtitle;
        if (world->gameType().type != GameType::Unknown)
            subtitle << world->gameType().toTranslatedString();
        if (world->lastPlayed().isValid())
            subtitle << tr("Played %1").arg(ago(world->lastPlayed()));
        item->setData(SubtitleRole, subtitle.join(QStringLiteral("  •  ")));
        item->setIcon(roundedIcon(QImage(world->iconFile()), worldFallback));
    }

    const auto servers = readServers(FS::PathCombine(m_instance->gameRoot(), "servers.dat"));
    addSection(tr("Servers  %1").arg(servers.size()));
    for (const auto& server : servers) {
        auto* item = new QListWidgetItem(server.name.isEmpty() ? server.address : server.name, m_list);
        item->setData(KindRole, ServerItem);
        item->setData(TargetRole, server.address);
        item->setData(SubtitleRole, server.address);
        item->setIcon(roundedIcon(QImage::fromData(server.icon), serverFallback));
    }

    const bool canJoinWorlds = m_instance->traits().contains("feature:is_quick_play_singleplayer");
    m_hint->setText(canJoinWorlds ? tr("Double-click a world or server to launch straight into it.")
                                  : tr("Double-click a server to launch and join it."));
}

void WorldsTab::join(QListWidgetItem* item)
{
    if (!m_instance || !item)
        return;
    const int kind = item->data(KindRole).toInt();
    const QString target = item->data(TargetRole).toString();
    if (kind == WorldItem && m_instance->traits().contains("feature:is_quick_play_singleplayer"))
        APPLICATION->launch(m_instance, LaunchMode::Normal, std::make_shared<MinecraftTarget>(MinecraftTarget::parse(target, true)));
    else if (kind == ServerItem)
        APPLICATION->launch(m_instance, LaunchMode::Normal, std::make_shared<MinecraftTarget>(MinecraftTarget::parse(target, false)));
}

void WorldsTab::showContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = m_list->itemAt(pos);
    if (!item || item->data(KindRole).toInt() == Section)
        return;
    const QColor iconColor = palette().color(QPalette::WindowText);
    QMenu menu(this);
    const int kind = item->data(KindRole).toInt();
    if (kind == ServerItem || m_instance->traits().contains("feature:is_quick_play_singleplayer"))
        menu.addAction(ReprismIcons::icon("play", iconColor), kind == ServerItem ? tr("Join server") : tr("Play world"), this,
                       [this, item] { join(item); });
    if (kind == WorldItem)
        menu.addAction(ReprismIcons::icon("folder-open", iconColor), tr("Open world folder"), this,
                       [item] { DesktopServices::openPath(item->data(PathRole).toString(), true); });
    menu.exec(m_list->viewport()->mapToGlobal(pos));
}
