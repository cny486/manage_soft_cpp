#pragma once

#include "emailsettings.h"

#include <QString>

class SmtpEmailClient {
public:
    static bool sendPlainTextMail(const EmailSettings &settings,
                                  const QString &toEmail,
                                  const QString &subject,
                                  const QString &body,
                                  QString *errorMessage = nullptr);
};