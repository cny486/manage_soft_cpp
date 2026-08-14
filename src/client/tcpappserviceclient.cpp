#include "tcpappserviceclient.h"

#include "appversion.h"
#include "tcpmessagecodec.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QTcpSocket>
#include <QUuid>

namespace {
QJsonObject makeRequest(const QString &action, const QJsonObject &payload)
{
    return {
        {QStringLiteral("version"), AppVersion::protocolVersion()},
        {QStringLiteral("requestId"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {QStringLiteral("action"), action},
        {QStringLiteral("payload"), payload}
    };
}

bool readLocalFileBytes(const QString &filePath, QByteArray *content, QString *errorMessage)
{
    if (content == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("无效的附件缓冲区。");
        }
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("读取附件失败：%1").arg(file.errorString());
        }
        return false;
    }

    *content = file.readAll();
    return true;
}

QStringList reimbursementAttachmentKeys()
{
    return {QStringLiteral("invoiceAttachment"), QStringLiteral("otherAttachments")};
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

QJsonArray reimbursementAttachmentUploads(const QVariantMap &record, QString *errorMessage)
{
    QJsonArray uploads;

    for (const QString &key : reimbursementAttachmentKeys()) {
        const QStringList paths = splitAttachmentPaths(record.value(key));
        for (int index = 0; index < paths.size(); ++index) {
            const QString &path = paths.at(index);
            QFileInfo fileInfo(path);
            if (!fileInfo.exists()) {
                continue;
            }

            if (!fileInfo.isFile()) {
                if (errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("附件不是有效文件：%1").arg(path);
                }
                return {};
            }

            QByteArray content;
            if (!readLocalFileBytes(fileInfo.absoluteFilePath(), &content, errorMessage)) {
                return {};
            }

            uploads.append(QJsonObject{{QStringLiteral("fieldKey"), key},
                                       {QStringLiteral("index"), index},
                                       {QStringLiteral("sourcePath"), path},
                                       {QStringLiteral("fileName"), fileInfo.fileName()},
                                       {QStringLiteral("fileContentBase64"), QString::fromLatin1(content.toBase64())}});
        }
    }

    return uploads;
}

QJsonObject aiSettingsToJson(const AiApiSettings &settings)
{
    return {
        {QStringLiteral("apiUrl"), settings.apiUrl},
        {QStringLiteral("apiKey"), settings.apiKey},
        {QStringLiteral("model"), settings.model},
        {QStringLiteral("timeoutMs"), settings.timeoutMs}
    };
}
}

TcpAppServiceClient::TcpAppServiceClient(const QString &host, quint16 port, int timeoutMs)
    : m_host(host),
      m_port(port),
      m_timeoutMs(timeoutMs)
{
}

AuthenticationStatus TcpAppServiceClient::authenticate(const QString &username,
                                                       const QString &password,
                                                       QString *errorMessage)
{
    QJsonObject response;
    if (!sendRequest(QStringLiteral("auth.login"),
                     {{QStringLiteral("username"), username.trimmed()},
                      {QStringLiteral("password"), password}},
                     &response,
                     errorMessage)) {
        m_sessionToken.clear();
        m_currentUserName.clear();
        return AuthenticationStatus::Failed;
    }

    QString message;
    if (!responseSucceeded(response, &message)) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        m_sessionToken.clear();
        m_currentUserName.clear();
        return AuthenticationStatus::Failed;
    }

    const QJsonObject payload = response.value(QStringLiteral("payload")).toObject();
    const QString status = payload.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("success")) {
        m_sessionToken = payload.value(QStringLiteral("sessionToken")).toString();
        m_currentUserName = payload.value(QStringLiteral("userName")).toString();
        return AuthenticationStatus::Success;
    }

    m_sessionToken.clear();
    m_currentUserName.clear();
    if (status == QStringLiteral("user_not_found")) {
        return AuthenticationStatus::UserNotFound;
    }

    if (status == QStringLiteral("wrong_password")) {
        return AuthenticationStatus::WrongPassword;
    }

    if (errorMessage != nullptr) {
        *errorMessage = payload.value(QStringLiteral("message")).toString();
    }
    return AuthenticationStatus::Failed;
}

void TcpAppServiceClient::setCurrentUserName(const QString &userName)
{
    m_currentUserName = userName.trimmed();
}

QString TcpAppServiceClient::currentUserName() const
{
    return m_currentUserName;
}

bool TcpAppServiceClient::loadCurrentUserSecurityInfo(UserSecurityInfo *info,
                                                      QString *errorMessage) const
{
    if (info == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("用户信息输出参数无效。");
        }
        return false;
    }

    QJsonObject response;
    if (!sendRequest(QStringLiteral("account.security.info"), {}, &response, errorMessage)) {
        return false;
    }

    QString message;
    if (!responseSucceeded(response, &message)) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        return false;
    }

    const QJsonObject payload = response.value(QStringLiteral("payload")).toObject();
    info->userName = payload.value(QStringLiteral("userName")).toString();
    info->email = payload.value(QStringLiteral("email")).toString();
    return true;
}

bool TcpAppServiceClient::sendCurrentUserVerificationCode(VerificationPurpose purpose,
                                                          const QString &email,
                                                          QString *errorMessage)
{
    QJsonObject response;
    if (!sendRequest(QStringLiteral("account.verification.send"),
                     {{QStringLiteral("purpose"), static_cast<int>(purpose)},
                      {QStringLiteral("email"), email.trimmed()}},
                     &response,
                     errorMessage)) {
        return false;
    }

    QString message;
    const bool success = responseSucceeded(response, &message);
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return success;
}

bool TcpAppServiceClient::bindCurrentUserEmail(const QString &email,
                                               const QString &verificationCode,
                                               QString *errorMessage)
{
    QJsonObject response;
    if (!sendRequest(QStringLiteral("account.email.bind"),
                     {{QStringLiteral("email"), email.trimmed()},
                      {QStringLiteral("verificationCode"), verificationCode.trimmed()}},
                     &response,
                     errorMessage)) {
        return false;
    }

    QString message;
    const bool success = responseSucceeded(response, &message);
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return success;
}

bool TcpAppServiceClient::changeCurrentUserPassword(const QString &newPassword,
                                                    const QString &verificationCode,
                                                    QString *errorMessage)
{
    QJsonObject response;
    if (!sendRequest(QStringLiteral("account.password.change"),
                     {{QStringLiteral("newPassword"), newPassword},
                      {QStringLiteral("verificationCode"), verificationCode.trimmed()}},
                     &response,
                     errorMessage)) {
        return false;
    }

    QString message;
    const bool success = responseSucceeded(response, &message);
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return success;
}

QString TcpAppServiceClient::storageRoot() const
{
    return QStringLiteral("tcp://%1:%2").arg(m_host).arg(m_port);
}

QList<QVariantMap> TcpAppServiceClient::loadPageRecords(const QString &pageId) const
{
    QJsonObject response;
    QString errorMessage;
    if (!sendRequest(QStringLiteral("page.list"), {{QStringLiteral("pageId"), pageId}}, &response, &errorMessage)) {
        return {};
    }

    QString message;
    if (!responseSucceeded(response, &message)) {
        return {};
    }

    return TcpMessageCodec::variantMapsFromJson(response.value(QStringLiteral("payload")).toObject().value(QStringLiteral("records")).toArray());
}

bool TcpAppServiceClient::upsertRecord(const QString &pageId, QVariantMap record, QString *errorMessage) const
{
    QJsonObject payload{{QStringLiteral("pageId"), pageId},
                        {QStringLiteral("record"), QJsonObject::fromVariantMap(record)}};
    if (pageId == QStringLiteral("reimbursement")) {
        const QJsonArray uploads = reimbursementAttachmentUploads(record, errorMessage);
        if (uploads.isEmpty() == false) {
            payload.insert(QStringLiteral("attachmentUploads"), uploads);
        } else if (errorMessage != nullptr && !errorMessage->isEmpty()) {
            return false;
        }
    }

    QJsonObject response;
    if (!sendRequest(QStringLiteral("page.upsert"), payload, &response, errorMessage)) {
        return false;
    }

    QString message;
    const bool success = responseSucceeded(response, &message);
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return success;
}

bool TcpAppServiceClient::removeRecord(const QString &pageId, const QString &id, QString *errorMessage) const
{
    QJsonObject response;
    if (!sendRequest(QStringLiteral("page.remove"),
                     {{QStringLiteral("pageId"), pageId}, {QStringLiteral("id"), id}},
                     &response,
                     errorMessage)) {
        return false;
    }

    QString message;
    const bool success = responseSucceeded(response, &message);
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return success;
}

bool TcpAppServiceClient::deleteInventoryRecord(const QString &id,
                                                const QString &note,
                                                QString *errorMessage) const
{
    QJsonObject response;
    if (!sendRequest(QStringLiteral("inventory.remove"),
                     {{QStringLiteral("id"), id}, {QStringLiteral("note"), note}},
                     &response,
                     errorMessage)) {
        return false;
    }

    QString message;
    const bool success = responseSucceeded(response, &message);
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return success;
}

bool TcpAppServiceClient::importExcel(const QString &pageId,
                                      const QList<FieldDefinition> &fields,
                                      const QString &filePath,
                                      QString *errorMessage) const
{
    QByteArray content;
    if (!readFileBytes(filePath, &content, errorMessage)) {
        return false;
    }

    QJsonObject response;
    if (!sendRequest(QStringLiteral("page.importExcel"),
                     {{QStringLiteral("pageId"), pageId},
                      {QStringLiteral("fields"), TcpMessageCodec::fieldDefinitionsToJson(fields)},
                      {QStringLiteral("fileName"), QFileInfo(filePath).fileName()},
                      {QStringLiteral("fileContentBase64"), QString::fromLatin1(content.toBase64())}},
                     &response,
                     errorMessage)) {
        return false;
    }

    QString message;
    const bool success = responseSucceeded(response, &message);
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return success;
}

bool TcpAppServiceClient::exportExcel(const QString &pageId,
                                      const QList<FieldDefinition> &fields,
                                      const QString &sheetName,
                                      const QString &filePath,
                                      QString *errorMessage) const
{
    QJsonObject response;
    if (!sendRequest(QStringLiteral("page.exportExcel"),
                     {{QStringLiteral("pageId"), pageId},
                      {QStringLiteral("fields"), TcpMessageCodec::fieldDefinitionsToJson(fields)},
                      {QStringLiteral("sheetName"), sheetName}},
                     &response,
                     errorMessage)) {
        return false;
    }

    QString message;
    if (!responseSucceeded(response, &message)) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        return false;
    }

    const QByteArray content = QByteArray::fromBase64(
        response.value(QStringLiteral("payload")).toObject().value(QStringLiteral("fileContentBase64")).toString().toLatin1());
    if (!writeFileBytes(filePath, content, errorMessage)) {
        return false;
    }
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return true;
}

QList<QVariantMap> TcpAppServiceClient::loadInventoryHistory() const
{
    QJsonObject response;
    QString errorMessage;
    if (!sendRequest(QStringLiteral("inventory.history.list"), {}, &response, &errorMessage)) {
        return {};
    }

    QString message;
    if (!responseSucceeded(response, &message)) {
        return {};
    }

    return TcpMessageCodec::variantMapsFromJson(response.value(QStringLiteral("payload")).toObject().value(QStringLiteral("records")).toArray());
}

bool TcpAppServiceClient::applyInventoryChange(const QVariantMap &itemData,
                                               InventoryOperationType operationType,
                                               InventoryInputType inputType,
                                               const QString &note,
                                               const QString &sourceFile,
                                               int sourceRow,
                                               QString *errorMessage) const
{
    QJsonObject response;
    if (!sendRequest(QStringLiteral("inventory.change"),
                     {{QStringLiteral("itemData"), QJsonObject::fromVariantMap(itemData)},
                      {QStringLiteral("operationType"), static_cast<int>(operationType)},
                      {QStringLiteral("inputType"), static_cast<int>(inputType)},
                      {QStringLiteral("note"), note},
                      {QStringLiteral("sourceFile"), sourceFile},
                      {QStringLiteral("sourceRow"), sourceRow}},
                     &response,
                     errorMessage)) {
        return false;
    }

    QString message;
    const bool success = responseSucceeded(response, &message);
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return success;
}

bool TcpAppServiceClient::importInventoryExcel(const QList<FieldDefinition> &fields,
                                               const QString &filePath,
                                               InventoryOperationType operationType,
                                               const QString &note,
                                               QString *errorMessage) const
{
    QByteArray content;
    if (!readFileBytes(filePath, &content, errorMessage)) {
        return false;
    }

    QJsonObject response;
    if (!sendRequest(QStringLiteral("inventory.importExcel"),
                     {{QStringLiteral("fields"), TcpMessageCodec::fieldDefinitionsToJson(fields)},
                      {QStringLiteral("fileName"), QFileInfo(filePath).fileName()},
                      {QStringLiteral("fileContentBase64"), QString::fromLatin1(content.toBase64())},
                      {QStringLiteral("operationType"), static_cast<int>(operationType)},
                      {QStringLiteral("note"), note}},
                     &response,
                     errorMessage)) {
        return false;
    }

    QString message;
    const bool success = responseSucceeded(response, &message);
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return success;
}

bool TcpAppServiceClient::importInventoryBom(const QString &filePath,
                                             InventoryOperationType operationType,
                                             const QString &note,
                                             QString *errorMessage) const
{
    QByteArray content;
    if (!readFileBytes(filePath, &content, errorMessage)) {
        return false;
    }

    QJsonObject response;
    if (!sendRequest(QStringLiteral("inventory.importBom"),
                     {{QStringLiteral("fileName"), QFileInfo(filePath).fileName()},
                      {QStringLiteral("fileContentBase64"), QString::fromLatin1(content.toBase64())},
                      {QStringLiteral("operationType"), static_cast<int>(operationType)},
                      {QStringLiteral("note"), note}},
                     &response,
                     errorMessage)) {
        return false;
    }

    QString message;
    const bool success = responseSucceeded(response, &message);
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return success;
}

bool TcpAppServiceClient::analyzeInventoryFulfillment(const QString &filePath,
                                                      int fulfillmentSetCount,
                                                      QList<InventoryFulfillmentResult> *results,
                                                      QString *errorMessage) const
{
    QByteArray content;
    if (!readFileBytes(filePath, &content, errorMessage)) {
        return false;
    }

    QJsonObject response;
    if (!sendRequest(QStringLiteral("inventory.fulfillment.analyze"),
                     {{QStringLiteral("fileName"), QFileInfo(filePath).fileName()},
                      {QStringLiteral("fulfillmentSetCount"), fulfillmentSetCount},
                      {QStringLiteral("fileContentBase64"), QString::fromLatin1(content.toBase64())}},
                     &response,
                     errorMessage,
                     fulfillmentRequestTimeoutMs())) {
        return false;
    }

    QString message;
    const bool success = responseSucceeded(response, &message);
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    if (!success) {
        return false;
    }

    if (results != nullptr) {
        *results = TcpMessageCodec::fulfillmentResultsFromJson(
            response.value(QStringLiteral("payload")).toObject().value(QStringLiteral("results")).toArray());
    }
    return true;
}

bool TcpAppServiceClient::enrichInventoryRecord(const QString &manufacturerPart,
                                                const QVariantMap &currentRecord,
                                                const QStringList &desiredFieldKeys,
                                                InventoryEnrichmentResult *result,
                                                QString *errorMessage) const
{
    QJsonObject response;
    if (!sendRequest(QStringLiteral("inventory.agent.enrich"),
                     {{QStringLiteral("manufacturerPart"), manufacturerPart},
                      {QStringLiteral("currentRecord"), QJsonObject::fromVariantMap(currentRecord)},
                      {QStringLiteral("desiredFieldKeys"), QJsonArray::fromStringList(desiredFieldKeys)}},
                     &response,
                     errorMessage,
                     aiRequestTimeoutMs())) {
        return false;
    }

    QString message;
    const bool success = responseSucceeded(response, &message);
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    if (!success) {
        return false;
    }

    if (result != nullptr) {
        *result = TcpMessageCodec::inventoryEnrichmentResultFromJson(
            response.value(QStringLiteral("payload")).toObject().value(QStringLiteral("result")).toObject());
    }
    return true;
}

bool TcpAppServiceClient::importDemandList(const QString &name,
                                           const QString &filePath,
                                           QString *errorMessage) const
{
    QByteArray content;
    if (!readFileBytes(filePath, &content, errorMessage)) {
        return false;
    }

    QJsonObject response;
    if (!sendRequest(QStringLiteral("demand.list.import"),
                     {{QStringLiteral("name"), name.trimmed()},
                      {QStringLiteral("fileName"), QFileInfo(filePath).fileName()},
                      {QStringLiteral("fileContentBase64"), QString::fromLatin1(content.toBase64())}},
                     &response,
                     errorMessage,
                     fulfillmentRequestTimeoutMs())) {
        return false;
    }

    QString message;
    const bool success = responseSucceeded(response, &message);
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return success;
}

bool TcpAppServiceClient::exportDemandList(const QString &recordId,
                                           const QString &filePath,
                                           QString *errorMessage) const
{
    QJsonObject response;
    if (!sendRequest(QStringLiteral("demand.list.export"),
                     {{QStringLiteral("recordId"), recordId}},
                     &response,
                     errorMessage,
                     fulfillmentRequestTimeoutMs())) {
        return false;
    }

    QString message;
    if (!responseSucceeded(response, &message)) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        return false;
    }

    const QByteArray content = QByteArray::fromBase64(
        response.value(QStringLiteral("payload")).toObject().value(QStringLiteral("fileContentBase64")).toString().toLatin1());
    if (!writeFileBytes(filePath, content, errorMessage)) {
        return false;
    }
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return true;
}

bool TcpAppServiceClient::loadDemandListItems(const QString &recordId,
                                              QList<DemandListItem> *items,
                                              QString *errorMessage) const
{
    if (items == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("清单条目输出参数不能为空。");
        }
        return false;
    }

    QJsonObject response;
    if (!sendRequest(QStringLiteral("demand.list.items"),
                     {{QStringLiteral("recordId"), recordId}},
                     &response,
                     errorMessage,
                     fulfillmentRequestTimeoutMs())) {
        return false;
    }

    QString message;
    if (!responseSucceeded(response, &message)) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        return false;
    }

    *items = TcpMessageCodec::demandListItemsFromJson(
        response.value(QStringLiteral("payload")).toObject().value(QStringLiteral("items")).toArray());
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return true;
}

bool TcpAppServiceClient::analyzeDemandListFulfillment(const QString &recordId,
                                                       int buildCount,
                                                       QList<InventoryFulfillmentResult> *results,
                                                       QString *errorMessage) const
{
    QJsonObject response;
    if (!sendRequest(QStringLiteral("demand.list.fulfillment.analyze"),
                     {{QStringLiteral("recordId"), recordId},
                      {QStringLiteral("buildCount"), buildCount}},
                     &response,
                     errorMessage,
                     fulfillmentRequestTimeoutMs())) {
        return false;
    }

    QString message;
    const bool success = responseSucceeded(response, &message);
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    if (!success) {
        return false;
    }

    if (results != nullptr) {
        *results = TcpMessageCodec::fulfillmentResultsFromJson(
            response.value(QStringLiteral("payload")).toObject().value(QStringLiteral("results")).toArray());
    }
    return true;
}

bool TcpAppServiceClient::exportInventoryFulfillment(const QList<InventoryFulfillmentResult> &results,
                                                     const QString &sourceFilePath,
                                                     const QString &filePath,
                                                     QString *errorMessage) const
{
    QByteArray sourceContent;
    if (!readFileBytes(sourceFilePath, &sourceContent, errorMessage)) {
        return false;
    }

    QJsonObject response;
    if (!sendRequest(QStringLiteral("inventory.fulfillment.export"),
                     {{QStringLiteral("results"), TcpMessageCodec::fulfillmentResultsToJson(results)},
                      {QStringLiteral("sourceFileName"), QFileInfo(sourceFilePath).fileName()},
                      {QStringLiteral("sourceFileContentBase64"), QString::fromLatin1(sourceContent.toBase64())}},
                     &response,
                     errorMessage,
                     fulfillmentRequestTimeoutMs())) {
        return false;
    }

    QString message;
    if (!responseSucceeded(response, &message)) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        return false;
    }

    const QByteArray content = QByteArray::fromBase64(
        response.value(QStringLiteral("payload")).toObject().value(QStringLiteral("fileContentBase64")).toString().toLatin1());
    if (!writeFileBytes(filePath, content, errorMessage)) {
        return false;
    }
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return true;
}

bool TcpAppServiceClient::applyInventoryFulfillment(const QList<InventoryFulfillmentResult> &results,
                                                    QString *errorMessage) const
{
    QJsonObject response;
    if (!sendRequest(QStringLiteral("inventory.fulfillment.apply"),
                     {{QStringLiteral("results"), TcpMessageCodec::fulfillmentResultsToJson(results)}},
                     &response,
                     errorMessage)) {
        return false;
    }

    QString message;
    const bool success = responseSucceeded(response, &message);
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return success;
}

bool TcpAppServiceClient::loadReimbursementAttachments(const QVariantMap &record,
                                                       QList<ReimbursementAttachmentContent> *attachments,
                                                       QString *errorMessage) const
{
    if (attachments == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("附件输出缓冲区无效。");
        }
        return false;
    }

    QJsonObject response;
    if (!sendRequest(QStringLiteral("reimbursement.attachments.fetch"),
                     {{QStringLiteral("record"), QJsonObject::fromVariantMap(record)}},
                     &response,
                     errorMessage)) {
        return false;
    }

    QString message;
    if (!responseSucceeded(response, &message)) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        return false;
    }

    attachments->clear();
    const QJsonArray items = response.value(QStringLiteral("payload")).toObject().value(QStringLiteral("attachments")).toArray();
    for (const QJsonValue &value : items) {
        const QJsonObject object = value.toObject();
        attachments->append({object.value(QStringLiteral("reference")).toString(),
                             object.value(QStringLiteral("fileName")).toString(),
                             QByteArray::fromBase64(object.value(QStringLiteral("fileContentBase64")).toString().toLatin1())});
    }
    return true;
}

bool TcpAppServiceClient::ping(QString *errorMessage) const
{
    QJsonObject response;
    if (!sendRequest(QStringLiteral("health.ping"), {}, &response, errorMessage)) {
        return false;
    }

    QString message;
    const bool success = responseSucceeded(response, &message);
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return success;
}

bool TcpAppServiceClient::testAiConnection(const AiApiSettings &settings,
                                           QString *responsePreview,
                                           QString *errorMessage) const
{
    QJsonObject response;
    if (!sendRequest(QStringLiteral("inventory.agent.test"),
                     {{QStringLiteral("aiSettings"), aiSettingsToJson(settings)}},
                     &response,
                     errorMessage,
                     aiRequestTimeoutMs(settings.timeoutMs + 5000))) {
        return false;
    }

    QString message;
    const bool success = responseSucceeded(response, &message);
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    if (!success) {
        return false;
    }

    if (responsePreview != nullptr) {
        *responsePreview = response.value(QStringLiteral("payload")).toObject().value(QStringLiteral("responsePreview")).toString();
    }
    return true;
}

bool TcpAppServiceClient::checkForClientUpdate(const QString &currentVersion,
                                               ClientUpdateInfo *info,
                                               QString *errorMessage) const
{
    if (info == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Update result output buffer is invalid.");
        }
        return false;
    }

    QJsonObject response;
    if (!sendRequest(QStringLiteral("client.update.check"),
                     {{QStringLiteral("appVersion"), currentVersion.trimmed()}},
                     &response,
                     errorMessage)) {
        return false;
    }

    QString message;
    if (!responseSucceeded(response, &message)) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        return false;
    }

    *info = clientUpdateInfoFromJson(response.value(QStringLiteral("payload")).toObject());
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return true;
}

bool TcpAppServiceClient::downloadClientUpdatePackage(const QString &version,
                                                      QString *fileName,
                                                      QByteArray *content,
                                                      QString *sha256,
                                                      QString *errorMessage) const
{
    if (content == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Update package content buffer is invalid.");
        }
        return false;
    }

    QJsonObject response;
    if (!sendRequest(QStringLiteral("client.update.download"),
                     {{QStringLiteral("version"), version.trimmed()}},
                     &response,
                     errorMessage,
                     qMax(m_timeoutMs, 300000))) {
        return false;
    }

    QString message;
    if (!responseSucceeded(response, &message)) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        return false;
    }

    const QJsonObject payload = response.value(QStringLiteral("payload")).toObject();
    *content = QByteArray::fromBase64(payload.value(QStringLiteral("fileContentBase64")).toString().toLatin1());
    if (fileName != nullptr) {
        *fileName = payload.value(QStringLiteral("fileName")).toString().trimmed();
    }
    if (sha256 != nullptr) {
        *sha256 = payload.value(QStringLiteral("sha256")).toString().trimmed();
    }
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return true;
}

bool TcpAppServiceClient::sendRequest(const QString &action,
                                      const QJsonObject &payload,
                                      QJsonObject *response,
                                      QString *errorMessage,
                                      int timeoutMs) const
{
    QJsonObject requestPayload = payload;
    if (action != QStringLiteral("auth.login") && !m_sessionToken.isEmpty()) {
        requestPayload.insert(QStringLiteral("sessionToken"), m_sessionToken);
    }

    const int effectiveTimeoutMs = timeoutMs > 0 ? timeoutMs : m_timeoutMs;
    QTcpSocket socket;
    socket.connectToHost(m_host, m_port);
    if (!socket.waitForConnected(effectiveTimeoutMs)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("连接服务器失败：%1").arg(socket.errorString());
        }
        return false;
    }

    const QByteArray requestBytes = TcpMessageCodec::encodeMessage(makeRequest(action, requestPayload));
    if (socket.write(requestBytes) != requestBytes.size() || !socket.waitForBytesWritten(effectiveTimeoutMs)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("发送请求失败：%1").arg(socket.errorString());
        }
        return false;
    }

    QByteArray buffer;
    while (true) {
        buffer.append(socket.readAll());
        if (response != nullptr && TcpMessageCodec::tryTakeMessage(&buffer, response)) {
            return true;
        }
        if (!socket.waitForReadyRead(effectiveTimeoutMs)) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("等待服务器响应失败：%1").arg(socket.errorString());
            }
            return false;
        }
    }
}

bool TcpAppServiceClient::readFileBytes(const QString &filePath, QByteArray *content, QString *errorMessage) const
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

bool TcpAppServiceClient::writeFileBytes(const QString &filePath, const QByteArray &content, QString *errorMessage) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("写入文件失败：%1").arg(file.errorString());
        }
        return false;
    }
    if (file.write(content) != content.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("写入文件失败：%1").arg(file.errorString());
        }
        return false;
    }
    return true;
}

bool TcpAppServiceClient::responseSucceeded(const QJsonObject &response, QString *message) const
{
    if (message != nullptr) {
        *message = response.value(QStringLiteral("message")).toString();
    }
    return response.value(QStringLiteral("success")).toBool();
}

int TcpAppServiceClient::aiRequestTimeoutMs(int suggestedTimeoutMs) const
{
    const int minimumAiTimeoutMs = 65000;
    if (suggestedTimeoutMs > 0) {
        return qMax(qMax(m_timeoutMs, suggestedTimeoutMs), minimumAiTimeoutMs);
    }
    return qMax(m_timeoutMs, minimumAiTimeoutMs);
}

int TcpAppServiceClient::fulfillmentRequestTimeoutMs(int suggestedTimeoutMs) const
{
    const int minimumFulfillmentTimeoutMs = 120000;
    if (suggestedTimeoutMs > 0) {
        return qMax(qMax(m_timeoutMs, suggestedTimeoutMs), minimumFulfillmentTimeoutMs);
    }
    return qMax(m_timeoutMs, minimumFulfillmentTimeoutMs);
}
