#include "smtpemailclient.h"

#include <QRegularExpression>
#include <QSslSocket>

namespace {
QString normalizeResponse(const QByteArray &data)
{
    return QString::fromUtf8(data).replace(QStringLiteral("\r"), QString());
}

bool waitForCompleteResponse(QSslSocket *socket,
                             int timeoutMs,
                             QByteArray *responseData,
                             QString *errorMessage)
{
    if (socket == nullptr || responseData == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("SMTP 响应缓冲区无效。");
        }
        return false;
    }

    static const QRegularExpression responsePattern(QStringLiteral("^([0-9]{3})([ -]).*$"));

    while (true) {
        while (socket->canReadLine()) {
            const QByteArray line = socket->readLine();
            responseData->append(line);
            const QString textLine = QString::fromUtf8(line).trimmed();
            const QRegularExpressionMatch match = responsePattern.match(textLine);
            if (match.hasMatch() && match.captured(2) == QStringLiteral(" ")) {
                return true;
            }
        }

        if (!socket->waitForReadyRead(timeoutMs)) {
            if (errorMessage != nullptr) {
                const QString socketError = socket->errorString().trimmed();
                *errorMessage = socketError.isEmpty()
                                    ? QStringLiteral("等待 SMTP 响应超时。")
                                    : QStringLiteral("等待 SMTP 响应失败：%1").arg(socketError);
            }
            return false;
        }
    }
}

bool expectResponse(QSslSocket *socket,
                    int timeoutMs,
                    const QList<int> &acceptedCodes,
                    QString *errorMessage)
{
    QByteArray responseData;
    if (!waitForCompleteResponse(socket, timeoutMs, &responseData, errorMessage)) {
        return false;
    }

    const QString responseText = normalizeResponse(responseData).trimmed();
    bool codeOk = false;
    const int responseCode = responseText.left(3).toInt(&codeOk);
    if (codeOk && acceptedCodes.contains(responseCode)) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("SMTP 返回异常：%1").arg(responseText);
    }
    return false;
}

bool writeCommand(QSslSocket *socket,
                  const QByteArray &command,
                  int timeoutMs,
                  QString *errorMessage)
{
    if (socket->write(command) != command.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("发送 SMTP 指令失败：%1").arg(socket->errorString());
        }
        return false;
    }

    if (!socket->waitForBytesWritten(timeoutMs)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("等待 SMTP 指令写出失败：%1").arg(socket->errorString());
        }
        return false;
    }
    return true;
}

QString formatAddress(const QString &address)
{
    return QStringLiteral("<%1>").arg(address.trimmed());
}
}

bool SmtpEmailClient::sendPlainTextMail(const EmailSettings &settings,
                                        const QString &toEmail,
                                        const QString &subject,
                                        const QString &body,
                                        QString *errorMessage)
{
    if (settings.smtpHost.trimmed().isEmpty()
        || settings.senderEmail.trimmed().isEmpty()
        || settings.authUser.trimmed().isEmpty()
        || settings.authPassword.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("未配置 SMTP 参数。请在连接设置中填写邮件验证码设置，或配置 MANAGE_SOFT_SMTP_* 环境变量。");
        }
        return false;
    }

    QSslSocket socket;
    socket.connectToHostEncrypted(settings.smtpHost.trimmed(), static_cast<quint16>(settings.smtpPort));
    if (!socket.waitForEncrypted(settings.timeoutMs)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("连接 SMTP 服务器失败：%1").arg(socket.errorString());
        }
        return false;
    }

    if (!expectResponse(&socket, settings.timeoutMs, {220}, errorMessage)) {
        return false;
    }

    if (!writeCommand(&socket, QByteArray("EHLO managesoftcpp\r\n"), settings.timeoutMs, errorMessage)
        || !expectResponse(&socket, settings.timeoutMs, {250}, errorMessage)
        || !writeCommand(&socket, QByteArray("AUTH LOGIN\r\n"), settings.timeoutMs, errorMessage)
        || !expectResponse(&socket, settings.timeoutMs, {334}, errorMessage)
        || !writeCommand(&socket, settings.authUser.toUtf8().toBase64() + QByteArray("\r\n"), settings.timeoutMs, errorMessage)
        || !expectResponse(&socket, settings.timeoutMs, {334}, errorMessage)
        || !writeCommand(&socket, settings.authPassword.toUtf8().toBase64() + QByteArray("\r\n"), settings.timeoutMs, errorMessage)
        || !expectResponse(&socket, settings.timeoutMs, {235}, errorMessage)
        || !writeCommand(&socket, QStringLiteral("MAIL FROM:%1\r\n").arg(formatAddress(settings.senderEmail)).toUtf8(), settings.timeoutMs, errorMessage)
        || !expectResponse(&socket, settings.timeoutMs, {250}, errorMessage)
        || !writeCommand(&socket, QStringLiteral("RCPT TO:%1\r\n").arg(formatAddress(toEmail)).toUtf8(), settings.timeoutMs, errorMessage)
        || !expectResponse(&socket, settings.timeoutMs, {250, 251}, errorMessage)
        || !writeCommand(&socket, QByteArray("DATA\r\n"), settings.timeoutMs, errorMessage)
        || !expectResponse(&socket, settings.timeoutMs, {354}, errorMessage)) {
        return false;
    }

    const QByteArray message = QStringLiteral(
                                   "From: %1 %2\r\n"
                                   "To: %3\r\n"
                                   "Subject: %4\r\n"
                                   "MIME-Version: 1.0\r\n"
                                   "Content-Type: text/plain; charset=UTF-8\r\n"
                                   "Content-Transfer-Encoding: 8bit\r\n"
                                   "\r\n"
                                   "%5\r\n.\r\n")
                                   .arg(settings.senderName.trimmed(),
                                        formatAddress(settings.senderEmail),
                                        formatAddress(toEmail),
                                        subject.trimmed(),
                                        body)
                                   .toUtf8();

    if (!writeCommand(&socket, message, settings.timeoutMs, errorMessage)
        || !expectResponse(&socket, settings.timeoutMs, {250}, errorMessage)) {
        return false;
    }

    if (!writeCommand(&socket, QByteArray("QUIT\r\n"), settings.timeoutMs, errorMessage)
        || !expectResponse(&socket, settings.timeoutMs, {221}, errorMessage)) {
        return false;
    }

    return true;
}