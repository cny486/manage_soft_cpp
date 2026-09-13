#include "updateinfo.h"

#include <QJsonArray>

QJsonObject clientUpdateInfoToJson(const ClientUpdateInfo &info)
{
    return {
        {QStringLiteral("hasUpdate"), info.hasUpdate},
        {QStringLiteral("mandatory"), info.mandatory},
        {QStringLiteral("currentVersion"), info.currentVersion},
        {QStringLiteral("latestVersion"), info.latestVersion},
        {QStringLiteral("minimumSupportedVersion"), info.minimumSupportedVersion},
        {QStringLiteral("title"), info.title},
        {QStringLiteral("publishedAt"), info.publishedAt},
        {QStringLiteral("packageFileName"), info.packageFileName},
        {QStringLiteral("sha256"), info.sha256},
        {QStringLiteral("description"), QJsonArray::fromStringList(info.descriptionLines)}
    };
}

ClientUpdateInfo clientUpdateInfoFromJson(const QJsonObject &object)
{
    ClientUpdateInfo info;
    info.hasUpdate = object.value(QStringLiteral("hasUpdate")).toBool();
    info.mandatory = object.value(QStringLiteral("mandatory")).toBool();
    info.currentVersion = object.value(QStringLiteral("currentVersion")).toString().trimmed();
    info.latestVersion = object.value(QStringLiteral("latestVersion")).toString().trimmed();
    info.minimumSupportedVersion = object.value(QStringLiteral("minimumSupportedVersion")).toString().trimmed();
    info.title = object.value(QStringLiteral("title")).toString().trimmed();
    info.publishedAt = object.value(QStringLiteral("publishedAt")).toString().trimmed();
    info.packageFileName = object.value(QStringLiteral("packageFileName")).toString().trimmed();
    info.sha256 = object.value(QStringLiteral("sha256")).toString().trimmed();

    const QJsonArray description = object.value(QStringLiteral("description")).toArray();
    for (const QJsonValue &value : description) {
        const QString line = value.toString().trimmed();
        if (!line.isEmpty()) {
            info.descriptionLines.append(line);
        }
    }

    return info;
}
