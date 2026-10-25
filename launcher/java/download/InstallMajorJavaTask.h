// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "tasks/Task.h"

namespace Java {

/// Downloads Mojang's Java runtime for one major version (e.g. 21) into the launcher's Java folder,
/// using the same runtime names the automatic Java installer looks for when launching an instance.
class InstallMajorJavaTask : public Task {
    Q_OBJECT
   public:
    explicit InstallMajorJavaTask(int major);
    ~InstallMajorJavaTask() override = default;

    bool canAbort() const override { return m_currentTask ? m_currentTask->canAbort() : false; }
    bool abort() override;

    /// Mojang runtime name preferred for a major version, or an empty string if unknown.
    static QString preferredRuntimeName(int major);
    /// Whether the preferred runtime for this major version is already in the launcher's Java folder.
    static bool isInstalled(int major);

   protected:
    void executeTask() override;

   private:
    void download();
    void watch(Task* task);

    int m_major;
    Task::Ptr m_currentTask;
};

}  // namespace Java
