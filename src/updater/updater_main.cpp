#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
bool waitForProcessExit(qint64 processId, int timeoutMs)
{
#ifdef Q_OS_WIN
    HANDLE processHandle = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(processId));
    if (processHandle == nullptr) {
        return true;
    }

    const DWORD waitResult = WaitForSingleObject(processHandle, static_cast<DWORD>(timeoutMs));
    CloseHandle(processHandle);
    return waitResult == WAIT_OBJECT_0 || waitResult == WAIT_FAILED;
#else
    Q_UNUSED(processId);
    QThread::sleep(2);
    return true;
#endif
}

bool expandArchive(const QString &archivePath, const QString &destinationDir, QString *errorMessage)
{
#ifdef Q_OS_WIN
    QDir().mkpath(destinationDir);
    const QString escapedArchivePath = QDir::toNativeSeparators(archivePath).replace(QChar('\''), QStringLiteral("''"));
    const QString escapedDestinationDir = QDir::toNativeSeparators(destinationDir).replace(QChar('\''), QStringLiteral("''"));
    const QString command = QStringLiteral(
        "Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
                                .arg(escapedArchivePath, escapedDestinationDir);

    QProcess process;
    process.start(QStringLiteral("powershell"),
                  {QStringLiteral("-NoProfile"),
                   QStringLiteral("-ExecutionPolicy"),
                   QStringLiteral("Bypass"),
                   QStringLiteral("-Command"),
                   command});
    if (!process.waitForFinished(120000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Expand-Archive failed: %1")
                                .arg(QString::fromLocal8Bit(process.readAllStandardError()).trimmed());
        }
        return false;
    }

    return true;
#else
    Q_UNUSED(archivePath);
    Q_UNUSED(destinationDir);
    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("Automatic update is not supported on this platform.");
    }
    return false;
#endif
}

QString extractedPayloadRoot(const QString &directoryPath)
{
    QDir dir(directoryPath);
    const QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    if (entries.size() == 1 && entries.constFirst().isDir()) {
        return entries.constFirst().absoluteFilePath();
    }

    return directoryPath;
}

bool copyDirectoryContents(const QString &sourceDirPath, const QString &targetDirPath, QString *errorMessage)
{
    QDir sourceDir(sourceDirPath);
    if (!sourceDir.exists()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Extracted update package directory does not exist.");
        }
        return false;
    }

    if (!QDir().mkpath(targetDirPath)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not create target directory.");
        }
        return false;
    }

    QDirIterator iterator(sourceDirPath, QDir::NoDotAndDotDot | QDir::AllEntries, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        const QFileInfo entryInfo = iterator.fileInfo();
        const QString relativePath = sourceDir.relativeFilePath(entryInfo.absoluteFilePath());
        const QString targetPath = QDir(targetDirPath).filePath(relativePath);

        if (entryInfo.isDir()) {
            if (!QDir().mkpath(targetPath)) {
                if (errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Could not create directory: %1").arg(targetPath);
                }
                return false;
            }
            continue;
        }

        QDir().mkpath(QFileInfo(targetPath).absolutePath());
        QFile::remove(targetPath);
        if (!QFile::copy(entryInfo.absoluteFilePath(), targetPath)) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Could not replace file: %1").arg(targetPath);
            }
            return false;
        }
    }

    return true;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ManageSoftUpdater"));

    QCommandLineParser parser;
    parser.addHelpOption();

    QCommandLineOption pidOption(QStringList() << QStringLiteral("pid"),
                                 QStringLiteral("Process id to wait for."),
                                 QStringLiteral("pid"));
    QCommandLineOption packageOption(QStringList() << QStringLiteral("package"),
                                     QStringLiteral("Update package zip path."),
                                     QStringLiteral("path"));
    QCommandLineOption targetDirOption(QStringList() << QStringLiteral("target-dir"),
                                       QStringLiteral("Client install directory."),
                                       QStringLiteral("path"));
    QCommandLineOption launchExeOption(QStringList() << QStringLiteral("launch-exe"),
                                       QStringLiteral("Executable path to relaunch."),
                                       QStringLiteral("path"));

    parser.addOption(pidOption);
    parser.addOption(packageOption);
    parser.addOption(targetDirOption);
    parser.addOption(launchExeOption);
    parser.process(app);

    bool pidOk = false;
    const qint64 processId = parser.value(pidOption).toLongLong(&pidOk);
    const QString packagePath = parser.value(packageOption).trimmed();
    const QString targetDirPath = parser.value(targetDirOption).trimmed();
    const QString launchExePath = parser.value(launchExeOption).trimmed();

    if (!pidOk || processId <= 0 || packagePath.isEmpty() || targetDirPath.isEmpty() || launchExePath.isEmpty()) {
        return 1;
    }

    if (!waitForProcessExit(processId, 120000)) {
        return 2;
    }

    const QString extractDir = QDir(QFileInfo(packagePath).absolutePath())
                                   .filePath(QStringLiteral("payload"));
    QDir(extractDir).removeRecursively();

    QString errorMessage;
    if (!expandArchive(packagePath, extractDir, &errorMessage)) {
        return 3;
    }

    const QString payloadRoot = extractedPayloadRoot(extractDir);
    if (!copyDirectoryContents(payloadRoot, targetDirPath, &errorMessage)) {
        return 4;
    }

    if (!QProcess::startDetached(launchExePath, {}, QFileInfo(launchExePath).absolutePath())) {
        return 5;
    }

    return 0;
}
