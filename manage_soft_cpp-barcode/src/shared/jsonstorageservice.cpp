#include "jsonstorageservice.h"

#include "aiinventoryenricher.h"
#include "emailsettings.h"
#include "simplexlsxdocument.h"
#include "smtpemailclient.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>

namespace {
const char kUsersPageId[] = "users";
const char kDefaultPassword[] = "12345678";
const int kVerificationCodeExpiryMinutes = 10;
const int kVerificationSendWindowSeconds = 60;
const int kVerificationSendLimit = 3;

QStringList builtInUserNames()
{
    return {
        QStringLiteral("沈鹿易"),
        QStringLiteral("陈时进"),
        QStringLiteral("王晨漉"),
        QStringLiteral("张庆志"),
        QStringLiteral("钱思琪")
    };
}

QString normalizedEmail(const QString &email)
{
    return email.trimmed().toLower();
}

bool isValidEmail(const QString &email)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[A-Z0-9._%+-]+@[A-Z0-9.-]+\\.[A-Z]{2,}$"),
        QRegularExpression::CaseInsensitiveOption);
    return pattern.match(normalizedEmail(email)).hasMatch();
}

QString verificationPurposeKey(VerificationPurpose purpose)
{
    switch (purpose) {
    case VerificationPurpose::BindEmail:
        return QStringLiteral("bind_email");
    case VerificationPurpose::ChangePassword:
        return QStringLiteral("change_password");
    }

    return QStringLiteral("unknown");
}

QString verificationPurposeText(VerificationPurpose purpose)
{
    switch (purpose) {
    case VerificationPurpose::BindEmail:
        return QStringLiteral("绑定邮箱");
    case VerificationPurpose::ChangePassword:
        return QStringLiteral("修改密码");
    }

    return QStringLiteral("未知用途");
}

QString generateVerificationCode()
{
    return QStringLiteral("%1").arg(QRandomGenerator::global()->bounded(1000000), 6, 10, QChar('0'));
}

QString errorText(const QString &prefix, const QFile &file)
{
    return QString("%1：%2").arg(prefix, file.errorString());
}

QJsonObject mapToJsonObject(const QVariantMap &record)
{
    return QJsonObject::fromVariantMap(record);
}

QVariantMap jsonObjectToMap(const QJsonObject &object)
{
    return object.toVariantMap();
}

QString nowString()
{
    return QDateTime::currentDateTime().toString(Qt::ISODate);
}

bool isReimbursementPage(const QString &pageId)
{
    return pageId == QStringLiteral("reimbursement");
}

QStringList reimbursementAttachmentKeys()
{
    return {QStringLiteral("invoiceAttachment"), QStringLiteral("otherAttachments")};
}

QString attachmentDisplayName(const QString &reference)
{
    const QString fileName = QFileInfo(reference).fileName();
    const QRegularExpression archivedNamePattern(QStringLiteral("^[0-9a-fA-F]{32}_(.+)$"));
    const QRegularExpressionMatch match = archivedNamePattern.match(fileName);
    return match.hasMatch() ? match.captured(1) : fileName;
}

QStringList splitAttachmentPaths(const QVariant &value)
{
    QStringList paths;
    const QStringList rawParts = value.toString().split(QChar('\n'), Qt::SkipEmptyParts);
    for (const QString &part : rawParts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            paths.append(QDir::cleanPath(trimmed));
        }
    }
    return paths;
}

QString resolveAttachmentReference(const QString &storageRoot, const QString &path)
{
    const QFileInfo fileInfo(path);
    if (fileInfo.isAbsolute()) {
        return QDir::cleanPath(fileInfo.absoluteFilePath());
    }

    return QDir(storageRoot).filePath(path);
}

bool isPathUnderDirectory(const QString &filePath, const QString &directoryPath)
{
    const QString normalizedFile = QDir::cleanPath(filePath);
    const QString normalizedDirectory = QDir::cleanPath(QFileInfo(directoryPath).absoluteFilePath());
    return normalizedFile == normalizedDirectory
           || normalizedFile.startsWith(normalizedDirectory + QDir::separator());
}

void removeFilesAndEmptyParents(const QStringList &filePaths, const QString &stopDirectory)
{
    const QString normalizedStopDirectory = QDir::cleanPath(QFileInfo(stopDirectory).absoluteFilePath());
    for (const QString &filePath : filePaths) {
        QFile::remove(filePath);

        QDir dir(QFileInfo(filePath).absolutePath());
        while (dir.exists()) {
            const QString currentPath = QDir::cleanPath(dir.absolutePath());
            if (currentPath == normalizedStopDirectory) {
                dir.rmdir(QStringLiteral("."));
                break;
            }

            if (!dir.rmdir(QStringLiteral("."))) {
                break;
            }

            if (!dir.cdUp()) {
                break;
            }
        }
    }
}

QString inventoryOperationTypeText(InventoryOperationType operationType)
{
    switch (operationType) {
    case InventoryOperationType::DirectUpdate:
        return QStringLiteral("直接修改");
    case InventoryOperationType::StockIn:
        return QStringLiteral("入库");
    case InventoryOperationType::StockOut:
        return QStringLiteral("出库");
    case InventoryOperationType::Delete:
        return QStringLiteral("删除");
    }

    return QStringLiteral("未知");
}

QString inventoryInputTypeText(InventoryInputType inputType)
{
    switch (inputType) {
    case InventoryInputType::Manual:
        return QStringLiteral("手动录入");
    case InventoryInputType::Excel:
        return QStringLiteral("Excel 导入");
    case InventoryInputType::Scanner:
        return QStringLiteral("扫码枪");
    }

    return QStringLiteral("未知");
}

QString inventoryFulfillmentStatusText(InventoryFulfillmentStatus status)
{
    switch (status) {
    case InventoryFulfillmentStatus::Sufficient:
        return QStringLiteral("有该元件且充足");
    case InventoryFulfillmentStatus::Insufficient:
        return QStringLiteral("有该元件但数量不足");
    case InventoryFulfillmentStatus::Missing:
        return QStringLiteral("无该元件");
    }

    return QStringLiteral("未知");
}

QString inventoryBusinessKey(const QVariantMap &record)
{
    const QString manufacturerPart = record.value(QStringLiteral("manufacturerPart")).toString().trimmed();
    if (!manufacturerPart.isEmpty()) {
        return manufacturerPart;
    }

    const QString uniqueId = record.value(QStringLiteral("uniqueId")).toString().trimmed();
    if (!uniqueId.isEmpty()) {
        return uniqueId;
    }

    const QString manufacturer = record.value(QStringLiteral("manufacturer")).toString().trimmed();
    if (!manufacturerPart.isEmpty() && !manufacturer.isEmpty()) {
        return manufacturerPart + QStringLiteral(" @ ") + manufacturer;
    }

    return manufacturerPart;
}

QString variantText(const QVariant &value)
{
    if (value.type() == QVariant::StringList) {
        return value.toStringList().join(QStringLiteral("; "));
    }

    if (value.type() == QVariant::List) {
        QStringList parts;
        const QVariantList list = value.toList();
        parts.reserve(list.size());
        for (const QVariant &item : list) {
            parts.append(item.toString());
        }
        return parts.join(QStringLiteral("; "));
    }

    return value.toString();
}

QString normalizedFieldText(const QVariant &value)
{
    if (value.type() == QVariant::StringList || value.type() == QVariant::List) {
        return variantText(value).trimmed();
    }

    return value.toString().trimmed();
}

QString combineNotes(const QString &operationNote, const QString &entryNote)
{
    const QString trimmedOperationNote = operationNote.trimmed();
    const QString trimmedEntryNote = entryNote.trimmed();
    if (trimmedOperationNote.isEmpty()) {
        return trimmedEntryNote;
    }
    if (trimmedEntryNote.isEmpty() || trimmedEntryNote == trimmedOperationNote) {
        return trimmedOperationNote;
    }

    return QStringLiteral("操作备注：%1\n条目备注：%2").arg(trimmedOperationNote, trimmedEntryNote);
}

bool variantBoolValue(const QVariant &value)
{
    if (value.type() == QVariant::Bool) {
        return value.toBool();
    }

    const QString text = value.toString().trimmed().toLower();
    return text == QStringLiteral("true")
           || text == QStringLiteral("1")
           || text == QStringLiteral("yes")
           || text == QStringLiteral("y")
           || text == QStringLiteral("是")
           || text == QStringLiteral("已开票")
           || text == QStringLiteral("已报账");
}

QString normalizedHeaderKey(QString text)
{
    text = text.trimmed().toLower();
    QString normalized;
    normalized.reserve(text.size());
    for (const QChar ch : text) {
        if (ch.isLetterOrNumber() || ch.unicode() > 127) {
            normalized.append(ch);
        }
    }
    return normalized;
}

QStringList manufacturerPartAliases()
{
    return {
        QStringLiteral("Manufacturer Part"),
        QStringLiteral("ManufacturerPart"),
        QStringLiteral("Part Number"),
        QStringLiteral("PartNumber"),
        QStringLiteral("MPN"),
        QStringLiteral("厂家料号"),
        QStringLiteral("料号")
    };
}

QStringList quantityAliases()
{
    return {
        QStringLiteral("Quantity"),
        QStringLiteral("Qty"),
        QStringLiteral("数量"),
        QStringLiteral("用量")
    };
}

QString sourceRowsText(const QList<int> &sourceRows)
{
    QStringList values;
    values.reserve(sourceRows.size());
    for (const int sourceRow : sourceRows) {
        values.append(QString::number(sourceRow));
    }
    return values.join(QStringLiteral(", "));
}

QString inventoryFulfillmentSummary(const InventoryFulfillmentResult &result)
{
    QStringList parts;
    if (!result.uniqueId.trimmed().isEmpty()) {
        parts.append(result.uniqueId.trimmed());
    }
    if (!result.manufacturerPart.trimmed().isEmpty()) {
        parts.append(result.manufacturerPart.trimmed());
    }
    if (!result.manufacturer.trimmed().isEmpty()) {
        parts.append(result.manufacturer.trimmed());
    }
    if (!result.name.trimmed().isEmpty()) {
        parts.append(result.name.trimmed());
    }
    return parts.join(QStringLiteral(" / "));
}

int findHeaderIndex(const QStringList &headers, const QStringList &aliases)
{
    QSet<QString> normalizedAliases;
    for (const QString &alias : aliases) {
        normalizedAliases.insert(normalizedHeaderKey(alias));
    }

    for (int index = 0; index < headers.size(); ++index) {
        if (normalizedAliases.contains(normalizedHeaderKey(headers.at(index)))) {
            return index;
        }
    }
    return -1;
}

bool validateInventoryRequiredFields(const QVariantMap &record,
                                     QString *errorMessage,
                                     const QStringList &fieldKeys = {})
{
    static const QList<QPair<QString, QString>> requiredFields = {
        {QStringLiteral("manufacturerPart"), QStringLiteral("厂家料号")},
        {QStringLiteral("quantity"), QStringLiteral("当前库存数量")}
    };

    for (const auto &field : requiredFields) {
        if (!fieldKeys.isEmpty() && !fieldKeys.contains(field.first)) {
            continue;
        }

        if (!record.contains(field.first)) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("库存记录缺少必填字段：%1").arg(field.second);
            }
            return false;
        }

        if (normalizedFieldText(record.value(field.first)).isEmpty()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("库存记录必填字段不能为空：%1").arg(field.second);
            }
            return false;
        }
    }

    return true;
}

QStringList describeRecordChanges(const QVariantMap &beforeRecord, const QVariantMap &afterRecord)
{
    QSet<QString> keys;
    for (auto it = beforeRecord.constBegin(); it != beforeRecord.constEnd(); ++it) {
        keys.insert(it.key());
    }
    for (auto it = afterRecord.constBegin(); it != afterRecord.constEnd(); ++it) {
        keys.insert(it.key());
    }

    keys.remove(QStringLiteral("id"));
    keys.remove(QStringLiteral("createdAt"));
    keys.remove(QStringLiteral("updatedAt"));

    QStringList changedFields;
    const QList<QString> orderedKeys = keys.values();
    for (const QString &key : orderedKeys) {
        const QString beforeText = variantText(beforeRecord.value(key));
        const QString afterText = variantText(afterRecord.value(key));
        if (beforeText == afterText) {
            continue;
        }
        changedFields.append(QStringLiteral("%1: %2 -> %3").arg(key, beforeText, afterText));
    }

    changedFields.sort();
    return changedFields;
}

int countUniqueMatches(const QList<QVariantMap> &records,
                       const QString &key,
                       const QString &value,
                       int *matchedIndex)
{
    int count = 0;
    for (int index = 0; index < records.size(); ++index) {
        if (records.at(index).value(key).toString().trimmed() == value) {
            *matchedIndex = index;
            ++count;
        }
    }
    return count;
}

int findInventoryRecordIndex(const QList<QVariantMap> &records,
                             const QVariantMap &itemData,
                             QString *errorMessage)
{
    const QString recordId = itemData.value(QStringLiteral("id")).toString().trimmed();
    if (!recordId.isEmpty()) {
        for (int index = 0; index < records.size(); ++index) {
            if (records.at(index).value(QStringLiteral("id")).toString() == recordId) {
                return index;
            }
        }
        return -1;
    }

    const QString barcode = itemData.value(QStringLiteral("barcode")).toString().trimmed();
    if (!barcode.isEmpty()) {
        int matchedIndex = -1;
        const int count = countUniqueMatches(records,
                                             QStringLiteral("barcode"),
                                             barcode,
                                             &matchedIndex);
        if (count == 1) {
            return matchedIndex;
        }
        if (count > 1) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("条码匹配到多条库存记录：%1").arg(barcode);
            }
            return -2;
        }
    }

    const QString manufacturerPart = itemData.value(QStringLiteral("manufacturerPart")).toString().trimmed();
    if (!manufacturerPart.isEmpty()) {
        int matchedIndex = -1;
        const int count = countUniqueMatches(records,
                                             QStringLiteral("manufacturerPart"),
                                             manufacturerPart,
                                             &matchedIndex);
        if (count == 1) {
            return matchedIndex;
        }
        if (count > 1) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Manufacturer Part 匹配到多条库存记录：%1")
                                    .arg(manufacturerPart);
            }
            return -2;
        }
    }

    return -1;
}

void mergeProvidedFields(const QVariantMap &source, QVariantMap *target)
{
    for (auto it = source.constBegin(); it != source.constEnd(); ++it) {
        if (it.key() == QStringLiteral("id")
            || it.key() == QStringLiteral("createdAt")
            || it.key() == QStringLiteral("updatedAt")
            || it.key() == QStringLiteral("quantity")) {
            continue;
        }

        const QString text = it.value().toString().trimmed();
        if (!text.isEmpty() || it.value().type() == QVariant::StringList || it.value().type() == QVariant::List) {
            target->insert(it.key(), it.value());
        }
    }
}

QString nextInventoryNumber(const QList<QVariantMap> &records)
{
    int maxNumber = 0;
    for (const QVariantMap &record : records) {
        bool ok = false;
        const int current = record.value(QStringLiteral("number")).toString().toInt(&ok);
        if (ok) {
            maxNumber = qMax(maxNumber, current);
        }
    }
    return QString::number(maxNumber + 1);
}

QString excelSerialDateToIsoString(const QString &value)
{
    bool ok = false;
    const double serial = value.toDouble(&ok);
    if (!ok) {
        return {};
    }

    const int wholeDays = static_cast<int>(serial);
    if (wholeDays <= 0) {
        return {};
    }

    const QDate excelEpoch(1899, 12, 30);
    const QDate converted = excelEpoch.addDays(wholeDays);
    return converted.isValid() ? converted.toString(Qt::ISODate) : QString();
}

QVariant convertFieldValue(const FieldDefinition &field, const QString &rawValue)
{
    const QString trimmed = rawValue.trimmed();
    switch (field.type) {
    case FieldType::Integer:
        return trimmed.toInt();
    case FieldType::Double:
        return trimmed.toDouble();
    case FieldType::Date: {
        const QDate parsed = QDate::fromString(trimmed, Qt::ISODate);
        if (parsed.isValid()) {
            return parsed.toString(Qt::ISODate);
        }

        const QString excelDate = excelSerialDateToIsoString(trimmed);
        return !excelDate.isEmpty() ? excelDate : trimmed;
    }
    case FieldType::Boolean:
        return variantBoolValue(trimmed);
    case FieldType::Multiline:
    case FieldType::Choice:
    case FieldType::File:
    case FieldType::Files:
    case FieldType::Text:
    default:
        return trimmed;
    }
}
}

JsonStorageService::JsonStorageService() = default;

AuthenticationStatus JsonStorageService::authenticate(const QString &username,
                                                     const QString &password,
                                                     QString *errorMessage)
{
    if (!ensureUserStoreReady(errorMessage)) {
        m_currentUserName.clear();
        return AuthenticationStatus::Failed;
    }

    const QString trimmedUserName = username.trimmed();
    const QList<QVariantMap> users = loadPageRecords(QString::fromLatin1(kUsersPageId));
    for (const QVariantMap &user : users) {
        const QString storedUserName = user.value(QStringLiteral("username")).toString().trimmed();
        if (storedUserName != trimmedUserName) {
            continue;
        }

        if (user.value(QStringLiteral("password")).toString() != password) {
            m_currentUserName.clear();
            return AuthenticationStatus::WrongPassword;
        }

        m_currentUserName = storedUserName;
        return AuthenticationStatus::Success;
    }

    m_currentUserName.clear();
    return AuthenticationStatus::UserNotFound;
}

void JsonStorageService::setCurrentUserName(const QString &userName)
{
    m_currentUserName = userName.trimmed();
}

QString JsonStorageService::currentUserName() const
{
    return m_currentUserName;
}

bool JsonStorageService::loadCurrentUserSecurityInfo(UserSecurityInfo *info,
                                                     QString *errorMessage) const
{
    if (info == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("用户信息输出参数无效。");
        }
        return false;
    }

    QList<QVariantMap> users;
    if (!loadUsers(&users, errorMessage)) {
        return false;
    }

    const int index = indexOfUserByName(users, m_currentUserName);
    if (index < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("当前登录用户不存在。请重新登录。");
        }
        return false;
    }

    const QVariantMap &user = users.at(index);
    info->userName = user.value(QStringLiteral("username")).toString().trimmed();
    info->email = user.value(QStringLiteral("email")).toString().trimmed();
    return true;
}

bool JsonStorageService::sendCurrentUserVerificationCode(VerificationPurpose purpose,
                                                         const QString &email,
                                                         QString *errorMessage)
{
    QList<QVariantMap> users;
    if (!loadUsers(&users, errorMessage)) {
        return false;
    }

    const int index = indexOfUserByName(users, m_currentUserName);
    if (index < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("当前登录用户不存在。请重新登录。");
        }
        return false;
    }

    const QVariantMap &user = users.at(index);
    const QString storedEmail = normalizedEmail(user.value(QStringLiteral("email")).toString());
    QString targetEmail;

    if (purpose == VerificationPurpose::BindEmail) {
        if (!storedEmail.isEmpty()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("当前账号已绑定邮箱，如需变更请联系管理员处理。");
            }
            return false;
        }

        targetEmail = normalizedEmail(email);
        if (!isValidEmail(targetEmail)) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("邮箱格式无效，请重新输入。");
            }
            return false;
        }

        for (int userIndex = 0; userIndex < users.size(); ++userIndex) {
            if (userIndex == index) {
                continue;
            }

            if (normalizedEmail(users.at(userIndex).value(QStringLiteral("email")).toString()) == targetEmail) {
                if (errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("该邮箱已绑定到其他账号。");
                }
                return false;
            }
        }
    } else {
        if (storedEmail.isEmpty()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("当前账号尚未绑定邮箱，请先完成邮箱绑定。");
            }
            return false;
        }
        targetEmail = storedEmail;
    }

    if (!rateLimitAllowsSend(m_currentUserName, errorMessage)) {
        return false;
    }

    const QString verificationCode = generateVerificationCode();
    const QString subject = QStringLiteral("ManageSoft verification code");
    const QString body = QStringLiteral("用户名：%1\n用途：%2\n验证码：%3\n有效期：%4 分钟。\n如果这不是你的操作，请忽略此邮件。")
                             .arg(m_currentUserName,
                                  verificationPurposeText(purpose),
                                  verificationCode,
                                  QString::number(kVerificationCodeExpiryMinutes));

    if (!SmtpEmailClient::sendPlainTextMail(loadEmailSettings(), targetEmail, subject, body, errorMessage)) {
        return false;
    }

    const QString key = verificationStateKey(m_currentUserName, purpose);
    VerificationCodeState state;
    state.code = verificationCode;
    state.email = targetEmail;
    state.purpose = purpose;
    state.expiresAt = QDateTime::currentDateTime().addSecs(kVerificationCodeExpiryMinutes * 60);
    m_verificationCodes.insert(key, state);

    QList<QDateTime> history = m_verificationSendHistory.value(m_currentUserName);
    history.append(QDateTime::currentDateTime());
    m_verificationSendHistory.insert(m_currentUserName, history);
    return true;
}

bool JsonStorageService::bindCurrentUserEmail(const QString &email,
                                              const QString &verificationCode,
                                              QString *errorMessage)
{
    const QString targetEmail = normalizedEmail(email);
    if (!isValidEmail(targetEmail)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("邮箱格式无效，请重新输入。");
        }
        return false;
    }

    const QString code = verificationCode.trimmed();
    if (code.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("请输入验证码。");
        }
        return false;
    }

    QList<QVariantMap> users;
    if (!loadUsers(&users, errorMessage)) {
        return false;
    }

    const int index = indexOfUserByName(users, m_currentUserName);
    if (index < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("当前登录用户不存在。请重新登录。");
        }
        return false;
    }

    QVariantMap user = users.at(index);
    if (!normalizedEmail(user.value(QStringLiteral("email")).toString()).isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("当前账号已绑定邮箱，如需变更请联系管理员处理。");
        }
        return false;
    }

    const QString key = verificationStateKey(m_currentUserName, VerificationPurpose::BindEmail);
    const VerificationCodeState state = m_verificationCodes.value(key);
    if (state.code.isEmpty() || state.email != targetEmail) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("验证码无效或与当前邮箱不匹配，请重新获取。");
        }
        return false;
    }

    if (state.expiresAt < QDateTime::currentDateTime()) {
        m_verificationCodes.remove(key);
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("验证码已过期，请重新获取。");
        }
        return false;
    }

    if (state.code != code) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("验证码错误。");
        }
        return false;
    }

    for (int userIndex = 0; userIndex < users.size(); ++userIndex) {
        if (userIndex == index) {
            continue;
        }

        if (normalizedEmail(users.at(userIndex).value(QStringLiteral("email")).toString()) == targetEmail) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("该邮箱已绑定到其他账号。");
            }
            return false;
        }
    }

    user.insert(QStringLiteral("email"), targetEmail);
    user.insert(QStringLiteral("updatedAt"), nowString());
    users[index] = user;
    m_verificationCodes.remove(key);
    return saveUsers(users, errorMessage);
}

bool JsonStorageService::changeCurrentUserPassword(const QString &newPassword,
                                                  const QString &verificationCode,
                                                  QString *errorMessage)
{
    if (newPassword.size() < 8) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("新密码至少需要 8 位。");
        }
        return false;
    }

    const QString code = verificationCode.trimmed();
    if (code.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("请输入验证码。");
        }
        return false;
    }

    QList<QVariantMap> users;
    if (!loadUsers(&users, errorMessage)) {
        return false;
    }

    const int index = indexOfUserByName(users, m_currentUserName);
    if (index < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("当前登录用户不存在。请重新登录。");
        }
        return false;
    }

    QVariantMap user = users.at(index);
    const QString targetEmail = normalizedEmail(user.value(QStringLiteral("email")).toString());
    if (targetEmail.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("当前账号尚未绑定邮箱，请先完成邮箱绑定。");
        }
        return false;
    }

    const QString key = verificationStateKey(m_currentUserName, VerificationPurpose::ChangePassword);
    const VerificationCodeState state = m_verificationCodes.value(key);
    if (state.code.isEmpty() || state.email != targetEmail) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("验证码无效，请重新获取。");
        }
        return false;
    }

    if (state.expiresAt < QDateTime::currentDateTime()) {
        m_verificationCodes.remove(key);
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("验证码已过期，请重新获取。");
        }
        return false;
    }

    if (state.code != code) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("验证码错误。");
        }
        return false;
    }

    user.insert(QStringLiteral("password"), newPassword);
    user.insert(QStringLiteral("updatedAt"), nowString());
    users[index] = user;
    m_verificationCodes.remove(key);
    return saveUsers(users, errorMessage);
}

QString JsonStorageService::storageRoot() const
{
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(baseDir).filePath("data");
}

QList<QVariantMap> JsonStorageService::loadPageRecords(const QString &pageId) const
{
    QString errorMessage;
    if (!ensureStorageReady(&errorMessage)) {
        return {};
    }

    QFile file(pageFilePath(pageId));
    if (!file.exists()) {
        return {};
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isArray()) {
        return {};
    }

    QList<QVariantMap> records;
    const QJsonArray array = document.array();
    records.reserve(array.size());
    for (const QJsonValue &value : array) {
        if (value.isObject()) {
            records.append(jsonObjectToMap(value.toObject()));
        }
    }

    return records;
}

bool JsonStorageService::upsertRecord(const QString &pageId, QVariantMap record, QString *errorMessage) const
{
    if (pageId == QStringLiteral("inventory")
        && !validateInventoryRequiredFields(record, errorMessage)) {
        return false;
    }

    QList<QVariantMap> records = loadPageRecords(pageId);
    const QString recordId = record.value("id").toString();
    if (pageId == QStringLiteral("inventory")) {
        const QString manufacturerPart = record.value(QStringLiteral("manufacturerPart")).toString().trimmed();
        const QString barcode = record.value(QStringLiteral("barcode")).toString().trimmed();
        for (const QVariantMap &existingRecord : records) {
            if (existingRecord.value(QStringLiteral("id")).toString() == recordId) {
                continue;
            }

            if (existingRecord.value(QStringLiteral("manufacturerPart")).toString().trimmed() == manufacturerPart) {
                if (errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("厂家料号已存在，当前系统要求 Manufacturer Part 全局唯一：%1")
                                        .arg(manufacturerPart);
                }
                return false;
            }

            if (!barcode.isEmpty()
                && existingRecord.value(QStringLiteral("barcode")).toString().trimmed() == barcode) {
                if (errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("条码已绑定其它库存物料：%1").arg(barcode);
                }
                return false;
            }
        }
    }

    const QString timestamp = nowString();
    bool updated = false;
    QVariantMap existingRecord;

    if (recordId.isEmpty()) {
        record.insert("id", QUuid::createUuid().toString(QUuid::WithoutBraces));
        record.insert("createdAt", timestamp);

        if (isReimbursementPage(pageId)
            && record.value(QStringLiteral("reimbursementOwner")).toString().trimmed().isEmpty()
            && !m_currentUserName.trimmed().isEmpty()) {
            record.insert(QStringLiteral("reimbursementOwner"), m_currentUserName);
        }
    }
    record.insert("updatedAt", timestamp);

    for (const QVariantMap &candidateRecord : records) {
        if (candidateRecord.value("id").toString() == record.value("id").toString()) {
            existingRecord = candidateRecord;
            break;
        }
    }

    QStringList obsoleteFiles;
    QStringList createdFiles;
    if (isReimbursementPage(pageId)
        && !archiveReimbursementAttachments(&record, existingRecord, &obsoleteFiles, &createdFiles, errorMessage)) {
        return false;
    }

    for (QVariantMap &existingRecord : records) {
        if (existingRecord.value("id").toString() == record.value("id").toString()) {
            record.insert("createdAt", existingRecord.value("createdAt", timestamp).toString());
            existingRecord = record;
            updated = true;
            break;
        }
    }

    if (!updated) {
        records.append(record);
    }

    if (!savePageRecords(pageId, records, errorMessage)) {
        removeFilesAndEmptyParents(createdFiles, QDir(storageRoot()).filePath(QStringLiteral("attachments")));
        return false;
    }

    if (isReimbursementPage(pageId) && !obsoleteFiles.isEmpty()) {
        removeFilesAndEmptyParents(obsoleteFiles, QDir(storageRoot()).filePath(QStringLiteral("attachments")));
    }

    return true;
}

bool JsonStorageService::removeRecord(const QString &pageId, const QString &id, QString *errorMessage) const
{
    QList<QVariantMap> records = loadPageRecords(pageId);
    const qsizetype originalSize = records.size();
    QVariantMap removedRecord;

    records.erase(std::remove_if(records.begin(), records.end(), [&id, &removedRecord](const QVariantMap &record) {
        if (record.value("id").toString() == id) {
            removedRecord = record;
            return true;
        }
        return false;
    }), records.end());

    if (records.size() == originalSize) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("未找到要删除的记录。");
        }
        return false;
    }

    if (!savePageRecords(pageId, records, errorMessage)) {
        return false;
    }

    if (isReimbursementPage(pageId)) {
        QStringList filesToRemove;
        const QString attachmentDirectory = recordAttachmentDirectory(pageId, id);
        for (const QString &key : reimbursementAttachmentKeys()) {
            for (const QString &path : splitAttachmentPaths(removedRecord.value(key))) {
                const QString resolvedPath = resolveAttachmentReference(storageRoot(), path);
                if (isPathUnderDirectory(resolvedPath, attachmentDirectory)) {
                    filesToRemove.append(resolvedPath);
                }
            }
        }

        removeFilesAndEmptyParents(filesToRemove, attachmentsRootDirectory());
    }

    return true;
}

bool JsonStorageService::deleteInventoryRecord(const QString &id,
                                              const QString &note,
                                              QString *errorMessage) const
{
    QList<QVariantMap> records = loadPageRecords(QStringLiteral("inventory"));
    const int matchedIndex = std::find_if(records.cbegin(),
                                          records.cend(),
                                          [&id](const QVariantMap &record) {
                                              return record.value(QStringLiteral("id")).toString() == id;
                                          }) - records.cbegin();
    if (matchedIndex < 0 || matchedIndex >= records.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("未找到要删除的库存记录。");
        }
        return false;
    }

    const QVariantMap beforeRecord = records.at(matchedIndex);
    const QString timestamp = nowString();
    QList<QVariantMap> historyRecords = loadInventoryHistory();
    const int beforeQuantity = beforeRecord.value(QStringLiteral("quantity")).toInt();

    records.removeAt(matchedIndex);
    if (!savePageRecords(QStringLiteral("inventory"), records, errorMessage)) {
        return false;
    }

    QVariantMap historyRecord;
    historyRecord.insert(QStringLiteral("historyId"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    historyRecord.insert(QStringLiteral("itemId"), beforeRecord.value(QStringLiteral("id")).toString());
    historyRecord.insert(QStringLiteral("itemKey"), inventoryBusinessKey(beforeRecord));
    historyRecord.insert(QStringLiteral("operationType"), inventoryOperationTypeText(InventoryOperationType::Delete));
    historyRecord.insert(QStringLiteral("inputType"), inventoryInputTypeText(InventoryInputType::Manual));
    historyRecord.insert(QStringLiteral("changeTime"), timestamp);
    historyRecord.insert(QStringLiteral("beforeQuantity"), beforeQuantity);
    historyRecord.insert(QStringLiteral("changeQuantity"), -beforeQuantity);
    historyRecord.insert(QStringLiteral("afterQuantity"), 0);
    historyRecord.insert(QStringLiteral("changedFields"), QStringList{QStringLiteral("记录状态: 保留 -> 已删除")});
    historyRecord.insert(QStringLiteral("note"), note.trimmed());
    historyRecord.insert(QStringLiteral("modifier"), m_currentUserName);
    historyRecord.insert(QStringLiteral("sourceFile"), QString());
    historyRecord.insert(QStringLiteral("sourceRow"), 0);
    historyRecord.insert(QStringLiteral("createdAt"), timestamp);
    historyRecords.append(historyRecord);
    return saveInventoryHistory(historyRecords, errorMessage);
}

bool JsonStorageService::importExcel(const QString &pageId,
                                     const QList<FieldDefinition> &fields,
                                     const QString &filePath,
                                     QString *errorMessage) const
{
    XlsxSheetData sheetData;
    if (!SimpleXlsxDocument::readSheet(filePath, &sheetData, errorMessage)) {
        return false;
    }

    if (sheetData.headers.isEmpty() || sheetData.rows.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("导入文件没有可用的数据行。");
        }
        return false;
    }

    QList<QVariantMap> records = loadPageRecords(pageId);
    int importedCount = 0;

    for (const QStringList &values : sheetData.rows) {
        QVariantMap record;
        for (int column = 0; column < sheetData.headers.size(); ++column) {
            const QString header = sheetData.headers.at(column).trimmed();
            const QString rawValue = column < values.size() ? values.at(column) : QString();
            for (const FieldDefinition &field : fields) {
                if (field.key == header || field.label == header) {
                    record.insert(field.key, convertFieldValue(field, rawValue));
                    break;
                }
            }
        }

        bool hasValue = false;
        for (const FieldDefinition &field : fields) {
            if (!record.value(field.key).toString().trimmed().isEmpty()) {
                hasValue = true;
                break;
            }
        }

        if (!hasValue) {
            continue;
        }

        record.insert("id", QUuid::createUuid().toString(QUuid::WithoutBraces));
        const QString timestamp = nowString();
        record.insert("createdAt", timestamp);
        record.insert("updatedAt", timestamp);
        records.append(record);
        ++importedCount;
    }

    if (importedCount == 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Excel 文件中没有匹配当前表头格式的数据。");
        }
        return false;
    }

    return savePageRecords(pageId, records, errorMessage);
}

bool JsonStorageService::exportExcel(const QString &pageId,
                                     const QList<FieldDefinition> &fields,
                                     const QString &sheetName,
                                     const QString &filePath,
                                     QString *errorMessage) const
{
    QStringList headers;
    for (const FieldDefinition &field : fields) {
        headers.append(field.label);
    }

    QList<QList<QVariant>> rows;
    const QList<QVariantMap> records = loadPageRecords(pageId);
    rows.reserve(records.size());
    for (const QVariantMap &record : records) {
        QList<QVariant> row;
        for (const FieldDefinition &field : fields) {
            row.append(record.value(field.key));
        }
        rows.append(row);
    }

    return SimpleXlsxDocument::writeSheet(filePath, sheetName, headers, rows, errorMessage);
}

QList<QVariantMap> JsonStorageService::loadInventoryHistory() const
{
    return loadPageRecords(QStringLiteral("inventory_history"));
}

bool JsonStorageService::applyInventoryChange(const QVariantMap &itemData,
                                              InventoryOperationType operationType,
                                              InventoryInputType inputType,
                                              const QString &note,
                                              const QString &sourceFile,
                                              int sourceRow,
                                              QString *errorMessage) const
{
    QList<QVariantMap> records = loadPageRecords(QStringLiteral("inventory"));
    QList<QVariantMap> historyRecords = loadInventoryHistory();

    QString matchError;
    const int matchedIndex = findInventoryRecordIndex(records, itemData, &matchError);
    if (matchedIndex == -2) {
        if (errorMessage != nullptr) {
            *errorMessage = matchError;
        }
        return false;
    }

    const QString timestamp = nowString();
    const QString businessDate = itemData.value(QStringLiteral("date")).toString().trimmed().isEmpty()
                                     ? QDate::currentDate().toString(Qt::ISODate)
                                     : itemData.value(QStringLiteral("date")).toString();

    QVariantMap beforeRecord;
    QVariantMap afterRecord;
    int beforeQuantity = 0;
    int afterQuantity = 0;
    int changeQuantity = 0;

    switch (operationType) {
    case InventoryOperationType::DirectUpdate: {
        if (matchedIndex < 0) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("直接修改必须先匹配到已有库存记录。");
            }
            return false;
        }

        beforeRecord = records.at(matchedIndex);
        afterRecord = beforeRecord;
        for (auto it = itemData.constBegin(); it != itemData.constEnd(); ++it) {
            if (it.key() == QStringLiteral("id")
                || it.key() == QStringLiteral("createdAt")
                || it.key() == QStringLiteral("updatedAt")) {
                continue;
            }
            afterRecord.insert(it.key(), it.value());
        }

        beforeQuantity = beforeRecord.value(QStringLiteral("quantity")).toInt();
        afterQuantity = afterRecord.value(QStringLiteral("quantity")).toInt();
        changeQuantity = afterQuantity - beforeQuantity;
        afterRecord.insert(QStringLiteral("date"), businessDate);
        afterRecord.insert(QStringLiteral("updatedAt"), timestamp);
        if (!validateInventoryRequiredFields(afterRecord, errorMessage)) {
            return false;
        }
        records[matchedIndex] = afterRecord;
        break;
    }
    case InventoryOperationType::StockIn: {
        changeQuantity = itemData.value(QStringLiteral("quantity")).toInt();
        if (changeQuantity <= 0) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("入库数量必须大于 0。");
            }
            return false;
        }

        if (matchedIndex >= 0) {
            beforeRecord = records.at(matchedIndex);
            afterRecord = beforeRecord;
            mergeProvidedFields(itemData, &afterRecord);
            beforeQuantity = beforeRecord.value(QStringLiteral("quantity")).toInt();
            afterQuantity = beforeQuantity + changeQuantity;
            afterRecord.insert(QStringLiteral("quantity"), afterQuantity);
            afterRecord.insert(QStringLiteral("date"), businessDate);
            afterRecord.insert(QStringLiteral("updatedAt"), timestamp);
            if (!validateInventoryRequiredFields(afterRecord, errorMessage)) {
                return false;
            }
            records[matchedIndex] = afterRecord;
        } else {
            const QString manufacturerPart = itemData.value(QStringLiteral("manufacturerPart")).toString().trimmed();
            if (manufacturerPart.isEmpty()) {
                if (errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("新元件入库至少需要 Manufacturer Part。");
                }
                return false;
            }

            beforeRecord = {};
            afterRecord = itemData;
            beforeQuantity = 0;
            afterQuantity = changeQuantity;
            afterRecord.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
            afterRecord.insert(QStringLiteral("quantity"), afterQuantity);
            afterRecord.insert(QStringLiteral("number"), nextInventoryNumber(records));
            afterRecord.insert(QStringLiteral("date"), businessDate);
            afterRecord.insert(QStringLiteral("createdAt"), timestamp);
            afterRecord.insert(QStringLiteral("updatedAt"), timestamp);
            if (!validateInventoryRequiredFields(afterRecord, errorMessage)) {
                return false;
            }
            records.append(afterRecord);
        }
        break;
    }
    case InventoryOperationType::StockOut: {
        changeQuantity = itemData.value(QStringLiteral("quantity")).toInt();
        if (changeQuantity <= 0) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("出库数量必须大于 0。");
            }
            return false;
        }
        if (matchedIndex < 0) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("出库必须先匹配到已有库存记录。");
            }
            return false;
        }

        beforeRecord = records.at(matchedIndex);
        afterRecord = beforeRecord;
        mergeProvidedFields(itemData, &afterRecord);
        beforeQuantity = beforeRecord.value(QStringLiteral("quantity")).toInt();
        afterQuantity = beforeQuantity - changeQuantity;
        if (afterQuantity < 0) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("出库后库存不能小于 0。");
            }
            return false;
        }

        afterRecord.insert(QStringLiteral("quantity"), afterQuantity);
        afterRecord.insert(QStringLiteral("date"), businessDate);
        afterRecord.insert(QStringLiteral("updatedAt"), timestamp);
        if (!validateInventoryRequiredFields(afterRecord, errorMessage)) {
            return false;
        }
        records[matchedIndex] = afterRecord;
        changeQuantity = -changeQuantity;
        break;
    }
    }

    const QStringList changedFields = describeRecordChanges(beforeRecord, afterRecord);

    QVariantMap historyRecord;
    historyRecord.insert(QStringLiteral("historyId"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    historyRecord.insert(QStringLiteral("itemId"), afterRecord.value(QStringLiteral("id")).toString());
    historyRecord.insert(QStringLiteral("itemKey"), inventoryBusinessKey(afterRecord));
    historyRecord.insert(QStringLiteral("operationType"), inventoryOperationTypeText(operationType));
    historyRecord.insert(QStringLiteral("inputType"), inventoryInputTypeText(inputType));
    historyRecord.insert(QStringLiteral("changeTime"), timestamp);
    historyRecord.insert(QStringLiteral("beforeQuantity"), beforeQuantity);
    historyRecord.insert(QStringLiteral("changeQuantity"), changeQuantity);
    historyRecord.insert(QStringLiteral("afterQuantity"), afterQuantity);
    historyRecord.insert(QStringLiteral("changedFields"), changedFields);
    historyRecord.insert(QStringLiteral("note"), note.trimmed());
    historyRecord.insert(QStringLiteral("modifier"), m_currentUserName);
    historyRecord.insert(QStringLiteral("sourceFile"), sourceFile);
    historyRecord.insert(QStringLiteral("sourceRow"), sourceRow);
    historyRecord.insert(QStringLiteral("createdAt"), timestamp);
    historyRecords.append(historyRecord);

    if (!savePageRecords(QStringLiteral("inventory"), records, errorMessage)) {
        return false;
    }

    return saveInventoryHistory(historyRecords, errorMessage);
}

bool JsonStorageService::importInventoryExcel(const QList<FieldDefinition> &fields,
                                             const QString &filePath,
                                             InventoryOperationType operationType,
                                             const QString &note,
                                             QString *errorMessage) const
{
    XlsxSheetData sheetData;
    if (!SimpleXlsxDocument::readSheet(filePath, &sheetData, errorMessage)) {
        return false;
    }

    if (sheetData.headers.isEmpty() || sheetData.rows.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("导入文件没有可用的数据行。");
        }
        return false;
    }

    int successCount = 0;
    QStringList failures;
    const QString sourceFile = QFileInfo(filePath).fileName();

    for (int rowIndex = 0; rowIndex < sheetData.rows.size(); ++rowIndex) {
        const QStringList &values = sheetData.rows.at(rowIndex);
        QVariantMap record;
        for (int column = 0; column < sheetData.headers.size(); ++column) {
            const QString header = sheetData.headers.at(column).trimmed();
            const QString rawValue = column < values.size() ? values.at(column) : QString();
            for (const FieldDefinition &field : fields) {
                if (field.key == header || field.label == header) {
                    record.insert(field.key, convertFieldValue(field, rawValue));
                    break;
                }
            }
        }

        bool hasValue = false;
        for (const FieldDefinition &field : fields) {
            if (!record.value(field.key).toString().trimmed().isEmpty()) {
                hasValue = true;
                break;
            }
        }
        if (!hasValue) {
            continue;
        }

        QString rowError;
        if (applyInventoryChange(record,
                                 operationType,
                                 InventoryInputType::Excel,
                     combineNotes(note, record.value(QStringLiteral("comment")).toString()),
                                 sourceFile,
                                 rowIndex + 2,
                                 &rowError)) {
            ++successCount;
        } else {
            failures.append(QStringLiteral("第 %1 行：%2").arg(rowIndex + 2).arg(rowError));
        }
    }

    if (successCount == 0) {
        if (errorMessage != nullptr) {
            *errorMessage = failures.isEmpty()
                                ? QStringLiteral("Excel 文件中没有可导入的数据。")
                                : failures.join(QStringLiteral("\n"));
        }
        return false;
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("成功 %1 行，失败 %2 行。")
                            .arg(successCount)
                            .arg(failures.size());
        if (!failures.isEmpty()) {
            *errorMessage += QStringLiteral("\n\n") + failures.join(QStringLiteral("\n"));
        }
    }

    return successCount > 0;
}

bool JsonStorageService::importInventoryBom(const QString &filePath,
                                           InventoryOperationType operationType,
                                           const QString &note,
                                           QString *errorMessage) const
{
    XlsxSheetData sheetData;
    if (!SimpleXlsxDocument::readSheet(filePath, &sheetData, errorMessage)) {
        return false;
    }

    if (sheetData.headers.isEmpty() || sheetData.rows.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("导入文件没有可用的数据行。");
        }
        return false;
    }

    const int manufacturerPartColumn = findHeaderIndex(sheetData.headers, manufacturerPartAliases());
    const int quantityColumn = findHeaderIndex(sheetData.headers, quantityAliases());

    if (manufacturerPartColumn < 0 || quantityColumn < 0) {
        if (errorMessage != nullptr) {
            QStringList missingHeaders;
            if (manufacturerPartColumn < 0) {
                missingHeaders.append(QStringLiteral("Manufacturer Part"));
            }
            if (quantityColumn < 0) {
                missingHeaders.append(QStringLiteral("Quantity"));
            }
            *errorMessage = QStringLiteral("BOM 缺少关键列：%1。当前 BOM 出入库仅识别 Manufacturer Part 和 Quantity 两列。")
                                .arg(missingHeaders.join(QStringLiteral("、")));
        }
        return false;
    }

    int successCount = 0;
    QStringList failures;
    const QString sourceFile = QFileInfo(filePath).fileName();

    for (int rowIndex = 0; rowIndex < sheetData.rows.size(); ++rowIndex) {
        const QStringList &values = sheetData.rows.at(rowIndex);
        const QString manufacturerPart = manufacturerPartColumn < values.size()
                                             ? values.at(manufacturerPartColumn).trimmed()
                                             : QString();
        const QString rawQuantity = quantityColumn < values.size()
                                        ? values.at(quantityColumn).trimmed()
                                        : QString();

        if (manufacturerPart.isEmpty() && rawQuantity.isEmpty()) {
            continue;
        }

        bool ok = false;
        const int quantity = rawQuantity.toInt(&ok);
        if (manufacturerPart.isEmpty()) {
            failures.append(QStringLiteral("第 %1 行：Manufacturer Part 不能为空。").arg(rowIndex + 2));
            continue;
        }
        if (!ok || quantity <= 0) {
            failures.append(QStringLiteral("第 %1 行：Quantity 必须是大于 0 的整数。").arg(rowIndex + 2));
            continue;
        }

        QVariantMap record;
        record.insert(QStringLiteral("manufacturerPart"), manufacturerPart);
        record.insert(QStringLiteral("quantity"), quantity);

        QString rowError;
        if (applyInventoryChange(record,
                                 operationType,
                                 InventoryInputType::Excel,
                                 combineNotes(note, QStringLiteral("BOM 导入")),
                                 sourceFile,
                                 rowIndex + 2,
                                 &rowError)) {
            ++successCount;
        } else {
            failures.append(QStringLiteral("第 %1 行：%2").arg(rowIndex + 2).arg(rowError));
        }
    }

    if (successCount == 0) {
        if (errorMessage != nullptr) {
            *errorMessage = failures.isEmpty()
                                ? QStringLiteral("BOM 文件中没有可导入的数据。")
                                : failures.join(QStringLiteral("\n"));
        }
        return false;
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("成功 %1 行，失败 %2 行。").arg(successCount).arg(failures.size());
        if (!failures.isEmpty()) {
            *errorMessage += QStringLiteral("\n\n") + failures.join(QStringLiteral("\n"));
        }
    }

    return true;
}

bool JsonStorageService::analyzeInventoryFulfillment(const QString &filePath,
                                                    QList<InventoryFulfillmentResult> *results,
                                                    QString *errorMessage) const
{
    if (results == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("配单结果输出参数不能为空。");
        }
        return false;
    }

    XlsxSheetData sheetData;
    if (!SimpleXlsxDocument::readSheet(filePath, &sheetData, errorMessage)) {
        return false;
    }

    if (sheetData.headers.isEmpty() || sheetData.rows.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("导入文件没有可用的数据行。");
        }
        return false;
    }

    const int manufacturerPartColumn = findHeaderIndex(sheetData.headers, manufacturerPartAliases());
    const int quantityColumn = findHeaderIndex(sheetData.headers, quantityAliases());
    if (manufacturerPartColumn < 0 || quantityColumn < 0) {
        if (errorMessage != nullptr) {
            QStringList missingHeaders;
            if (manufacturerPartColumn < 0) {
                missingHeaders.append(QStringLiteral("Manufacturer Part"));
            }
            if (quantityColumn < 0) {
                missingHeaders.append(QStringLiteral("Quantity"));
            }
            *errorMessage = QStringLiteral("配单清单缺少关键列：%1。当前配单仅识别 Manufacturer Part 和 Quantity 两列。")
                                .arg(missingHeaders.join(QStringLiteral("、")));
        }
        return false;
    }

    struct DemandAggregate {
        int requiredQuantity = 0;
        QList<int> sourceRows;
    };

    QMap<QString, DemandAggregate> aggregatedDemand;
    QStringList orderedManufacturerParts;
    QStringList failures;

    for (int rowIndex = 0; rowIndex < sheetData.rows.size(); ++rowIndex) {
        const QStringList &values = sheetData.rows.at(rowIndex);
        const QString manufacturerPart = manufacturerPartColumn < values.size()
                                             ? values.at(manufacturerPartColumn).trimmed()
                                             : QString();
        const QString rawQuantity = quantityColumn < values.size()
                                        ? values.at(quantityColumn).trimmed()
                                        : QString();

        if (manufacturerPart.isEmpty() && rawQuantity.isEmpty()) {
            continue;
        }

        bool ok = false;
        const int quantity = rawQuantity.toInt(&ok);
        if (manufacturerPart.isEmpty()) {
            failures.append(QStringLiteral("第 %1 行：Manufacturer Part 不能为空。").arg(rowIndex + 2));
            continue;
        }
        if (!ok || quantity <= 0) {
            failures.append(QStringLiteral("第 %1 行：Quantity 必须是大于 0 的整数。").arg(rowIndex + 2));
            continue;
        }

        if (!aggregatedDemand.contains(manufacturerPart)) {
            orderedManufacturerParts.append(manufacturerPart);
        }
        DemandAggregate aggregate = aggregatedDemand.value(manufacturerPart);
        aggregate.requiredQuantity += quantity;
        aggregate.sourceRows.append(rowIndex + 2);
        aggregatedDemand.insert(manufacturerPart, aggregate);
    }

    if (aggregatedDemand.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = failures.isEmpty()
                                ? QStringLiteral("配单清单中没有可识别的数据。")
                                : failures.join(QStringLiteral("\n"));
        }
        return false;
    }

    const QList<QVariantMap> inventoryRecords = loadPageRecords(QStringLiteral("inventory"));
    QList<InventoryFulfillmentResult> analyzedResults;
    analyzedResults.reserve(orderedManufacturerParts.size());

    int sufficientCount = 0;
    int insufficientCount = 0;
    int missingCount = 0;
    const QString sourceFile = QFileInfo(filePath).fileName();

    for (const QString &manufacturerPart : orderedManufacturerParts) {
        const DemandAggregate aggregate = aggregatedDemand.value(manufacturerPart);

        InventoryFulfillmentResult result;
        result.manufacturerPart = manufacturerPart;
        result.requiredQuantity = aggregate.requiredQuantity;
        result.sourceRows = aggregate.sourceRows;
        result.sourceFile = sourceFile;

        int matchedIndex = -1;
        const int matchCount = countUniqueMatches(inventoryRecords,
                                                  QStringLiteral("manufacturerPart"),
                                                  manufacturerPart,
                                                  &matchedIndex);
        if (matchCount == 1 && matchedIndex >= 0) {
            const QVariantMap matchedRecord = inventoryRecords.at(matchedIndex);
            result.itemId = matchedRecord.value(QStringLiteral("id")).toString();
            result.availableQuantity = matchedRecord.value(QStringLiteral("quantity")).toInt();
            result.manufacturer = matchedRecord.value(QStringLiteral("manufacturer")).toString();
            result.name = matchedRecord.value(QStringLiteral("name")).toString();
            result.uniqueId = matchedRecord.value(QStringLiteral("uniqueId")).toString();
            result.unit = matchedRecord.value(QStringLiteral("unit")).toString();
            result.location = matchedRecord.value(QStringLiteral("location")).toString();
            if (result.availableQuantity >= result.requiredQuantity) {
                result.status = InventoryFulfillmentStatus::Sufficient;
                ++sufficientCount;
            } else {
                result.status = InventoryFulfillmentStatus::Insufficient;
                ++insufficientCount;
            }
        } else {
            result.status = InventoryFulfillmentStatus::Missing;
            ++missingCount;
        }

        analyzedResults.append(result);
    }

    *results = analyzedResults;

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("共 %1 种元件：充足 %2 种，数量不足 %3 种，无料 %4 种。")
                            .arg(analyzedResults.size())
                            .arg(sufficientCount)
                            .arg(insufficientCount)
                            .arg(missingCount);
        if (!failures.isEmpty()) {
            *errorMessage += QStringLiteral("\n\n以下行已忽略：\n") + failures.join(QStringLiteral("\n"));
        }
    }

    return true;
}

bool JsonStorageService::enrichInventoryRecord(const QString &manufacturerPart,
                                               const QVariantMap &currentRecord,
                                               const QStringList &desiredFieldKeys,
                                               InventoryEnrichmentResult *result,
                                               QString *errorMessage) const
{
    return AiInventoryEnricher::enrich(manufacturerPart,
                                       currentRecord,
                                       loadPageRecords(QStringLiteral("inventory")),
                                       desiredFieldKeys,
                                       result,
                                       errorMessage);
}

bool JsonStorageService::exportInventoryFulfillment(const QList<InventoryFulfillmentResult> &results,
                                                   const QString &filePath,
                                                   QString *errorMessage) const
{
    if (results.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("当前没有可导出的配单结果。");
        }
        return false;
    }

    const QStringList headers = {
        QStringLiteral("配单状态"),
        QStringLiteral("Manufacturer Part"),
        QStringLiteral("需求数量"),
        QStringLiteral("可用库存"),
        QStringLiteral("缺口数量"),
        QStringLiteral("匹配库存物料"),
        QStringLiteral("Manufacturer"),
        QStringLiteral("Name"),
        QStringLiteral("单位"),
        QStringLiteral("存储位置"),
        QStringLiteral("来源文件"),
        QStringLiteral("来源行")
    };

    QList<QList<QVariant>> rows;
    rows.reserve(results.size());
    for (const InventoryFulfillmentResult &result : results) {
        QList<QVariant> row;
        row.append(inventoryFulfillmentStatusText(result.status));
        row.append(result.manufacturerPart);
        row.append(result.requiredQuantity);
        row.append(result.availableQuantity);
        row.append(qMax(0, result.requiredQuantity - result.availableQuantity));
        row.append(inventoryFulfillmentSummary(result));
        row.append(result.manufacturer);
        row.append(result.name);
        row.append(result.unit);
        row.append(result.location);
        row.append(result.sourceFile);
        row.append(sourceRowsText(result.sourceRows));
        rows.append(row);
    }

    return SimpleXlsxDocument::writeSheet(filePath,
                                          QStringLiteral("配单结果"),
                                          headers,
                                          rows,
                                          errorMessage);
}

bool JsonStorageService::applyInventoryFulfillment(const QList<InventoryFulfillmentResult> &results,
                                                  QString *errorMessage) const
{
    if (results.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("请先选择需要出库的元件。");
        }
        return false;
    }

    int successCount = 0;
    QStringList failures;
    for (const InventoryFulfillmentResult &result : results) {
        if (result.status != InventoryFulfillmentStatus::Sufficient) {
            failures.append(QStringLiteral("%1：当前状态不支持一键出库。").arg(result.manufacturerPart));
            continue;
        }

        QVariantMap changeRecord;
        if (!result.itemId.trimmed().isEmpty()) {
            changeRecord.insert(QStringLiteral("id"), result.itemId);
        } else {
            changeRecord.insert(QStringLiteral("manufacturerPart"), result.manufacturerPart);
        }
        changeRecord.insert(QStringLiteral("quantity"), result.requiredQuantity);

        QString note = QStringLiteral("配单出库");
        const QString rowsText = sourceRowsText(result.sourceRows);
        if (!rowsText.isEmpty()) {
            note += QStringLiteral("，来源行：%1").arg(rowsText);
        }

        QString rowError;
        if (applyInventoryChange(changeRecord,
                                 InventoryOperationType::StockOut,
                                 InventoryInputType::Excel,
                                 note,
                                 result.sourceFile,
                                 result.sourceRows.size() == 1 ? result.sourceRows.first() : 0,
                                 &rowError)) {
            ++successCount;
        } else {
            failures.append(QStringLiteral("%1：%2").arg(result.manufacturerPart, rowError));
        }
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("成功出库 %1 种，失败 %2 种。")
                            .arg(successCount)
                            .arg(failures.size());
        if (!failures.isEmpty()) {
            *errorMessage += QStringLiteral("\n\n") + failures.join(QStringLiteral("\n"));
        }
    }

    return successCount > 0;
}

bool JsonStorageService::loadReimbursementAttachments(const QVariantMap &record,
                                                     QList<ReimbursementAttachmentContent> *attachments,
                                                     QString *errorMessage) const
{
    if (attachments == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("附件输出缓冲区无效。");
        }
        return false;
    }

    attachments->clear();
    for (const QString &key : reimbursementAttachmentKeys()) {
        const QStringList references = splitAttachmentPaths(record.value(key));
        for (const QString &reference : references) {
            const QString resolvedPath = resolveAttachmentReference(storageRoot(), reference);
            QFile file(resolvedPath);
            if (!file.open(QIODevice::ReadOnly)) {
                if (errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("读取附件失败：%1").arg(file.errorString());
                }
                attachments->clear();
                return false;
            }

            attachments->append({reference, attachmentDisplayName(reference), file.readAll()});
        }
    }

    return true;
}

QString JsonStorageService::pageFilePath(const QString &pageId) const
{
    return QDir(storageRoot()).filePath(pageId + QStringLiteral(".json"));
}

QString JsonStorageService::usersFilePath() const
{
    return pageFilePath(QString::fromLatin1(kUsersPageId));
}

QString JsonStorageService::attachmentsRootDirectory() const
{
    return QDir(storageRoot()).filePath(QStringLiteral("attachments"));
}

QString JsonStorageService::recordAttachmentRelativeDirectory(const QString &pageId, const QString &recordId) const
{
    return QStringLiteral("attachments/%1/%2").arg(pageId, recordId);
}

QString JsonStorageService::recordAttachmentDirectory(const QString &pageId, const QString &recordId) const
{
    return QDir(storageRoot()).filePath(recordAttachmentRelativeDirectory(pageId, recordId));
}

QString JsonStorageService::inventoryHistoryFilePath() const
{
    return pageFilePath(QStringLiteral("inventory_history"));
}

bool JsonStorageService::ensureStorageReady(QString *errorMessage) const
{
    QDir dir;
    if (dir.mkpath(storageRoot())) {
        return true;
    }

    if (errorMessage != nullptr) {
        *errorMessage = QStringLiteral("无法创建本地数据目录。");
    }
    return false;
}

bool JsonStorageService::ensureUserStoreReady(QString *errorMessage) const
{
    if (!ensureStorageReady(errorMessage)) {
        return false;
    }

    QList<QVariantMap> users;
    if (!loadUsers(&users, errorMessage)) {
        return false;
    }

    const QString timestamp = nowString();
    bool changed = false;
    for (const QString &userName : builtInUserNames()) {
        if (indexOfUserByName(users, userName) >= 0) {
            continue;
        }

        QVariantMap userRecord;
        userRecord.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
        userRecord.insert(QStringLiteral("username"), userName);
        userRecord.insert(QStringLiteral("password"), QString::fromLatin1(kDefaultPassword));
        userRecord.insert(QStringLiteral("email"), QString());
        userRecord.insert(QStringLiteral("createdAt"), timestamp);
        userRecord.insert(QStringLiteral("updatedAt"), timestamp);
        users.append(userRecord);
        changed = true;
    }

    return !changed || saveUsers(users, errorMessage);
}

bool JsonStorageService::loadUsers(QList<QVariantMap> *users, QString *errorMessage) const
{
    if (users == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("用户列表输出参数无效。");
        }
        return false;
    }

    if (!ensureStorageReady(errorMessage)) {
        return false;
    }

    *users = loadPageRecords(QString::fromLatin1(kUsersPageId));
    return true;
}

bool JsonStorageService::saveUsers(const QList<QVariantMap> &users, QString *errorMessage) const
{
    return savePageRecords(QString::fromLatin1(kUsersPageId), users, errorMessage);
}

int JsonStorageService::indexOfUserByName(const QList<QVariantMap> &users, const QString &userName) const
{
    const QString trimmedUserName = userName.trimmed();
    for (int index = 0; index < users.size(); ++index) {
        if (users.at(index).value(QStringLiteral("username")).toString().trimmed() == trimmedUserName) {
            return index;
        }
    }
    return -1;
}

bool JsonStorageService::rateLimitAllowsSend(const QString &userName, QString *errorMessage) const
{
    const QDateTime now = QDateTime::currentDateTime();
    QList<QDateTime> history = m_verificationSendHistory.value(userName.trimmed());
    history.erase(std::remove_if(history.begin(), history.end(), [&now](const QDateTime &time) {
        return time.secsTo(now) > kVerificationSendWindowSeconds;
    }), history.end());
    m_verificationSendHistory.insert(userName.trimmed(), history);

    if (history.size() >= kVerificationSendLimit) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("验证码发送过于频繁，请 1 分钟后再试。");
        }
        return false;
    }
    return true;
}

QString JsonStorageService::verificationStateKey(const QString &userName, VerificationPurpose purpose) const
{
    return userName.trimmed() + QStringLiteral("|") + verificationPurposeKey(purpose);
}

bool JsonStorageService::archiveReimbursementAttachments(QVariantMap *record,
                                                        const QVariantMap &existingRecord,
                                                        QStringList *obsoleteFiles,
                                                        QStringList *createdFiles,
                                                        QString *errorMessage) const
{
    if (record == nullptr) {
        return false;
    }

    const QString recordId = record->value(QStringLiteral("id")).toString();
    const QString attachmentDirectory = recordAttachmentDirectory(QStringLiteral("reimbursement"), recordId);
    const QString attachmentRelativeDirectory = recordAttachmentRelativeDirectory(QStringLiteral("reimbursement"), recordId);
    QDir attachmentDir;
    if (!attachmentDir.mkpath(attachmentDirectory)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("无法创建报账附件归档目录。");
        }
        return false;
    }

    const QStringList keys = reimbursementAttachmentKeys();
    for (const QString &key : keys) {
        const QStringList incomingPaths = splitAttachmentPaths(record->value(key));
        const QStringList existingPaths = splitAttachmentPaths(existingRecord.value(key));
        QStringList archivedPaths;

        for (const QString &incomingPath : incomingPaths) {
            const QString resolvedIncomingPath = resolveAttachmentReference(storageRoot(), incomingPath);
            if (isPathUnderDirectory(resolvedIncomingPath, attachmentDirectory)) {
                archivedPaths.append(incomingPath);
                continue;
            }

            QFileInfo sourceInfo(resolvedIncomingPath);
            if (!sourceInfo.exists() || !sourceInfo.isFile()) {
                if (existingPaths.contains(incomingPath)) {
                    archivedPaths.append(incomingPath);
                    continue;
                }

                if (errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("附件不存在或不可读取：%1").arg(incomingPath);
                }
                return false;
            }

            const QString archivedFileName = QUuid::createUuid().toString(QUuid::WithoutBraces)
                                             + QStringLiteral("_")
                                             + sourceInfo.fileName();
            const QString archivedPath = QDir(attachmentDirectory).filePath(archivedFileName);
            if (!QFile::copy(sourceInfo.absoluteFilePath(), archivedPath)) {
                if (errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("无法归档附件：%1").arg(sourceInfo.absoluteFilePath());
                }
                return false;
            }

            archivedPaths.append(QDir::cleanPath(attachmentRelativeDirectory + QStringLiteral("/") + archivedFileName));
            if (createdFiles != nullptr) {
                createdFiles->append(QDir::cleanPath(archivedPath));
            }
        }

        QStringList filesToDelete;
        for (const QString &existingPath : existingPaths) {
            const QString resolvedExistingPath = resolveAttachmentReference(storageRoot(), existingPath);
            if (!archivedPaths.contains(existingPath)
                && isPathUnderDirectory(resolvedExistingPath, attachmentDirectory)) {
                filesToDelete.append(resolvedExistingPath);
            }
        }

        if (obsoleteFiles != nullptr) {
            obsoleteFiles->append(filesToDelete);
        }
        record->insert(key, archivedPaths.join(QStringLiteral("\n")));
    }

    return true;
}

bool JsonStorageService::savePageRecords(const QString &pageId,
                                         const QList<QVariantMap> &records,
                                         QString *errorMessage) const
{
    if (!ensureStorageReady(errorMessage)) {
        return false;
    }

    if (pageId != QString::fromLatin1(kUsersPageId) && !ensureUserStoreReady(errorMessage)) {
        return false;
    }

    QJsonArray array;
    for (const QVariantMap &record : records) {
        array.append(mapToJsonObject(record));
    }

    QFile file(pageFilePath(pageId));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage != nullptr) {
            *errorMessage = errorText(QStringLiteral("无法写入本地数据文件"), file);
        }
        return false;
    }

    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    return true;
}

bool JsonStorageService::saveInventoryHistory(const QList<QVariantMap> &records,
                                             QString *errorMessage) const
{
    return savePageRecords(QStringLiteral("inventory_history"), records, errorMessage);
}
