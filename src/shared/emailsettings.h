#pragma once

#include <QString>

struct EmailSettings {
    QString smtpHost = QStringLiteral("smtp.163.com");
    int smtpPort = 465;
    QString senderEmail = QStringLiteral("cug_sly_manage@163.com");
    QString senderName = QStringLiteral("ManageSoftCpp");
    QString authUser = QStringLiteral("cug_sly_manage@163.com");
    QString authPassword = QStringLiteral("KUSqZufDyuQv8rww");
    int timeoutMs = 30000;
};

EmailSettings loadEmailSettings();
void saveEmailSettings(const EmailSettings &settings);