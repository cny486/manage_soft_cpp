#include "emailsettings.h"

#include <QSettings>

namespace {
QSettings sharedSettingsStore()
{
    return QSettings(QStringLiteral("ManageSoftCpp"), QStringLiteral("SharedSettings"));
}
}

EmailSettings loadEmailSettings()
{
    EmailSettings settings;
    QSettings store = sharedSettingsStore();

    settings.smtpHost = store.value(QStringLiteral("email/smtpHost"), settings.smtpHost).toString().trimmed();

    bool portOk = false;
    const int storedPort = store.value(QStringLiteral("email/smtpPort"), settings.smtpPort).toInt(&portOk);
    if (portOk && storedPort > 0 && storedPort <= 65535) {
        settings.smtpPort = storedPort;
    }

    settings.senderEmail = store.value(QStringLiteral("email/senderEmail"), settings.senderEmail).toString().trimmed();
    settings.senderName = store.value(QStringLiteral("email/senderName"), settings.senderName).toString().trimmed();
    settings.authUser = store.value(QStringLiteral("email/authUser"), settings.authUser).toString().trimmed();
    settings.authPassword = store.value(QStringLiteral("email/authPassword"), settings.authPassword).toString();

    bool timeoutOk = false;
    const int storedTimeout = store.value(QStringLiteral("email/timeoutMs"), settings.timeoutMs).toInt(&timeoutOk);
    if (timeoutOk && storedTimeout > 0) {
        settings.timeoutMs = storedTimeout;
    }

    if (settings.smtpHost.isEmpty()) {
        settings.smtpHost = qEnvironmentVariable("MANAGE_SOFT_SMTP_HOST").trimmed();
    }

    const QString portOverride = qEnvironmentVariable("MANAGE_SOFT_SMTP_PORT").trimmed();
    if (!portOverride.isEmpty()) {
        const int envPort = portOverride.toInt(&portOk);
        if (portOk && envPort > 0 && envPort <= 65535) {
            settings.smtpPort = envPort;
        }
    }

    if (settings.senderEmail.isEmpty()) {
        settings.senderEmail = qEnvironmentVariable("MANAGE_SOFT_SMTP_SENDER_EMAIL").trimmed();
    }
    if (settings.senderName.trimmed().isEmpty()) {
        settings.senderName = qEnvironmentVariable("MANAGE_SOFT_SMTP_SENDER_NAME").trimmed();
    }
    if (settings.authUser.isEmpty()) {
        settings.authUser = qEnvironmentVariable("MANAGE_SOFT_SMTP_USER").trimmed();
    }
    if (settings.authPassword.isEmpty()) {
        settings.authPassword = qEnvironmentVariable("MANAGE_SOFT_SMTP_PASSWORD");
    }

    const QString timeoutOverride = qEnvironmentVariable("MANAGE_SOFT_SMTP_TIMEOUT_MS").trimmed();
    if (!timeoutOverride.isEmpty()) {
        const int envTimeout = timeoutOverride.toInt(&timeoutOk);
        if (timeoutOk && envTimeout > 0) {
            settings.timeoutMs = envTimeout;
        }
    }

    if (settings.senderName.trimmed().isEmpty()) {
        settings.senderName = QStringLiteral("ManageSoftCpp");
    }

    return settings;
}

void saveEmailSettings(const EmailSettings &settings)
{
    QSettings store = sharedSettingsStore();
    store.setValue(QStringLiteral("email/smtpHost"), settings.smtpHost.trimmed());
    store.setValue(QStringLiteral("email/smtpPort"), settings.smtpPort);
    store.setValue(QStringLiteral("email/senderEmail"), settings.senderEmail.trimmed());
    store.setValue(QStringLiteral("email/senderName"), settings.senderName.trimmed());
    store.setValue(QStringLiteral("email/authUser"), settings.authUser.trimmed());
    store.setValue(QStringLiteral("email/authPassword"), settings.authPassword);
    store.setValue(QStringLiteral("email/timeoutMs"), settings.timeoutMs);
    store.sync();
}