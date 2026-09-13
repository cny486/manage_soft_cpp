#pragma once

#include "updateinfo.h"

#include <QString>

class UpdateCatalogService {
public:
    struct ReleaseRecord {
        QString version;
        QString title;
        QString publishedAt;
        QString packageFile;
        QString sha256;
        QStringList descriptionLines;
        bool mandatory = false;
    };

    struct ManifestRecord {
        QString latestVersion;
        QString minimumSupportedVersion;
        QList<ReleaseRecord> releases;
    };

    bool checkForUpdate(const QString &currentVersion,
                        ClientUpdateInfo *info,
                        QString *errorMessage = nullptr) const;
    bool loadUpdatePackage(const QString &version,
                           QByteArray *content,
                           QString *fileName,
                           QString *sha256,
                           QString *errorMessage = nullptr) const;

private:
    QString manifestPath() const;
    bool loadManifest(ManifestRecord *manifest, QString *errorMessage) const;
};
