#include "appversion.h"

#include <QStringList>

namespace {
QList<int> versionParts(const QString &version)
{
    QList<int> parts;
    const QStringList tokens = version.trimmed().split(QChar('.'), Qt::SkipEmptyParts);
    parts.reserve(tokens.size());

    for (const QString &token : tokens) {
        bool ok = false;
        const int value = token.toInt(&ok);
        parts.append(ok ? value : 0);
    }

    return parts;
}
}

QString AppVersion::clientVersion()
{
    return QStringLiteral("1.0.8");
}

int AppVersion::protocolVersion()
{
    return 1;
}

QString AppVersion::updaterExecutableName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("ManageSoftUpdater.exe");
#else
    return QStringLiteral("ManageSoftUpdater");
#endif
}

int AppVersion::compareVersions(const QString &left, const QString &right)
{
    const QList<int> leftParts = versionParts(left);
    const QList<int> rightParts = versionParts(right);
    const int maxCount = qMax(leftParts.size(), rightParts.size());

    for (int index = 0; index < maxCount; ++index) {
        const int leftValue = index < leftParts.size() ? leftParts.at(index) : 0;
        const int rightValue = index < rightParts.size() ? rightParts.at(index) : 0;
        if (leftValue < rightValue) {
            return -1;
        }
        if (leftValue > rightValue) {
            return 1;
        }
    }

    return 0;
}
