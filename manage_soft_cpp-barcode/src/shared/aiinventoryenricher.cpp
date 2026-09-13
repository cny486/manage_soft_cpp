#include "aiinventoryenricher.h"

#include "aiapisettings.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QTimer>
#include <QUrl>

namespace {
QStringList enrichableFieldKeys()
{
    return {
        QStringLiteral("manufacturer"),
        QStringLiteral("name"),
        QStringLiteral("category"),
        QStringLiteral("device"),
        QStringLiteral("footprint"),
        QStringLiteral("value"),
        QStringLiteral("supplier"),
        QStringLiteral("unit"),
        QStringLiteral("pinCount"),
        QStringLiteral("currentRating"),
        QStringLiteral("currentRatingMax"),
        QStringLiteral("dcResistanceDcr"),
        QStringLiteral("equivalentSeriesResistanceEsr"),
        QStringLiteral("gateChargeQg"),
        QStringLiteral("gateThresholdVoltageVgsTh"),
        QStringLiteral("overloadVoltageMax"),
        QStringLiteral("comment")
    };
}

QStringList effectiveFieldKeys(const QStringList &desiredFieldKeys)
{
    if (desiredFieldKeys.isEmpty()) {
        return enrichableFieldKeys();
    }

    QStringList filteredKeys;
    const QStringList availableFieldKeys = enrichableFieldKeys();
    for (const QString &fieldKey : desiredFieldKeys) {
        if (availableFieldKeys.contains(fieldKey) && !filteredKeys.contains(fieldKey)) {
            filteredKeys.append(fieldKey);
        }
    }
    return filteredKeys;
}

QSet<QString> enrichableFieldKeySet()
{
    const QStringList fieldKeys = enrichableFieldKeys();
    return QSet<QString>(fieldKeys.begin(), fieldKeys.end());
}

QString currentRecordSummary(const QVariantMap &currentRecord)
{
    QStringList lines;
    for (auto it = currentRecord.constBegin(); it != currentRecord.constEnd(); ++it) {
        const QString value = it.value().toString().trimmed();
        if (!value.isEmpty()) {
            lines.append(QStringLiteral("- %1: %2").arg(it.key(), value));
        }
    }

    if (lines.isEmpty()) {
        return QStringLiteral("(empty)");
    }
    return lines.join(QStringLiteral("\n"));
}

QString allowedFieldsSummary(const QStringList &fieldKeys)
{
    return fieldKeys.join(QStringLiteral(", "));
}

QString extractJsonObjectText(const QString &text)
{
    const QString trimmed = text.trimmed();
    const int fenceIndex = trimmed.indexOf(QStringLiteral("```"));
    QString candidate = trimmed;
    if (fenceIndex >= 0) {
        const int firstBrace = trimmed.indexOf(QChar('{'));
        const int lastBrace = trimmed.lastIndexOf(QChar('}'));
        if (firstBrace >= 0 && lastBrace > firstBrace) {
            candidate = trimmed.mid(firstBrace, lastBrace - firstBrace + 1);
        }
    }

    const int start = candidate.indexOf(QChar('{'));
    if (start < 0) {
        return QString();
    }

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (int index = start; index < candidate.size(); ++index) {
        const QChar ch = candidate.at(index);
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == QChar('\\')) {
            escaped = true;
            continue;
        }
        if (ch == QChar('"')) {
            inString = !inString;
            continue;
        }
        if (inString) {
            continue;
        }
        if (ch == QChar('{')) {
            ++depth;
        } else if (ch == QChar('}')) {
            --depth;
            if (depth == 0) {
                return candidate.mid(start, index - start + 1);
            }
        }
    }

    return QString();
}

QString chatContentFromResponse(const QJsonObject &response)
{
    const QJsonArray choices = response.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        return QString();
    }

    const QJsonObject message = choices.first().toObject().value(QStringLiteral("message")).toObject();
    const QJsonValue contentValue = message.value(QStringLiteral("content"));
    if (contentValue.isString()) {
        return contentValue.toString();
    }

    if (contentValue.isArray()) {
        QStringList parts;
        for (const QJsonValue &partValue : contentValue.toArray()) {
            const QJsonObject partObject = partValue.toObject();
            const QString text = partObject.value(QStringLiteral("text")).toString().trimmed();
            if (!text.isEmpty()) {
                parts.append(text);
            }
        }
        return parts.join(QStringLiteral("\n"));
    }

    return QString();
}

bool postChatRequest(const AiApiSettings &settings,
                     const QString &systemPrompt,
                     const QString &userPrompt,
                     QString *content,
                     QString *errorMessage)
{
    if (settings.apiUrl.isEmpty() || settings.model.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("未配置 AI 补齐接口。请在连接设置中填写 AI API 地址和模型，或设置 MANAGE_SOFT_AI_API_URL / MANAGE_SOFT_AI_MODEL 环境变量。");
        }
        return false;
    }

    QJsonObject requestBody = {
        {QStringLiteral("model"), settings.model},
        {QStringLiteral("temperature"), 0.2},
        {QStringLiteral("messages"), QJsonArray{
             QJsonObject{{QStringLiteral("role"), QStringLiteral("system")}, {QStringLiteral("content"), systemPrompt}},
             QJsonObject{{QStringLiteral("role"), QStringLiteral("user")}, {QStringLiteral("content"), userPrompt}}
         }}
    };

    QNetworkRequest request(QUrl(settings.apiUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!settings.apiKey.isEmpty()) {
        request.setRawHeader("Authorization", QByteArray("Bearer ") + settings.apiKey.toUtf8());
    }

    QNetworkAccessManager manager;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QNetworkReply *reply = manager.post(request, QJsonDocument(requestBody).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(settings.timeoutMs);
    loop.exec();

    if (timer.isActive()) {
        timer.stop();
    } else {
        reply->abort();
        reply->deleteLater();
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("AI 补齐请求超时。请检查接口响应时间或调大 MANAGE_SOFT_AI_TIMEOUT_MS。");
        }
        return false;
    }

    const QByteArray responseBytes = reply->readAll();
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    reply->deleteLater();
    if (networkError != QNetworkReply::NoError) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("AI 补齐请求失败：%1").arg(networkErrorText);
        }
        return false;
    }

    QJsonParseError responseParseError;
    const QJsonDocument responseDoc = QJsonDocument::fromJson(responseBytes, &responseParseError);
    if (responseParseError.error != QJsonParseError::NoError || !responseDoc.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("AI 接口返回了无法解析的响应。");
        }
        return false;
    }

    const QString responseContent = chatContentFromResponse(responseDoc.object()).trimmed();
    if (responseContent.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("AI 接口返回成功，但没有可读取的内容。");
        }
        return false;
    }

    if (content != nullptr) {
        *content = responseContent;
    }
    return true;
}

bool addLocalRecordMatch(const QString &manufacturerPart,
                         const QList<QVariantMap> &inventoryRecords,
                         const QStringList &desiredFieldKeys,
                         InventoryEnrichmentResult *result)
{
    if (result == nullptr) {
        return false;
    }

    const QStringList fieldKeys = effectiveFieldKeys(desiredFieldKeys);
    const QSet<QString> allowedKeys(fieldKeys.begin(), fieldKeys.end());
    for (const QVariantMap &record : inventoryRecords) {
        if (record.value(QStringLiteral("manufacturerPart")).toString().trimmed().compare(manufacturerPart.trimmed(), Qt::CaseInsensitive) != 0) {
            continue;
        }

        result->manufacturerPart = manufacturerPart.trimmed();
        result->provider = QStringLiteral("local-inventory");
        const QString recordId = record.value(QStringLiteral("id")).toString().trimmed();
        const QString sourceUrl = recordId.isEmpty()
                                      ? QStringLiteral("local://inventory")
                                      : QStringLiteral("local://inventory/%1").arg(recordId);
        for (auto it = record.constBegin(); it != record.constEnd(); ++it) {
            if (!allowedKeys.contains(it.key())) {
                continue;
            }

            const QString value = it.value().toString().trimmed();
            if (value.isEmpty()) {
                continue;
            }

            result->fields.append({it.key(),
                                   value,
                                   QStringLiteral("本地库存记录"),
                                   sourceUrl});
        }
        return !result->fields.isEmpty();
    }

    return false;
}

bool callAiApi(const AiApiSettings &settings,
               const QString &manufacturerPart,
               const QVariantMap &currentRecord,
               const QStringList &desiredFieldKeys,
               InventoryEnrichmentResult *result,
               QString *errorMessage)
{
    if (result == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("内部错误：结果对象为空。");
        }
        return false;
    }

    const QStringList fieldKeys = effectiveFieldKeys(desiredFieldKeys);
    if (fieldKeys.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("当前没有可供 AI 补齐的目标字段。");
        }
        return false;
    }

    const QString systemPrompt = QStringLiteral(
        "You are an electronics component enrichment agent. "
        "Return ONLY a JSON object. "
        "For each filled field, include key, value, sourceTitle, and sourceUrl. "
        "Every field must have a sourceTitle and sourceUrl. "
        "If a field cannot be supported by a concrete source, omit that field entirely. "
        "Only use these field keys: %1. "
        "Do not fabricate unavailable information. "
        "Use concise normalized values.").arg(allowedFieldsSummary(fieldKeys));

    const QString userPrompt = QStringLiteral(
        "Manufacturer Part: %1\n"
        "Current record:\n%2\n\n"
        "Return JSON in this shape:\n"
        "{\n"
        "  \"manufacturerPart\": \"...\",\n"
        "  \"provider\": \"...\",\n"
        "  \"fields\": [\n"
        "    {\"key\": \"manufacturer\", \"value\": \"...\", \"sourceTitle\": \"...\", \"sourceUrl\": \"https://...\"}\n"
        "  ]\n"
        "}\n")
                                   .arg(manufacturerPart.trimmed(), currentRecordSummary(currentRecord));
    QString content;
    if (!postChatRequest(settings, systemPrompt, userPrompt, &content, errorMessage)) {
        return false;
    }
    const QString jsonText = extractJsonObjectText(content);
    if (jsonText.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("AI 未返回结构化 JSON 内容。");
        }
        return false;
    }

    QJsonParseError parsedError;
    const QJsonDocument parsedDoc = QJsonDocument::fromJson(jsonText.toUtf8(), &parsedError);
    if (parsedError.error != QJsonParseError::NoError || !parsedDoc.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("AI 返回的 JSON 无法解析。");
        }
        return false;
    }

    const QJsonObject root = parsedDoc.object();
    result->manufacturerPart = root.value(QStringLiteral("manufacturerPart")).toString().trimmed();
    result->provider = root.value(QStringLiteral("provider")).toString().trimmed();
    if (result->manufacturerPart.isEmpty()) {
        result->manufacturerPart = manufacturerPart.trimmed();
    }
    if (result->provider.isEmpty()) {
        result->provider = QStringLiteral("ai-api");
    }

    const QSet<QString> allowedKeys(fieldKeys.begin(), fieldKeys.end());
    QSet<QString> seenKeys;
    const QJsonArray fields = root.value(QStringLiteral("fields")).toArray();
    for (const QJsonValue &value : fields) {
        const QJsonObject fieldObject = value.toObject();
        const QString key = fieldObject.value(QStringLiteral("key")).toString().trimmed();
        const QString fieldValue = fieldObject.value(QStringLiteral("value")).toString().trimmed();
        const QString sourceTitle = fieldObject.value(QStringLiteral("sourceTitle")).toString().trimmed();
        const QString sourceUrl = fieldObject.value(QStringLiteral("sourceUrl")).toString().trimmed();
        if (!allowedKeys.contains(key) || fieldValue.isEmpty() || sourceTitle.isEmpty() || sourceUrl.isEmpty() || seenKeys.contains(key)) {
            continue;
        }

        result->fields.append({key, fieldValue, sourceTitle, sourceUrl});
        seenKeys.insert(key);
    }

    if (result->fields.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("AI 未返回可用的字段信息，或返回内容缺少来源。\n每条字段必须带 sourceTitle 和 sourceUrl。");
        }
        return false;
    }

    return true;
}
}

bool AiInventoryEnricher::testConnection(const AiApiSettings &settings,
                                         QString *responsePreview,
                                         QString *errorMessage)
{
    const QString systemPrompt = QStringLiteral(
        "You are a connectivity check endpoint. "
        "Reply with a short plain text confirmation that includes the word OK.");
    const QString userPrompt = QStringLiteral(
        "Return a single short sentence confirming this AI API is reachable for ManageSoftCpp.");

    QString content;
    if (!postChatRequest(settings, systemPrompt, userPrompt, &content, errorMessage)) {
        return false;
    }

    if (!content.contains(QStringLiteral("ok"), Qt::CaseInsensitive)) {
        if (responsePreview != nullptr) {
            *responsePreview = content.left(160).trimmed();
        }
        return true;
    }

    if (responsePreview != nullptr) {
        *responsePreview = content.left(160).trimmed();
    }
    return true;
}

bool AiInventoryEnricher::enrich(const QString &manufacturerPart,
                                 const QVariantMap &currentRecord,
                                 const QList<QVariantMap> &inventoryRecords,
                                 const QStringList &desiredFieldKeys,
                                 InventoryEnrichmentResult *result,
                                 QString *errorMessage)
{
    if (manufacturerPart.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Manufacturer Part 不能为空。");
        }
        return false;
    }

    if (result == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("内部错误：结果对象为空。");
        }
        return false;
    }

    result->manufacturerPart = manufacturerPart.trimmed();
    result->provider.clear();
    result->fields.clear();

    if (addLocalRecordMatch(manufacturerPart, inventoryRecords, desiredFieldKeys, result)) {
        return true;
    }

    return callAiApi(loadAiApiSettings(), manufacturerPart, currentRecord, desiredFieldKeys, result, errorMessage);
}