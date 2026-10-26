// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QStringList>

#include "modplatform/ModIndex.h"

/// Project icons from Modrinth and CurseForge, for content whose files don't bundle an icon (most NeoForge mods,
/// all shader packs). Requests are batched per platform and icons are cached on disk.
class ProjectIcons : public QObject {
    Q_OBJECT
   public:
    static ProjectIcons* instance();

    /// The cached icon, or a null pixmap while it is being fetched (iconsReady() fires when it arrives).
    QPixmap icon(ModPlatform::ResourceProvider provider, const QString& projectId);

   signals:
    void iconsReady();

   private:
    explicit ProjectIcons(QObject* parent = nullptr);
    QString key(ModPlatform::ResourceProvider provider, const QString& projectId) const;
    QString cachePath(const QString& key) const;
    void flush();
    void downloadIcons(const QHash<QString, QString>& urlsByKey);
    void store(const QString& key, const QByteArray& data);

    QHash<QString, QPixmap> m_icons;
    QSet<QString> m_requested;  ///< keys fetched or being fetched this session
    QStringList m_pendingModrinth;
    QStringList m_pendingFlame;
    bool m_flushQueued = false;
};
