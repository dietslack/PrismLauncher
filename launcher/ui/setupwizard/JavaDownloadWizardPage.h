// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QList>

#include "BaseWizardPage.h"

class QCheckBox;
class QLabel;

/// Setup wizard page offering to download Java runtimes up front, and to enable automatic Java downloads.
class JavaDownloadWizardPage : public BaseWizardPage {
    Q_OBJECT
   public:
    explicit JavaDownloadWizardPage(QWidget* parent = nullptr);
    ~JavaDownloadWizardPage() override = default;

    void initializePage() override;
    bool validatePage() override;

   protected:
    void retranslate() override;

   private:
    struct Entry {
        int major;
        QCheckBox* checkBox;
    };

    QLabel* m_intro = nullptr;
    QList<Entry> m_entries;
    QCheckBox* m_autoDownload = nullptr;
    QLabel* m_note = nullptr;
};
