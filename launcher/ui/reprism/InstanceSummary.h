// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QString>

class MinecraftInstance;

/// Short human-readable facts about an instance, for cards and the details panel.
namespace InstanceSummary {
struct Components {
    QString loader;          ///< e.g. "Fabric"; empty for vanilla
    QString loaderIconKey;   ///< built-in icon for the loader, e.g. "fabricmc"; empty for vanilla
    QString minecraft;       ///< e.g. "1.21.4"
};
/// Mod loader and Minecraft version, read from the instance's mmc-pack.json.
Components components(MinecraftInstance* instance);
/// e.g. "Fabric 1.21.4", or "Minecraft 1.8.9" for vanilla.
QString versionLine(MinecraftInstance* instance);
/// Modpack version for managed packs (e.g. "2.9.3"), otherwise the Minecraft version.
QString versionChip(MinecraftInstance* instance);
/// e.g. "Played 3 hours ago", or "Never played".
QString lastPlayedLine(MinecraftInstance* instance);
/// e.g. "by Shivaxi" for modpacks with a known creator, otherwise an empty string.
QString creatorLine(MinecraftInstance* instance);
/// e.g. "42h 10m played", or an empty string when never played.
QString playtimeLine(MinecraftInstance* instance);
/// Forget cached facts, e.g. after an instance's components changed.
void invalidate(const QString& instanceId = {});
}  // namespace InstanceSummary
