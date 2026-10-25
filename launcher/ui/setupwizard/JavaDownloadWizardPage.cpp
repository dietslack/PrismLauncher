// SPDX-License-Identifier: GPL-3.0-only
#include "JavaDownloadWizardPage.h"

#include <QCheckBox>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

#include "Application.h"
#include "BuildConfig.h"
#include "java/download/InstallMajorJavaTask.h"
#include "settings/SettingsObject.h"
#include "tasks/ConcurrentTask.h"
#include "ui/dialogs/ProgressDialog.h"

namespace {
// Major versions offered, newest first, and whether they are selected by default.
struct Offer {
    int major;
    bool selected;
};
const Offer s_offers[] = { { 25, true }, { 21, true }, { 17, true }, { 16, false }, { 8, false } };

QString coverage(int major)
{
    switch (major) {
        case 25:
            return QObject::tr("Minecraft 26.1 and newer");
        case 21:
            return QObject::tr("Minecraft 1.20.5 to 1.21.x");
        case 17:
            return QObject::tr("Minecraft 1.18 to 1.20.4");
        case 16:
            return QObject::tr("Minecraft 1.17");
        case 8:
            return QObject::tr("Minecraft 1.16.5 and older");
        default:
            return {};
    }
}
}  // namespace

JavaDownloadWizardPage::JavaDownloadWizardPage(QWidget* parent) : BaseWizardPage(parent)
{
    setObjectName(QStringLiteral("javaDownloadPage"));
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    m_intro = new QLabel(this);
    m_intro->setWordWrap(true);
    layout->addWidget(m_intro);
    layout->addSpacing(6);

    for (const auto& offer : s_offers) {
        auto* checkBox = new QCheckBox(this);
        checkBox->setChecked(offer.selected);
        layout->addWidget(checkBox);
        m_entries.append({ offer.major, checkBox });
    }

    layout->addSpacing(12);
    m_autoDownload = new QCheckBox(this);
    m_autoDownload->setChecked(APPLICATION->settings()->get("AutomaticJavaDownload").toBool());
    layout->addWidget(m_autoDownload);

    layout->addStretch();
    m_note = new QLabel(this);
    m_note->setWordWrap(true);
    m_note->setEnabled(false);  // rendered in the muted text color
    layout->addWidget(m_note);

    retranslate();
}

void JavaDownloadWizardPage::initializePage()
{
    for (auto& entry : m_entries) {
        const bool installed = Java::InstallMajorJavaTask::isInstalled(entry.major);
        entry.checkBox->setEnabled(!installed);
        if (installed)
            entry.checkBox->setChecked(true);
    }
    retranslate();
}

bool JavaDownloadWizardPage::validatePage()
{
    auto settings = APPLICATION->settings();
    settings->set("AutomaticJavaDownload", m_autoDownload->isChecked());
    settings->set("AutomaticJavaSwitch", true);
    settings->set("UserAskedAboutAutomaticJavaDownload", true);

    auto task = makeShared<ConcurrentTask>(tr("Download Java"), 2);
    QStringList failures;
    int count = 0;
    for (const auto& entry : m_entries) {
        if (!entry.checkBox->isEnabled() || !entry.checkBox->isChecked())
            continue;
        auto install = makeShared<Java::InstallMajorJavaTask>(entry.major);
        connect(install.get(), &Task::failed, this,
                [&failures, major = entry.major](const QString& reason) { failures << tr("Java %1: %2").arg(major).arg(reason); });
        task->addTask(install);
        count++;
    }
    if (count == 0)
        return true;

    ProgressDialog progress(this);
    progress.execWithTask(task.get());

    if (!failures.isEmpty()) {
        QMessageBox::warning(this, tr("Java download"),
                             tr("Some Java versions could not be downloaded. You can try again later from Settings > Java.\n\n%1")
                                 .arg(failures.join('\n')));
    }
    initializePage();
    return true;
}

void JavaDownloadWizardPage::retranslate()
{
    setTitle(tr("Java"));
    setSubTitle(tr("Download the Java versions Minecraft needs"));
    m_intro->setText(tr("Each Minecraft version needs a specific version of Java. %1 can download them for you now, "
                        "so your instances are ready to play right away.")
                         .arg(BuildConfig.LAUNCHER_DISPLAYNAME));
    for (auto& entry : m_entries) {
        QString text = tr("Java %1  —  %2").arg(entry.major).arg(coverage(entry.major));
        if (!entry.checkBox->isEnabled())
            text += "  " + tr("(installed)");
        entry.checkBox->setText(text);
    }
    m_autoDownload->setText(tr("Automatically download any other Java versions when they are needed"));
    m_note->setText(tr("Java is provided by Mojang and stored in the launcher's java folder. You can manage it later in Settings > Java."));
}
