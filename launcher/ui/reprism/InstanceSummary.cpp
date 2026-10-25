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

static QHash<QString, Components> s_components;

// Lets lupdate find the strings below under the "InstanceSummary" context.
struct Strings {
    Q_DECLARE_TR_FUNCTIONS(InstanceSummary)
};

Components components(MinecraftInstance* instance)
{
    if (!instance)
        return {};
    if (auto it = s_components.constFind(instance->id()); it != s_components.constEnd())
        return *it;

    // Read the component list straight from mmc-pack.json: cheap, and works without downloaded metadata.
    QHash<QString, QString> versions;
    QFile pack(instance->instanceRoot() + "/mmc-pack.json");
    if (pack.open(QIODevice::ReadOnly)) {
        const auto array = QJsonDocument::fromJson(pack.readAll()).object().value("components").toArray();
        for (const auto& value : array) {
            const auto component = value.toObject();
            versions.insert(component.value("uid").toString(), component.value("version").toString());
        }
    }

    static const struct {
        const char* uid;
        const char* name;
        const char* icon;
    } loaders[] = {
        { "net.neoforged", "NeoForge", "neoforged" },         { "net.minecraftforge", "Forge", "forge" },
        { "net.fabricmc.fabric-loader", "Fabric", "fabricmc" }, { "org.quiltmc.quilt-loader", "Quilt", "quiltmc" },
        { "com.mumfrey.liteloader", "LiteLoader", "liteloader" },
    };
    Components result;
    result.minecraft = versions.value("net.minecraft");
    for (const auto& loader : loaders) {
        if (!versions.value(loader.uid).isEmpty()) {
            result.loader = loader.name;
            result.loaderIconKey = loader.icon;
            break;
        }
    }
    s_components.insert(instance->id(), result);
    return result;
}

QString versionLine(MinecraftInstance* instance)
{
    const Components c = components(instance);
    const QString loader = c.loader.isEmpty() ? Strings::tr("Minecraft") : c.loader;
    return c.minecraft.isEmpty() ? loader : QString("%1 %2").arg(loader, c.minecraft);
}

QString versionChip(MinecraftInstance* instance)
{
    if (instance && instance->isManagedPack() && !instance->getManagedPackVersionName().isEmpty())
        return instance->getManagedPackVersionName();
    return components(instance).minecraft;
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
        s_components.clear();
    else
        s_components.remove(instanceId);
}

}  // namespace InstanceSummary
