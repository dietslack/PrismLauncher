// SPDX-License-Identifier: GPL-3.0-only
#include "ProjectIcons.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>

#include "Application.h"
#include "BuildConfig.h"

ProjectIcons::ProjectIcons(QObject* parent) : QObject(parent) {}

ProjectIcons* ProjectIcons::instance()
{
    static auto* s_instance = new ProjectIcons(APPLICATION);
    return s_instance;
}

QString ProjectIcons::key(ModPlatform::ResourceProvider provider, const QString& projectId) const
{
    return QString("%1-%2").arg(provider == ModPlatform::ResourceProvider::MODRINTH ? "modrinth" : "curseforge", projectId);
}

QString ProjectIcons::cachePath(const QString& key) const
{
    return QDir(APPLICATION->dataRoot()).filePath("cache/reprism-project-icons/" + key);
}

QPixmap ProjectIcons::icon(ModPlatform::ResourceProvider provider, const QString& projectId)
{
    if (projectId.isEmpty())
        return {};
    const QString k = key(provider, projectId);
    if (auto it = m_icons.constFind(k); it != m_icons.constEnd())
        return *it;
    if (m_requested.contains(k))
        return {};
    m_requested.insert(k);

    // Disk cache first
    QFile cached(cachePath(k));
    if (cached.open(QIODevice::ReadOnly)) {
        const QImage image = QImage::fromData(cached.readAll());
        if (!image.isNull()) {
            m_icons.insert(k, QPixmap::fromImage(image));
            return m_icons.value(k);
        }
    }

    // Then the platform, batched
    if (provider == ModPlatform::ResourceProvider::MODRINTH)
        m_pendingModrinth.append(projectId);
    else if (!APPLICATION->getFlameAPIKey().isEmpty())
        m_pendingFlame.append(projectId);
    if (!m_flushQueued) {
        m_flushQueued = true;
        QTimer::singleShot(150, this, &ProjectIcons::flush);
    }
    return {};
}

void ProjectIcons::flush()
{
    m_flushQueued = false;
    const QByteArray userAgent = APPLICATION->getUserAgent().toUtf8();

    if (!m_pendingModrinth.isEmpty()) {
        QUrl url(BuildConfig.MODRINTH_PROD_URL + "/projects");
        QUrlQuery query;
        query.addQueryItem("ids", QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(m_pendingModrinth)).toJson(QJsonDocument::Compact)));
        url.setQuery(query);
        QNetworkRequest request(url);
        request.setRawHeader("User-Agent", userAgent);
        m_pendingModrinth.clear();
        QNetworkReply* reply = APPLICATION->network()->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            reply->deleteLater();
            QHash<QString, QString> urls;
            for (const auto& value : QJsonDocument::fromJson(reply->readAll()).array()) {
                const auto project = value.toObject();
                const QString iconUrl = project.value("icon_url").toString();
                if (!iconUrl.isEmpty())
                    urls.insert(key(ModPlatform::ResourceProvider::MODRINTH, project.value("id").toString()), iconUrl);
            }
            downloadIcons(urls);
        });
    }

    if (!m_pendingFlame.isEmpty()) {
        QJsonArray ids;
        for (const QString& id : m_pendingFlame)
            ids.append(id.toLongLong());
        m_pendingFlame.clear();
        QNetworkRequest request(QUrl(BuildConfig.FLAME_BASE_URL + "/mods"));
        request.setRawHeader("User-Agent", userAgent);
        request.setRawHeader("x-api-key", APPLICATION->getFlameAPIKey().toUtf8());
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QNetworkReply* reply = APPLICATION->network()->post(request, QJsonDocument(QJsonObject{ { "modIds", ids } }).toJson(QJsonDocument::Compact));
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            reply->deleteLater();
            QHash<QString, QString> urls;
            const auto mods = QJsonDocument::fromJson(reply->readAll()).object().value("data").toArray();
            for (const auto& value : mods) {
                const auto mod = value.toObject();
                const QString iconUrl = mod.value("logo").toObject().value("thumbnailUrl").toString();
                if (!iconUrl.isEmpty())
                    urls.insert(key(ModPlatform::ResourceProvider::FLAME, QString::number(mod.value("id").toInteger())), iconUrl);
            }
            downloadIcons(urls);
        });
    }
}

void ProjectIcons::downloadIcons(const QHash<QString, QString>& urlsByKey)
{
    for (auto it = urlsByKey.cbegin(); it != urlsByKey.cend(); ++it) {
        QNetworkRequest request{ QUrl(it.value()) };
        request.setRawHeader("User-Agent", APPLICATION->getUserAgent().toUtf8());
        QNetworkReply* reply = APPLICATION->network()->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, k = it.key()] {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError)
                store(k, reply->readAll());
        });
    }
}

void ProjectIcons::store(const QString& key, const QByteArray& data)
{
    const QImage image = QImage::fromData(data);
    if (image.isNull())
        return;
    QDir().mkpath(QFileInfo(cachePath(key)).absolutePath());
    QFile file(cachePath(key));
    if (file.open(QIODevice::WriteOnly))
        file.write(data);
    m_icons.insert(key, QPixmap::fromImage(image));
    emit iconsReady();
}
