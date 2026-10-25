// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QObject>
#include <QSet>

class MinecraftInstance;
class QNetworkReply;

/// Looks up the creator of an instance's modpack (Modrinth or CurseForge) once, and caches it in the
/// instance's "ManagedPackAuthor" setting.
class ModpackAuthors : public QObject {
    Q_OBJECT
   public:
    static ModpackAuthors* instance();

    /// The cached creator, or an empty string. Starts a lookup if none was cached or attempted yet.
    QString author(MinecraftInstance* instance);

   signals:
    void resolved(const QString& instanceId);

   private:
    explicit ModpackAuthors(QObject* parent = nullptr);
    void store(const QString& instanceId, const QString& author);

    QSet<QString> m_attempted;  // instance ids looked up during this session
};
