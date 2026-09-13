#pragma once

#include <QString>

namespace AppVersion {

QString clientVersion();
int protocolVersion();
QString updaterExecutableName();
int compareVersions(const QString &left, const QString &right);

}
