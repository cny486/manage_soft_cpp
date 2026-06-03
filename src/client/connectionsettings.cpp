#include "connectionsettings.h"

#include <QSettings>

namespace {
const QLatin1String kUseLocalStorageKey("connection/useLocalStorage");
const QLatin1String kServerHostKey("connection/serverHost");
const QLatin1String kServerPortKey("connection/serverPort");
const QLatin1String kTimeoutMsKey("connection/timeoutMs");
const QLatin1String kLegacyDefaultServerHost("127.0.0.1");
const QLatin1String kCurrentDefaultServerHost("111.229.149.41");
}

ConnectionSettings loadConnectionSettings()
{
    QSettings settings;

    ConnectionSettings result;
    result.useLocalStorage = settings.value(kUseLocalStorageKey, result.useLocalStorage).toBool();
    result.serverHost = settings.value(kServerHostKey, result.serverHost).toString().trimmed();

    bool portOk = false;
    const int portValue = settings.value(kServerPortKey, result.serverPort).toInt(&portOk);
    if (portOk && portValue > 0 && portValue <= 65535) {
        result.serverPort = static_cast<quint16>(portValue);
    }

    bool timeoutOk = false;
    const int timeoutValue = settings.value(kTimeoutMsKey, result.timeoutMs).toInt(&timeoutOk);
    if (timeoutOk && timeoutValue > 0) {
        result.timeoutMs = timeoutValue;
    }

    if (result.serverHost.isEmpty()) {
        result.serverHost = kCurrentDefaultServerHost;
        settings.setValue(kServerHostKey, result.serverHost);
        settings.sync();
        return result;
    }

    if (!result.useLocalStorage && result.serverHost == kLegacyDefaultServerHost) {
        result.serverHost = kCurrentDefaultServerHost;
        settings.setValue(kServerHostKey, result.serverHost);
        settings.sync();
    }

    return result;
}

void saveConnectionSettings(const ConnectionSettings &settings)
{
    QSettings store;
    store.setValue(kUseLocalStorageKey, settings.useLocalStorage);
    store.setValue(kServerHostKey, settings.serverHost.trimmed());
    store.setValue(kServerPortKey, settings.serverPort);
    store.setValue(kTimeoutMsKey, settings.timeoutMs);
    store.sync();
}