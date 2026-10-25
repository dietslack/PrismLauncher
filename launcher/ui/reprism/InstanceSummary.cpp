// SPDX-License-Identifier: GPL-3.0-only
#include "InstanceSummary.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "Application.h"
#include "MMCTime.h"
#include "minecraft/MinecraftInstance.h"
#include "settings/SettingsObject.h"
#include "ui/reprism/ModpackAuthors.h"

namespace InstanceSummary {

static QHash<QString, QString> s_versionLines;

// Lets lupdate find the strings below under the "InstanceSummary" context.
struct Strings {
    Q_DECLARE_TR_FUNCTIONS(InstanceSummary)
};

QString versionLine(MinecraftInstance* instance)
{
    if (!instance)
        return {};
    if (auto it = s_versionLines.constFind(instance->id()); it != s_versionLines.constEnd())
        return *it;

    // Read the component list straight from mmc-pack.json: cheap, and works without downloaded metadata.
    QHash<QString, QString> components;
    QFile pack(instance->instanceRoot() + "/mmc-pack.json");
    if (pack.open(QIODevice::ReadOnly)) {
        const auto array = QJsonDocument::fromJson(pack.readAll()).object().value("components").toArray();
        for (const auto& value : array) {
            const auto component = value.toObject();
            components.insert(component.value("uid").toString(), component.value("version").toString());
        }
    }
    const QString minecraft = components.value("net.minecraft");

    static const std::pair<const char*, const char*> loaders[] = {
        { "net.neoforged", "NeoForge" },           { "net.minecraftforge", "Forge" },      { "net.fabricmc.fabric-loader", "Fabric" },
        { "org.quiltmc.quilt-loader", "Quilt" },   { "com.mumfrey.liteloader", "LiteLoader" },
    };
    QString loader = Strings::tr("Minecraft");
    for (const auto& [uid, name] : loaders) {
        if (!components.value(uid).isEmpty()) {
            loader = name;
            break;
        }
    }

    QString line = minecraft.isEmpty() ? loader : QString("%1 %2").arg(loader, minecraft);
    s_versionLines.insert(instance->id(), line);
    return line;
}

QString lastPlayedLine(MinecraftInstance* instance)
{
    if (!instance)
        return {};
    if (instance->isRunning())
        return Strings::tr("Playing now");
    const qint64 last = instance->lastLaunch();
    if (last <= 0)
        return Strings::tr("Never played");

    const qint64 seconds = QDateTime::fromMSecsSinceEpoch(last).secsTo(QDateTime::currentDateTime());
    if (seconds < 60)
        return Strings::tr("Played just now");
    // Separate singular strings so English reads naturally without a translation file.
    auto ago = [](qint64 n, const char* one, const char* many) {
        return n == 1 ? Strings::tr(one) : Strings::tr(many).arg(n);
    };
    if (seconds < 3600)
        return ago(seconds / 60, QT_TRANSLATE_NOOP("InstanceSummary", "Played a minute ago"), QT_TRANSLATE_NOOP("InstanceSummary", "Played %1 minutes ago"));
    if (seconds < 86400)
        return ago(seconds / 3600, QT_TRANSLATE_NOOP("InstanceSummary", "Played an hour ago"), QT_TRANSLATE_NOOP("InstanceSummary", "Played %1 hours ago"));
    if (seconds < 86400 * 30)
        return ago(seconds / 86400, QT_TRANSLATE_NOOP("InstanceSummary", "Played yesterday"), QT_TRANSLATE_NOOP("InstanceSummary", "Played %1 days ago"));
    if (seconds < 86400 * 365)
        return ago(seconds / (86400 * 30), QT_TRANSLATE_NOOP("InstanceSummary", "Played a month ago"), QT_TRANSLATE_NOOP("InstanceSummary", "Played %1 months ago"));
    return ago(seconds / (86400 * 365), QT_TRANSLATE_NOOP("InstanceSummary", "Played a year ago"), QT_TRANSLATE_NOOP("InstanceSummary", "Played %1 years ago"));
}

QString creatorLine(MinecraftInstance* instance)
{
    const QString author = ModpackAuthors::instance()->author(instance);
    return author.isEmpty() ? QString() : Strings::tr("by %1").arg(author);
}

QString playtimeLine(MinecraftInstance* instance)
{
    if (!instance || instance->totalTimePlayed() <= 0)
        return {};
    const bool noDays = APPLICATION->settings()->get("ShowGameTimeWithoutDays").toBool();
    return Strings::tr("%1 played").arg(Time::prettifyDuration(instance->totalTimePlayed(), noDays));
}

void invalidate(const QString& instanceId)
{
    if (instanceId.isEmpty())
        s_versionLines.clear();
    else
        s_versionLines.remove(instanceId);
}

}  // namespace InstanceSummary
