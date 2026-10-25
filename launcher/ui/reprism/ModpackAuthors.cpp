// SPDX-License-Identifier: GPL-3.0-only
#include "ModpackAuthors.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#include "Application.h"
#include "BuildConfig.h"
#include "InstanceList.h"
#include "minecraft/MinecraftInstance.h"
#include "settings/SettingsObject.h"

ModpackAuthors::ModpackAuthors(QObject* parent) : QObject(parent) {}

ModpackAuthors* ModpackAuthors::instance()
{
    static auto* s_instance = new ModpackAuthors(APPLICATION);
    return s_instance;
}

QString ModpackAuthors::author(MinecraftInstance* instance)
{
    if (!instance || !instance->isManagedPack())
        return {};
    const QString cached = instance->settings()->get("ManagedPackAuthor").toString();
    if (!cached.isEmpty() || m_attempted.contains(instance->id()))
        return cached;

    const QString type = instance->getManagedPackType();
    const QString packId = instance->getManagedPackID();
    if (packId.isEmpty() || (type != "modrinth" && type != "flame"))
        return {};
    m_attempted.insert(instance->id());

    QNetworkRequest request;
    request.setHeader(QNetworkRequest::UserAgentHeader, APPLICATION->getUserAgent());
    if (type == "modrinth") {
        // The search index reports the same author the Modrinth website shows, including for organization-owned packs.
        QUrl url(BuildConfig.MODRINTH_PROD_URL + "/search");
        QUrlQuery query;
        query.addQueryItem("facets", QString("[[\"project_id:%1\"]]").arg(packId));
        url.setQuery(query);
        request.setUrl(url);
    } else {
        const QString key = APPLICATION->getFlameAPIKey();
        if (key.isEmpty())
            return {};
        request.setUrl(QUrl(BuildConfig.FLAME_BASE_URL + "/mods/" + packId));
        request.setRawHeader("x-api-key", key.toUtf8());
    }

    QNetworkReply* reply = APPLICATION->network()->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, type, id = instance->id()] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Could not look up modpack author:" << reply->errorString();
            return;
        }
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        QString name;
        if (type == "modrinth") {
            const auto hits = root.value("hits").toArray();
            if (!hits.isEmpty())
                name = hits.first().toObject().value("author").toString();
        } else {
            const auto authors = root.value("data").toObject().value("authors").toArray();
            if (!authors.isEmpty())
                name = authors.first().toObject().value("name").toString();
        }
        if (!name.isEmpty())
            store(id, name);
    });
    return {};
}

void ModpackAuthors::store(const QString& instanceId, const QString& author)
{
    if (auto* instance = APPLICATION->instances()->getInstanceById(instanceId)) {
        instance->settings()->set("ManagedPackAuthor", author);
        emit resolved(instanceId);
    }
}
