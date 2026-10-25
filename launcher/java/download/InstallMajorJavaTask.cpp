// SPDX-License-Identifier: GPL-3.0-only
#include "InstallMajorJavaTask.h"

#include <QDir>
#include <QFileInfo>

#include "Application.h"
#include "FileSystem.h"
#include "QObjectPtr.h"
#include "SysInfo.h"
#include "java/JavaMetadata.h"
#include "java/JavaUtils.h"
#include "java/download/ArchiveDownloadTask.h"
#include "java/download/ManifestDownloadTask.h"
#include "java/download/SymlinkTask.h"
#include "meta/Index.h"
#include "meta/Version.h"
#include "meta/VersionList.h"
#include "net/Mode.h"
#include "tasks/SequentialTask.h"

namespace Java {

static const QString s_uid = QStringLiteral("net.minecraft.java");

InstallMajorJavaTask::InstallMajorJavaTask(int major) : m_major(major) {}

QString InstallMajorJavaTask::preferredRuntimeName(int major)
{
    switch (major) {
        case 8:
            return "jre-legacy";
        case 16:
            return "java-runtime-alpha";
        case 17:
            return "java-runtime-gamma";
        case 21:
            return "java-runtime-delta";
        case 25:
            return "java-runtime-epsilon";
        default:
            return {};
    }
}

bool InstallMajorJavaTask::isInstalled(int major)
{
    const auto name = preferredRuntimeName(major);
    if (name.isEmpty())
        return false;
    return QFileInfo::exists(QDir(APPLICATION->javaPath()).absoluteFilePath(FS::PathCombine(name, "bin", JavaUtils::javaExecutable)));
}

void InstallMajorJavaTask::watch(Task* task)
{
    connect(task, &Task::progress, this, &InstallMajorJavaTask::setProgress);
    connect(task, &Task::stepProgress, this, &InstallMajorJavaTask::propagateStepProgress);
    connect(task, &Task::details, this, &InstallMajorJavaTask::setDetails);
}

void InstallMajorJavaTask::executeTask()
{
    if (SysInfo::getSupportedJavaArchitecture().isEmpty()) {
        emitFailed(tr("Your system (%1-%2) is not supported by the Java downloader.").arg(SysInfo::currentSystem(), SysInfo::useQTForArch()));
        return;
    }
    setStatus(tr("Fetching Java %1 information…").arg(m_major));
    m_currentTask = APPLICATION->metadataIndex()->loadVersion(s_uid, QString("java%1").arg(m_major), Net::Mode::Online);
    connect(m_currentTask.get(), &Task::succeeded, this, &InstallMajorJavaTask::download);
    connect(m_currentTask.get(), &Task::failed, this, &InstallMajorJavaTask::emitFailed);
    connect(m_currentTask.get(), &Task::aborted, this, &InstallMajorJavaTask::emitAborted);
    watch(m_currentTask.get());
    if (!m_currentTask->isRunning())
        m_currentTask->start();
}

void InstallMajorJavaTask::download()
{
    auto versionList = APPLICATION->metadataIndex()->get(s_uid);
    auto version = versionList ? versionList->getVersion(QString("java%1").arg(m_major)) : nullptr;
    if (!version || !version->data()) {
        emitFailed(tr("No information is available for Java %1.").arg(m_major));
        return;
    }

    // Prefer the runtime Minecraft itself asks for, otherwise any non-snapshot Mojang runtime for this system.
    const auto arch = SysInfo::getSupportedJavaArchitecture();
    const auto preferred = preferredRuntimeName(m_major);
    MetadataPtr runtime;
    for (const auto& candidate : version->data()->runtimes) {
        if (candidate->runtimeOS != arch || candidate->vendor != "mojang" || candidate->m_name.endsWith("-snapshot"))
            continue;
        if (candidate->m_name == preferred) {
            runtime = candidate;
            break;
        }
        if (!runtime)
            runtime = candidate;
    }
    if (!runtime) {
        emitFailed(tr("Java %1 is not available for your system (%2).").arg(m_major).arg(arch));
        return;
    }

    const auto finalPath = QDir(APPLICATION->javaPath()).absoluteFilePath(runtime->m_name);
    switch (runtime->downloadType) {
        case DownloadType::Manifest:
            m_currentTask = makeShared<ManifestDownloadTask>(runtime->url, finalPath, runtime->checksumType, runtime->checksumHash);
            break;
        case DownloadType::Archive:
            m_currentTask = makeShared<ArchiveDownloadTask>(runtime->url, finalPath, runtime->checksumType, runtime->checksumHash);
            break;
        case DownloadType::Unknown:
            emitFailed(tr("Could not determine Java download type!"));
            return;
    }
#if defined(Q_OS_MACOS)
    auto seq = makeShared<SequentialTask>(tr("Install Java"));
    seq->addTask(m_currentTask);
    seq->addTask(makeShared<SymlinkTask>(finalPath));
    m_currentTask = seq;
#endif

    setStatus(tr("Downloading Java %1 (%2)…").arg(m_major).arg(runtime->version.toString()));
    auto deletePath = [finalPath] { FS::deletePath(finalPath); };
    connect(m_currentTask.get(), &Task::succeeded, this, &InstallMajorJavaTask::emitSucceeded);
    connect(m_currentTask.get(), &Task::failed, this, [this, deletePath](QString reason) {
        deletePath();
        emitFailed(reason);
    });
    connect(m_currentTask.get(), &Task::aborted, this, [this, deletePath] {
        deletePath();
        emitAborted();
    });
    watch(m_currentTask.get());
    m_currentTask->start();
}

bool InstallMajorJavaTask::abort()
{
    if (m_currentTask && m_currentTask->canAbort())
        return m_currentTask->abort();
    return Task::abort();
}

}  // namespace Java
