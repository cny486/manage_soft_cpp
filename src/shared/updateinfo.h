#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

struct ClientUpdateInfo {
    bool hasUpdate = false;
    bool mandatory = false;
    QString currentVersion;
    QString latestVersion;
    QString minimumSupportedVersion;
    QString title;
    QString publishedAt;
    QString packageFileName;
    QString sha256;
    QStringList descriptionLines;
};

QJsonObject clientUpdateInfoToJson(const ClientUpdateInfo &info);
ClientUpdateInfo clientUpdateInfoFromJson(const QJsonObject &object);
