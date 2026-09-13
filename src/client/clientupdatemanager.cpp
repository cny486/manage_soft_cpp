#include "clientupdatemanager.h"

#include "appversion.h"
#include "tcpappserviceclient.h"
#include "updateinfo.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>

namespace {
QString updateSummaryText(const ClientUpdateInfo &info)
{
    QStringList lines;
    lines.append(QStringLiteral("Current version: %1").arg(info.currentVersion));
    lines.append(QStringLiteral("Latest version: %1").arg(info.latestVersion));

    if (!info.publishedAt.trimmed().isEmpty()) {
        lines.append(QStringLiteral("Published at: %1").arg(info.publishedAt));
    }
    if (!info.title.trimmed().isEmpty()) {
        lines.append(QStringLiteral("Title: %1").arg(info.title));
    }

    if (!info.descriptionLines.isEmpty()) {
        lines.append(QString());
        lines.append(QStringLiteral("Release notes:"));
        for (const QString &line : info.descriptionLines) {
            lines.append(QStringLiteral("- %1").arg(line));
        }
    }

    return lines.join(QStringLiteral("\n"));
}

bool writeDownloadedPackage(const QString &filePath,
                            const QByteArray &content,
                            QString *errorMessage)
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not open update package for writing: %1").arg(file.errorString());
        }
        return false;
    }

    if (file.write(content) != content.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Update package write was incomplete.");
        }
        file.cancelWriting();
        return false;
    }

    if (!file.commit()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not save update package: %1").arg(file.errorString());
        }
        return false;
    }

    return true;
}

bool prepareUpdaterRuntime(const QString &sourceDirPath,
                           const QString &tempRoot,
                           QString *updaterCopyPath,
                           QString *errorMessage)
{
    const QString updaterFileName = AppVersion::updaterExecutableName();
    const QString updaterSourcePath = QDir(sourceDirPath).filePath(updaterFileName);
    if (!QFileInfo::exists(updaterSourcePath)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Updater executable not found: %1").arg(updaterSourcePath);
        }
        return false;
    }

    const QString targetUpdaterPath = QDir(tempRoot).filePath(updaterFileName);
    QFile::remove(targetUpdaterPath);
    if (!QFile::copy(updaterSourcePath, targetUpdaterPath)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not prepare the updater executable.");
        }
        return false;
    }

    const QDir sourceDir(sourceDirPath);
    const QFileInfoList runtimeFiles = sourceDir.entryInfoList(QStringList() << QStringLiteral("*.dll"),
                                                               QDir::Files | QDir::Readable);
    for (const QFileInfo &fileInfo : runtimeFiles) {
        const QString targetPath = QDir(tempRoot).filePath(fileInfo.fileName());
        QFile::remove(targetPath);
        if (!QFile::copy(fileInfo.absoluteFilePath(), targetPath)) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Could not prepare updater runtime file: %1").arg(fileInfo.fileName());
            }
            return false;
        }
    }

    if (updaterCopyPath != nullptr) {
        *updaterCopyPath = targetUpdaterPath;
    }
    return true;
}

ClientUpdateAction downloadAndLaunchUpdater(QWidget *parent,
                                            TcpAppServiceClient *client,
                                            const ClientUpdateInfo &info,
                                            QString *errorMessage)
{
    QByteArray packageContent;
    QString packageFileName;
    QString packageSha256;
    if (!client->downloadClientUpdatePackage(info.latestVersion,
                                             &packageFileName,
                                             &packageContent,
                                             &packageSha256,
                                             errorMessage)) {
        QMessageBox::critical(parent,
                              QStringLiteral("Update download failed"),
                              errorMessage != nullptr ? *errorMessage : QStringLiteral("Unknown error."));
        return ClientUpdateAction::Proceed;
    }

    if (!info.sha256.trimmed().isEmpty() && info.sha256.trimmed().compare(packageSha256, Qt::CaseInsensitive) != 0) {
        const QString message = QStringLiteral("Downloaded package SHA-256 does not match the manifest.");
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        QMessageBox::critical(parent, QStringLiteral("Update failed"), message);
        return ClientUpdateAction::Proceed;
    }

    const QString tempRoot = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                 .filePath(QStringLiteral("ManageSoftCppUpdater"));
    if (!QDir().mkpath(tempRoot)) {
        const QString message = QStringLiteral("Could not create the temporary update directory.");
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        QMessageBox::critical(parent, QStringLiteral("Update failed"), message);
        return ClientUpdateAction::Proceed;
    }

    const QString packagePath = QDir(tempRoot).filePath(packageFileName.trimmed().isEmpty()
                                                            ? QStringLiteral("ManageSoftCpp-update.zip")
                                                            : packageFileName.trimmed());
    if (!writeDownloadedPackage(packagePath, packageContent, errorMessage)) {
        QMessageBox::critical(parent,
                              QStringLiteral("Update failed"),
                              errorMessage != nullptr ? *errorMessage : QStringLiteral("Could not save the package."));
        return ClientUpdateAction::Proceed;
    }

    QString updaterCopyPath;
    if (!prepareUpdaterRuntime(QCoreApplication::applicationDirPath(), tempRoot, &updaterCopyPath, errorMessage)) {
        const QString message = errorMessage != nullptr && !errorMessage->trimmed().isEmpty()
                                    ? *errorMessage
                                    : QStringLiteral("Could not prepare the updater runtime.");
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        QMessageBox::critical(parent, QStringLiteral("Update failed"), message);
        return ClientUpdateAction::Proceed;
    }

    const QString launchExePath = QCoreApplication::applicationFilePath();
    const QStringList arguments = {
        QStringLiteral("--pid"), QString::number(QCoreApplication::applicationPid()),
        QStringLiteral("--package"), packagePath,
        QStringLiteral("--target-dir"), QCoreApplication::applicationDirPath(),
        QStringLiteral("--launch-exe"), launchExePath
    };

    if (!QProcess::startDetached(updaterCopyPath, arguments, tempRoot)) {
        const QString message = QStringLiteral("Could not launch the updater process.");
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        QMessageBox::critical(parent, QStringLiteral("Update failed"), message);
        return ClientUpdateAction::Proceed;
    }

    QMessageBox::information(parent,
                             QStringLiteral("Updating"),
                             QStringLiteral("The update has been downloaded. The application will now close and update itself."));
    return ClientUpdateAction::RelaunchingForUpdate;
}
}

ClientUpdateAction ClientUpdateManager::handlePostLoginUpdateCheck(QWidget *parent,
                                                                  TcpAppServiceClient *client,
                                                                  QString *errorMessage)
{
    if (client == nullptr) {
        return ClientUpdateAction::Proceed;
    }

    ClientUpdateInfo info;
    if (!client->checkForClientUpdate(AppVersion::clientVersion(), &info, errorMessage)) {
        return ClientUpdateAction::Proceed;
    }

    if (!info.hasUpdate) {
        return ClientUpdateAction::Proceed;
    }

    QMessageBox messageBox(parent);
    messageBox.setIcon(QMessageBox::Information);
    messageBox.setWindowTitle(info.mandatory ? QStringLiteral("Update required") : QStringLiteral("New version available"));
    messageBox.setText(info.mandatory
                           ? QStringLiteral("Your client version is no longer supported. Please update before continuing.")
                           : QStringLiteral("A newer client version is available on the server."));
    messageBox.setInformativeText(updateSummaryText(info));

    QPushButton *updateButton = messageBox.addButton(QStringLiteral("Update now"), QMessageBox::AcceptRole);
    if (info.mandatory) {
        messageBox.addButton(QStringLiteral("Exit"), QMessageBox::RejectRole);
    } else {
        messageBox.addButton(QStringLiteral("Later"), QMessageBox::RejectRole);
    }

    messageBox.exec();
    if (messageBox.clickedButton() == updateButton) {
        return downloadAndLaunchUpdater(parent, client, info, errorMessage);
    }

    return info.mandatory ? ClientUpdateAction::ExitApplication : ClientUpdateAction::Proceed;
}
