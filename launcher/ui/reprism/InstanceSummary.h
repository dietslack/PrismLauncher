// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QString>

class MinecraftInstance;

/// Short human-readable facts about an instance, for cards and the details panel.
namespace InstanceSummary {
/// e.g. "Fabric 1.21.4", or "Minecraft 1.8.9" for vanilla.
QString versionLine(MinecraftInstance* instance);
/// e.g. "Played 3 hours ago", or "Never played".
QString lastPlayedLine(MinecraftInstance* instance);
/// e.g. "42h 10m played", or an empty string when never played.
QString playtimeLine(MinecraftInstance* instance);
/// Forget cached facts, e.g. after an instance's components changed.
void invalidate(const QString& instanceId = {});
}  // namespace InstanceSummary
