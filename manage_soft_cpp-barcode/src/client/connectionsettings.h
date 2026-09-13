#pragma once

#include <QString>

struct ConnectionSettings {
    bool useLocalStorage = false;
    QString serverHost = QStringLiteral("111.229.149.41");
    quint16 serverPort = 45454;
    int timeoutMs = 10000;
};

ConnectionSettings loadConnectionSettings();
void saveConnectionSettings(const ConnectionSettings &settings);