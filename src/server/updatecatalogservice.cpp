#include "updatecatalogservice.h"

#include "appversion.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
QString sha256Hex(const QByteArray &content)
{
    return QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex());
}

const UpdateCatalogService::ReleaseRecord *findRelease(const QList<UpdateCatalogService::ReleaseRecord> &releases,
                                                       const QString &version)
{
    for (const UpdateCatalogService::ReleaseRecord &release : releases) {
        if (release.version.trimmed() == version.trimmed()) {
            return &release;
        }
    }

    return nullptr;
}
}

bool UpdateCatalogService::checkForUpdate(const QString &currentVersion,
                                          ClientUpdateInfo *info,
                                          QString *errorMessage) const
{
    if (info == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Update result output buffer is invalid.");
        }
        return false;
    }

    ManifestRecord manifest;
    if (!loadManifest(&manifest, errorMessage)) {
        return false;
    }

    info->currentVersion = currentVersion.trimmed();
    info->latestVersion = manifest.latestVersion;
    info->minimumSupportedVersion = manifest.minimumSupportedVersion;
    info->hasUpdate = AppVersion::compareVersions(info->currentVersion, info->latestVersion) < 0;

    const ReleaseRecord *latestRelease = findRelease(manifest.releases, manifest.latestVersion);
    if (latestRelease == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The update manifest does not contain a record for the latest version.");
        }
        return false;
    }

    info->title = latestRelease->title;
    info->publishedAt = latestRelease->publishedAt;
    info->packageFileName = QFileInfo(latestRelease->packageFile).fileName();
    info->sha256 = latestRelease->sha256;
    info->descriptionLines = latestRelease->descriptionLines;
    info->mandatory = latestRelease->mandatory
                      || AppVersion::compareVersions(info->currentVersion, info->minimumSupportedVersion) < 0;

    return true;
}

bool UpdateCatalogService::loadUpdatePackage(const QString &version,
                                             QByteArray *content,
                                             QString *fileName,
                                             QString *sha256,
                                             QString *errorMessage) const
{
    ManifestRecord manifest;
    if (!loadManifest(&manifest, errorMessage)) {
        return false;
    }

    const ReleaseRecord *release = findRelease(manifest.releases, version);
    if (release == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The requested update version was not found.");
        }
        return false;
    }

    QFile file(release->packageFile);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not read the update package: %1").arg(file.errorString());
        }
        return false;
    }

    const QByteArray bytes = file.readAll();
    const QString computedSha256 = sha256Hex(bytes);
    const QString expectedSha256 = release->sha256.trimmed().toLower();
    if (!expectedSha256.isEmpty() && expectedSha256 != computedSha256) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The update package SHA-256 does not match the manifest.");
        }
        return false;
    }

    if (content != nullptr) {
        *content = bytes;
    }
    if (fileName != nullptr) {
        *fileName = QFileInfo(file.fileName()).fileName();
    }
    if (sha256 != nullptr) {
        *sha256 = computedSha256;
    }

    return true;
}

QString UpdateCatalogService::manifestPath() const
{
    const QString overridePath = qEnvironmentVariable("MANAGE_SOFT_UPDATE_MANIFEST_PATH").trimmed();
    if (!overridePath.isEmpty()) {
        return overridePath;
    }

    return QCoreApplication::applicationDirPath() + QStringLiteral("/update_manifest.json");
}

bool UpdateCatalogService::loadManifest(ManifestRecord *manifest, QString *errorMessage) const
{
    if (manifest == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Update manifest output buffer is invalid.");
        }
        return false;
    }

    QFile file(manifestPath());
    if (!file.exists()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Update manifest file not found: %1").arg(QFileInfo(file).absoluteFilePath());
        }
        return false;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Could not open update manifest: %1").arg(file.errorString());
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Update manifest JSON is invalid.");
        }
        return false;
    }

    const QJsonObject root = doc.object();
    ManifestRecord loadedManifest;
    loadedManifest.latestVersion = root.value(QStringLiteral("latestVersion")).toString().trimmed();
    loadedManifest.minimumSupportedVersion = root.value(QStringLiteral("minimumSupportedVersion")).toString().trimmed();

    const QJsonArray releases = root.value(QStringLiteral("releases")).toArray();
    const QFileInfo manifestFileInfo(file.fileName());
    for (const QJsonValue &value : releases) {
        const QJsonObject object = value.toObject();
        ReleaseRecord record;
        record.version = object.value(QStringLiteral("version")).toString().trimmed();
        record.title = object.value(QStringLiteral("title")).toString().trimmed();
        record.publishedAt = object.value(QStringLiteral("publishedAt")).toString().trimmed();
        record.packageFile = object.value(QStringLiteral("packageFile")).toString().trimmed();
        record.sha256 = object.value(QStringLiteral("sha256")).toString().trimmed().toLower();
        record.mandatory = object.value(QStringLiteral("mandatory")).toBool();

        const QJsonArray description = object.value(QStringLiteral("description")).toArray();
        for (const QJsonValue &lineValue : description) {
            const QString line = lineValue.toString().trimmed();
            if (!line.isEmpty()) {
                record.descriptionLines.append(line);
            }
        }

        if (record.version.isEmpty() || record.packageFile.isEmpty()) {
            continue;
        }

        const QFileInfo packageInfo(record.packageFile);
        if (packageInfo.isRelative()) {
            record.packageFile = manifestFileInfo.dir().filePath(record.packageFile);
        }

        loadedManifest.releases.append(record);
    }

    if (loadedManifest.latestVersion.isEmpty()) {
        for (const ReleaseRecord &release : loadedManifest.releases) {
            if (loadedManifest.latestVersion.isEmpty()
                || AppVersion::compareVersions(release.version, loadedManifest.latestVersion) > 0) {
                loadedManifest.latestVersion = release.version;
            }
        }
    }

    if (loadedManifest.minimumSupportedVersion.isEmpty()) {
        loadedManifest.minimumSupportedVersion = loadedManifest.latestVersion;
    }

    if (loadedManifest.latestVersion.isEmpty() || loadedManifest.releases.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The update manifest does not contain any usable release entries.");
        }
        return false;
    }

    *manifest = loadedManifest;
    return true;
}
