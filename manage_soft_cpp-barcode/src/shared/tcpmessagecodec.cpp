#include "tcpmessagecodec.h"

#include <QDataStream>
#include <QIODevice>
#include <QJsonDocument>

namespace {
QJsonObject fieldDefinitionToJson(const FieldDefinition &field)
{
    return {
        {QStringLiteral("key"), field.key},
        {QStringLiteral("label"), field.label},
        {QStringLiteral("type"), static_cast<int>(field.type)},
        {QStringLiteral("required"), field.required},
        {QStringLiteral("options"), QJsonArray::fromStringList(field.options)}
    };
}

FieldDefinition fieldDefinitionFromJson(const QJsonObject &object)
{
    FieldDefinition field;
    field.key = object.value(QStringLiteral("key")).toString();
    field.label = object.value(QStringLiteral("label")).toString();
    field.type = static_cast<FieldType>(object.value(QStringLiteral("type")).toInt());
    field.required = object.value(QStringLiteral("required")).toBool();
    const QJsonArray options = object.value(QStringLiteral("options")).toArray();
    for (const QJsonValue &value : options) {
        field.options.append(value.toString());
    }
    return field;
}

QJsonObject fulfillmentResultToJson(const InventoryFulfillmentResult &result)
{
    QJsonArray sourceRows;
    for (const int row : result.sourceRows) {
        sourceRows.append(row);
    }

    return {
        {QStringLiteral("itemId"), result.itemId},
        {QStringLiteral("manufacturerPart"), result.manufacturerPart},
        {QStringLiteral("manufacturer"), result.manufacturer},
        {QStringLiteral("name"), result.name},
        {QStringLiteral("uniqueId"), result.uniqueId},
        {QStringLiteral("unit"), result.unit},
        {QStringLiteral("location"), result.location},
        {QStringLiteral("sourceFile"), result.sourceFile},
        {QStringLiteral("sourceRows"), sourceRows},
        {QStringLiteral("requiredQuantity"), result.requiredQuantity},
        {QStringLiteral("availableQuantity"), result.availableQuantity},
        {QStringLiteral("status"), static_cast<int>(result.status)}
    };
}

InventoryFulfillmentResult fulfillmentResultFromJson(const QJsonObject &object)
{
    InventoryFulfillmentResult result;
    result.itemId = object.value(QStringLiteral("itemId")).toString();
    result.manufacturerPart = object.value(QStringLiteral("manufacturerPart")).toString();
    result.manufacturer = object.value(QStringLiteral("manufacturer")).toString();
    result.name = object.value(QStringLiteral("name")).toString();
    result.uniqueId = object.value(QStringLiteral("uniqueId")).toString();
    result.unit = object.value(QStringLiteral("unit")).toString();
    result.location = object.value(QStringLiteral("location")).toString();
    result.sourceFile = object.value(QStringLiteral("sourceFile")).toString();
    const QJsonArray sourceRows = object.value(QStringLiteral("sourceRows")).toArray();
    for (const QJsonValue &value : sourceRows) {
        result.sourceRows.append(value.toInt());
    }
    result.requiredQuantity = object.value(QStringLiteral("requiredQuantity")).toInt();
    result.availableQuantity = object.value(QStringLiteral("availableQuantity")).toInt();
    result.status = static_cast<InventoryFulfillmentStatus>(object.value(QStringLiteral("status")).toInt());
    return result;
}

QJsonObject inventoryEnrichmentFieldToJson(const InventoryEnrichmentField &field)
{
    return {
        {QStringLiteral("key"), field.key},
        {QStringLiteral("value"), field.value},
        {QStringLiteral("sourceTitle"), field.sourceTitle},
        {QStringLiteral("sourceUrl"), field.sourceUrl}
    };
}

InventoryEnrichmentField inventoryEnrichmentFieldFromJson(const QJsonObject &object)
{
    InventoryEnrichmentField field;
    field.key = object.value(QStringLiteral("key")).toString();
    field.value = object.value(QStringLiteral("value")).toString();
    field.sourceTitle = object.value(QStringLiteral("sourceTitle")).toString();
    field.sourceUrl = object.value(QStringLiteral("sourceUrl")).toString();
    return field;
}
}

QByteArray TcpMessageCodec::encodeMessage(const QJsonObject &message)
{
    const QByteArray body = QJsonDocument(message).toJson(QJsonDocument::Compact);
    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << static_cast<quint32>(body.size());
    packet.append(body);
    return packet;
}

bool TcpMessageCodec::tryTakeMessage(QByteArray *buffer, QJsonObject *message)
{
    if (buffer == nullptr || message == nullptr || buffer->size() < static_cast<int>(sizeof(quint32))) {
        return false;
    }

    QDataStream stream(*buffer);
    stream.setByteOrder(QDataStream::BigEndian);
    quint32 payloadSize = 0;
    stream >> payloadSize;
    const int totalSize = static_cast<int>(sizeof(quint32) + payloadSize);
    if (buffer->size() < totalSize) {
        return false;
    }

    const QByteArray payload = buffer->mid(static_cast<int>(sizeof(quint32)), static_cast<int>(payloadSize));
    *buffer = buffer->mid(totalSize);
    *message = QJsonDocument::fromJson(payload).object();
    return true;
}

QJsonArray TcpMessageCodec::fieldDefinitionsToJson(const QList<FieldDefinition> &fields)
{
    QJsonArray array;
    for (const FieldDefinition &field : fields) {
        array.append(fieldDefinitionToJson(field));
    }
    return array;
}

QList<FieldDefinition> TcpMessageCodec::fieldDefinitionsFromJson(const QJsonArray &array)
{
    QList<FieldDefinition> fields;
    for (const QJsonValue &value : array) {
        fields.append(fieldDefinitionFromJson(value.toObject()));
    }
    return fields;
}

QJsonArray TcpMessageCodec::variantMapsToJson(const QList<QVariantMap> &records)
{
    QJsonArray array;
    for (const QVariantMap &record : records) {
        array.append(QJsonObject::fromVariantMap(record));
    }
    return array;
}

QList<QVariantMap> TcpMessageCodec::variantMapsFromJson(const QJsonArray &array)
{
    QList<QVariantMap> records;
    for (const QJsonValue &value : array) {
        records.append(value.toObject().toVariantMap());
    }
    return records;
}

QJsonArray TcpMessageCodec::fulfillmentResultsToJson(const QList<InventoryFulfillmentResult> &results)
{
    QJsonArray array;
    for (const InventoryFulfillmentResult &result : results) {
        array.append(fulfillmentResultToJson(result));
    }
    return array;
}

QList<InventoryFulfillmentResult> TcpMessageCodec::fulfillmentResultsFromJson(const QJsonArray &array)
{
    QList<InventoryFulfillmentResult> results;
    for (const QJsonValue &value : array) {
        results.append(fulfillmentResultFromJson(value.toObject()));
    }
    return results;
}

QJsonObject TcpMessageCodec::inventoryEnrichmentResultToJson(const InventoryEnrichmentResult &result)
{
    QJsonArray fields;
    for (const InventoryEnrichmentField &field : result.fields) {
        fields.append(inventoryEnrichmentFieldToJson(field));
    }

    return {
        {QStringLiteral("manufacturerPart"), result.manufacturerPart},
        {QStringLiteral("provider"), result.provider},
        {QStringLiteral("fields"), fields}
    };
}

InventoryEnrichmentResult TcpMessageCodec::inventoryEnrichmentResultFromJson(const QJsonObject &object)
{
    InventoryEnrichmentResult result;
    result.manufacturerPart = object.value(QStringLiteral("manufacturerPart")).toString();
    result.provider = object.value(QStringLiteral("provider")).toString();
    const QJsonArray fields = object.value(QStringLiteral("fields")).toArray();
    for (const QJsonValue &value : fields) {
        result.fields.append(inventoryEnrichmentFieldFromJson(value.toObject()));
    }
    return result;
}
