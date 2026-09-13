#include "aiapisettings.h"

#include <QSettings>

namespace {
QSettings sharedSettingsStore()
{
    return QSettings(QStringLiteral("ManageSoftCpp"), QStringLiteral("SharedSettings"));
}
}

AiApiSettings loadAiApiSettings()
{
    AiApiSettings settings;
    QSettings store = sharedSettingsStore();

    settings.apiUrl = store.value(QStringLiteral("ai/apiUrl")).toString().trimmed();
    settings.apiKey = store.value(QStringLiteral("ai/apiKey")).toString().trimmed();
    settings.model = store.value(QStringLiteral("ai/model")).toString().trimmed();

    bool timeoutOk = false;
    const int storedTimeout = store.value(QStringLiteral("ai/timeoutMs"), settings.timeoutMs).toInt(&timeoutOk);
    if (timeoutOk && storedTimeout > 0) {
        settings.timeoutMs = storedTimeout;
    }

    if (settings.apiUrl.isEmpty()) {
        settings.apiUrl = qEnvironmentVariable("MANAGE_SOFT_AI_API_URL").trimmed();
    }
    if (settings.apiKey.isEmpty()) {
        settings.apiKey = qEnvironmentVariable("MANAGE_SOFT_AI_API_KEY").trimmed();
    }
    if (settings.model.isEmpty()) {
        settings.model = qEnvironmentVariable("MANAGE_SOFT_AI_MODEL").trimmed();
    }

    const QString timeoutOverride = qEnvironmentVariable("MANAGE_SOFT_AI_TIMEOUT_MS").trimmed();
    if (!timeoutOverride.isEmpty()) {
        const int envTimeout = timeoutOverride.toInt(&timeoutOk);
        if (timeoutOk && envTimeout > 0) {
            settings.timeoutMs = envTimeout;
        }
    }

    return settings;
}

void saveAiApiSettings(const AiApiSettings &settings)
{
    QSettings store = sharedSettingsStore();
    store.setValue(QStringLiteral("ai/apiUrl"), settings.apiUrl.trimmed());
    store.setValue(QStringLiteral("ai/apiKey"), settings.apiKey.trimmed());
    store.setValue(QStringLiteral("ai/model"), settings.model.trimmed());
    store.setValue(QStringLiteral("ai/timeoutMs"), settings.timeoutMs);
    store.sync();
}