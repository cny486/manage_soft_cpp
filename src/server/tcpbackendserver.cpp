#include "tcpbackendserver.h"

#include "aiinventoryenricher.h"
#include "jsonstorageservice.h"
#include "tcpmessagecodec.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUuid>

namespace {
QJsonObject payloadFromRequest(const QJsonObject &request)
{
    return request.value(QStringLiteral("payload")).toObject();
}

QString authStatusText(AuthenticationStatus status)
{
    switch (status) {
    case AuthenticationStatus::Success:
        return QStringLiteral("success");
    case AuthenticationStatus::UserNotFound:
        return QStringLiteral("user_not_found");
    case AuthenticationStatus::WrongPassword:
        return QStringLiteral("wrong_password");
    case AuthenticationStatus::Failed:
    default:
        return QStringLiteral("failed");
    }
}

QStringList splitAttachmentPaths(const QVariant &value)
{
    QStringList paths;
    const QStringList parts = value.toString().split(QChar('\n'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            paths.append(trimmed);
        }
    }
    return paths;
}

void cleanupTempFiles(const QStringList &filePaths)
{
    for (const QString &filePath : filePaths) {
        QFile::remove(filePath);
    }
}

AiApiSettings aiSettingsFromJson(const QJsonObject &object)
{
    AiApiSettings settings;
    settings.apiUrl = object.value(QStringLiteral("apiUrl")).toString().trimmed();
    settings.apiKey = object.value(QStringLiteral("apiKey")).toString().trimmed();
    settings.model = object.value(QStringLiteral("model")).toString().trimmed();

    const QJsonValue timeoutValue = object.value(QStringLiteral("timeoutMs"));
    const int timeoutMs = timeoutValue.toVariant().toInt();
    if (!timeoutValue.isUndefined() && timeoutMs > 0) {
        settings.timeoutMs = timeoutMs;
    }

    return settings;
}
}

TcpBackendServer::TcpBackendServer(JsonStorageService *storage, QObject *parent)
    : QObject(parent),
      m_storage(storage),
      m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, [this]() { handleNewConnection(); });
}

bool TcpBackendServer::listen(const QHostAddress &address, quint16 port, QString *errorMessage)
{
    if (m_server->listen(address, port)) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = m_server->errorString();
    }
    return false;
}

void TcpBackendServer::handleNewConnection()
{
    while (QTcpSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { handleSocketReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }
}

void TcpBackendServer::handleSocketReadyRead(QTcpSocket *socket)
{
    QByteArray buffer = socket->property("buffer").toByteArray();
    buffer.append(socket->readAll());

    QJsonObject request;
    while (TcpMessageCodec::tryTakeMessage(&buffer, &request)) {
        const QJsonObject response = processRequest(request);
        socket->write(TcpMessageCodec::encodeMessage(response));
        socket->flush();
    }

    socket->setProperty("buffer", buffer);
}

QJsonObject TcpBackendServer::processRequest(const QJsonObject &request) const
{
    const QString requestId = request.value(QStringLiteral("requestId")).toString();
    const QString action = request.value(QStringLiteral("action")).toString();
    const QJsonObject payload = payloadFromRequest(request);

    if (action == QStringLiteral("health.ping")) {
        return makeResponse(requestId,
                            true,
                            {{QStringLiteral("server"), QStringLiteral("ManageSoftServer")}},
                            QStringLiteral("pong"));
    }

    if (action == QStringLiteral("auth.login")) {
        QString message;
        const AuthenticationStatus status = m_storage->authenticate(payload.value(QStringLiteral("username")).toString(),
                                                                    payload.value(QStringLiteral("password")).toString(),
                                                                    &message);
        QJsonObject responsePayload{{QStringLiteral("status"), authStatusText(status)}};
        if (status == AuthenticationStatus::Success) {
            const QString sessionToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
            const QString userName = m_storage->currentUserName();
            m_sessionUsers.insert(sessionToken, userName);
            responsePayload.insert(QStringLiteral("sessionToken"), sessionToken);
            responsePayload.insert(QStringLiteral("userName"), userName);
        } else if (!message.isEmpty()) {
            responsePayload.insert(QStringLiteral("message"), message);
        }

        return makeResponse(requestId, true, responsePayload, message);
    }

    if (authenticatedUserName(payload).isEmpty()) {
        return makeResponse(requestId, false, {}, QStringLiteral("未登录或登录状态已失效，请重新登录。"));
    }

    m_storage->setCurrentUserName(authenticatedUserName(payload));

    if (action == QStringLiteral("account.security.info")) {
        UserSecurityInfo info;
        QString message;
        const bool success = m_storage->loadCurrentUserSecurityInfo(&info, &message);
        return makeResponse(requestId,
                            success,
                            {{QStringLiteral("userName"), info.userName},
                             {QStringLiteral("email"), info.email}},
                            message);
    }

    if (action == QStringLiteral("account.verification.send")) {
        QString message;
        const bool success = m_storage->sendCurrentUserVerificationCode(
            static_cast<VerificationPurpose>(payload.value(QStringLiteral("purpose")).toInt()),
            payload.value(QStringLiteral("email")).toString(),
            &message);
        return makeResponse(requestId, success, {}, message);
    }

    if (action == QStringLiteral("account.email.bind")) {
        QString message;
        const bool success = m_storage->bindCurrentUserEmail(payload.value(QStringLiteral("email")).toString(),
                                                             payload.value(QStringLiteral("verificationCode")).toString(),
                                                             &message);
        return makeResponse(requestId, success, {}, message);
    }

    if (action == QStringLiteral("account.password.change")) {
        QString message;
        const bool success = m_storage->changeCurrentUserPassword(payload.value(QStringLiteral("newPassword")).toString(),
                                                                  payload.value(QStringLiteral("verificationCode")).toString(),
                                                                  &message);
        return makeResponse(requestId, success, {}, message);
    }

    if (action == QStringLiteral("page.list")) {
        const QList<QVariantMap> records = m_storage->loadPageRecords(payload.value(QStringLiteral("pageId")).toString());
        return makeResponse(requestId, true, {{QStringLiteral("records"), TcpMessageCodec::variantMapsToJson(records)}});
    }

    if (action == QStringLiteral("page.upsert")) {
        QVariantMap record = payload.value(QStringLiteral("record")).toObject().toVariantMap();
        QStringList tempFiles;
        const QJsonArray uploads = payload.value(QStringLiteral("attachmentUploads")).toArray();
        QString message;

        for (const QJsonValue &value : uploads) {
            const QJsonObject upload = value.toObject();
            const QString sourceFileName = upload.value(QStringLiteral("fileName")).toString();
            const QString suffix = QFileInfo(sourceFileName).suffix().trimmed();
            QString tempFilePath;
            const QByteArray content = QByteArray::fromBase64(upload.value(QStringLiteral("fileContentBase64")).toString().toLatin1());
            if (!writeTempFile(suffix.isEmpty() ? QStringLiteral(".bin") : QStringLiteral(".") + suffix,
                               content,
                               &tempFilePath,
                               &message)) {
                cleanupTempFiles(tempFiles);
                return makeResponse(requestId, false, {}, message);
            }

            tempFiles.append(tempFilePath);

            const QString fieldKey = upload.value(QStringLiteral("fieldKey")).toString();
            QStringList paths = splitAttachmentPaths(record.value(fieldKey));
            const int index = upload.value(QStringLiteral("index")).toInt();
            while (paths.size() <= index) {
                paths.append(QString());
            }
            paths[index] = tempFilePath;
            record.insert(fieldKey, paths.join(QStringLiteral("\n")));
        }

        const bool success = m_storage->upsertRecord(payload.value(QStringLiteral("pageId")).toString(),
                                                     record,
                                                     &message);
        cleanupTempFiles(tempFiles);
        return makeResponse(requestId, success, {}, message);
    }

    if (action == QStringLiteral("page.remove")) {
        QString message;
        const bool success = m_storage->removeRecord(payload.value(QStringLiteral("pageId")).toString(),
                                                     payload.value(QStringLiteral("id")).toString(),
                                                     &message);
        return makeResponse(requestId, success, {}, message);
    }

    if (action == QStringLiteral("reimbursement.attachments.fetch")) {
        QList<ReimbursementAttachmentContent> attachments;
        QString message;
        const bool success = m_storage->loadReimbursementAttachments(payload.value(QStringLiteral("record")).toObject().toVariantMap(),
                                                                     &attachments,
                                                                     &message);
        if (!success) {
            return makeResponse(requestId, false, {}, message);
        }

        QJsonArray items;
        for (const ReimbursementAttachmentContent &attachment : attachments) {
            items.append(QJsonObject{{QStringLiteral("reference"), attachment.reference},
                                     {QStringLiteral("fileName"), attachment.fileName},
                                     {QStringLiteral("fileContentBase64"), QString::fromLatin1(attachment.content.toBase64())}});
        }
        return makeResponse(requestId, true, {{QStringLiteral("attachments"), items}});
    }

    if (action == QStringLiteral("inventory.history.list")) {
        return makeResponse(requestId,
                            true,
                            {{QStringLiteral("records"), TcpMessageCodec::variantMapsToJson(m_storage->loadInventoryHistory())}});
    }

    if (action == QStringLiteral("inventory.change")) {
        QString message;
        const bool success = m_storage->applyInventoryChange(
            payload.value(QStringLiteral("itemData")).toObject().toVariantMap(),
            static_cast<InventoryOperationType>(payload.value(QStringLiteral("operationType")).toInt()),
            static_cast<InventoryInputType>(payload.value(QStringLiteral("inputType")).toInt()),
            payload.value(QStringLiteral("note")).toString(),
            payload.value(QStringLiteral("sourceFile")).toString(),
            payload.value(QStringLiteral("sourceRow")).toInt(),
            &message);
        return makeResponse(requestId, success, {}, message);
    }

    if (action == QStringLiteral("inventory.remove")) {
        QString message;
        const bool success = m_storage->deleteInventoryRecord(payload.value(QStringLiteral("id")).toString(),
                                                              payload.value(QStringLiteral("note")).toString(),
                                                              &message);
        return makeResponse(requestId, success, {}, message);
    }

    if (action == QStringLiteral("inventory.agent.enrich")) {
        QString message;
        InventoryEnrichmentResult result;
        const bool success = m_storage->enrichInventoryRecord(
            payload.value(QStringLiteral("manufacturerPart")).toString(),
            payload.value(QStringLiteral("currentRecord")).toObject().toVariantMap(),
            payload.value(QStringLiteral("desiredFieldKeys")).toVariant().toStringList(),
            &result,
            &message);
        return makeResponse(requestId,
                            success,
                            {{QStringLiteral("result"), TcpMessageCodec::inventoryEnrichmentResultToJson(result)}},
                            message);
    }

    if (action == QStringLiteral("inventory.agent.test")) {
        QString message;
        QString responsePreview;
        const AiApiSettings settings = aiSettingsFromJson(payload.value(QStringLiteral("aiSettings")).toObject());
        const bool success = AiInventoryEnricher::testConnection(settings, &responsePreview, &message);
        return makeResponse(requestId,
                            success,
                            {{QStringLiteral("responsePreview"), responsePreview}},
                            success ? QStringLiteral("AI 连接测试成功。") : message);
    }

    if (action == QStringLiteral("page.importExcel")
        || action == QStringLiteral("inventory.importExcel")
        || action == QStringLiteral("inventory.importBom")
        || action == QStringLiteral("inventory.fulfillment.analyze")) {
        const QByteArray content = QByteArray::fromBase64(payload.value(QStringLiteral("fileContentBase64")).toString().toLatin1());
        QString tempFilePath;
        QString message;
        if (!writeTempFile(QStringLiteral(".xlsx"), content, &tempFilePath, &message)) {
            return makeResponse(requestId, false, {}, message);
        }

        if (action == QStringLiteral("page.importExcel")) {
            const bool success = m_storage->importExcel(payload.value(QStringLiteral("pageId")).toString(),
                                                        TcpMessageCodec::fieldDefinitionsFromJson(payload.value(QStringLiteral("fields")).toArray()),
                                                        tempFilePath,
                                                        &message);
            QFile::remove(tempFilePath);
            return makeResponse(requestId, success, {}, message);
        }

        if (action == QStringLiteral("inventory.importExcel")) {
            const bool success = m_storage->importInventoryExcel(
                TcpMessageCodec::fieldDefinitionsFromJson(payload.value(QStringLiteral("fields")).toArray()),
                tempFilePath,
                static_cast<InventoryOperationType>(payload.value(QStringLiteral("operationType")).toInt()),
                payload.value(QStringLiteral("note")).toString(),
                &message);
            QFile::remove(tempFilePath);
            return makeResponse(requestId, success, {}, message);
        }

        if (action == QStringLiteral("inventory.importBom")) {
            const bool success = m_storage->importInventoryBom(
                tempFilePath,
                static_cast<InventoryOperationType>(payload.value(QStringLiteral("operationType")).toInt()),
                payload.value(QStringLiteral("note")).toString(),
                &message);
            QFile::remove(tempFilePath);
            return makeResponse(requestId, success, {}, message);
        }

        QList<InventoryFulfillmentResult> results;
        const bool success = m_storage->analyzeInventoryFulfillment(tempFilePath, &results, &message);
        QFile::remove(tempFilePath);
        return makeResponse(requestId,
                            success,
                            {{QStringLiteral("results"), TcpMessageCodec::fulfillmentResultsToJson(results)}},
                            message);
    }

    if (action == QStringLiteral("page.exportExcel")) {
        const QString filePath = QDir::temp().filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".xlsx"));
        QString message;
        const bool success = m_storage->exportExcel(payload.value(QStringLiteral("pageId")).toString(),
                                                    TcpMessageCodec::fieldDefinitionsFromJson(payload.value(QStringLiteral("fields")).toArray()),
                                                    payload.value(QStringLiteral("sheetName")).toString(),
                                                    filePath,
                                                    &message);
        if (!success) {
            return makeResponse(requestId, false, {}, message);
        }

        QByteArray content;
        if (!readFileBytes(filePath, &content, &message)) {
            QFile::remove(filePath);
            return makeResponse(requestId, false, {}, message);
        }
        QFile::remove(filePath);
        return makeResponse(requestId,
                            true,
                            {{QStringLiteral("fileContentBase64"), QString::fromLatin1(content.toBase64())}},
                            message);
    }

    if (action == QStringLiteral("inventory.fulfillment.export")) {
        const QString filePath = QDir::temp().filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".xlsx"));
        QString message;
        const bool success = m_storage->exportInventoryFulfillment(
            TcpMessageCodec::fulfillmentResultsFromJson(payload.value(QStringLiteral("results")).toArray()),
            filePath,
            &message);
        if (!success) {
            return makeResponse(requestId, false, {}, message);
        }

        QByteArray content;
        if (!readFileBytes(filePath, &content, &message)) {
            QFile::remove(filePath);
            return makeResponse(requestId, false, {}, message);
        }
        QFile::remove(filePath);
        return makeResponse(requestId,
                            true,
                            {{QStringLiteral("fileContentBase64"), QString::fromLatin1(content.toBase64())}},
                            message);
    }

    if (action == QStringLiteral("inventory.fulfillment.apply")) {
        QString message;
        const bool success = m_storage->applyInventoryFulfillment(
            TcpMessageCodec::fulfillmentResultsFromJson(payload.value(QStringLiteral("results")).toArray()),
            &message);
        return makeResponse(requestId, success, {}, message);
    }

    return makeResponse(requestId, false, {}, QStringLiteral("不支持的操作：%1").arg(action));
}

QString TcpBackendServer::authenticatedUserName(const QJsonObject &payload) const
{
    const QString sessionToken = payload.value(QStringLiteral("sessionToken")).toString().trimmed();
    if (sessionToken.isEmpty()) {
        return QString();
    }

    return m_sessionUsers.value(sessionToken).trimmed();
}

QJsonObject TcpBackendServer::makeResponse(const QString &requestId,
                                          bool success,
                                          const QJsonObject &payload,
                                          const QString &message) const
{
    return {
        {QStringLiteral("requestId"), requestId},
        {QStringLiteral("success"), success},
        {QStringLiteral("message"), message},
        {QStringLiteral("payload"), payload}
    };
}

bool TcpBackendServer::writeTempFile(const QString &suffix,
                                     const QByteArray &content,
                                     QString *filePath,
                                     QString *errorMessage) const
{
    const QString path = QDir::temp().filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + suffix);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("写入临时文件失败：%1").arg(file.errorString());
        }
        return false;
    }
    if (file.write(content) != content.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("写入临时文件失败：%1").arg(file.errorString());
        }
        return false;
    }
    file.close();

    if (filePath != nullptr) {
        *filePath = path;
    }
    return true;
}

bool TcpBackendServer::readFileBytes(const QString &filePath, QByteArray *content, QString *errorMessage) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("读取文件失败：%1").arg(file.errorString());
        }
        return false;
    }
    if (content != nullptr) {
        *content = file.readAll();
    }
    return true;
}