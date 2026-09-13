#include "tcpmessagecodec.h"

#include <QDataStream>
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

QJsonObject demandListItemToJson(const DemandListItem &item)
{
    return {
        {QStringLiteral("manufacturerPart"), item.manufacturerPart},
        {QStringLiteral("name"), item.name},
        {QStringLiteral("value"), item.value},
        {QStringLiteral("footprint"), item.footprint},
        {QStringLiteral("voltage"), item.voltage},
        {QStringLiteral("manufacturer"), item.manufacturer},
        {QStringLiteral("supplier"), item.supplier},
        {QStringLiteral("device"), item.device},
        {QStringLiteral("category"), item.category},
        {QStringLiteral("designator"), item.designator},
        {QStringLiteral("comment"), item.comment},
        {QStringLiteral("quantity"), item.quantity},
        {QStringLiteral("sourceRow"), item.sourceRow}
    };
}

DemandListItem demandListItemFromJson(const QJsonObject &object)
{
    DemandListItem item;
    item.manufacturerPart = object.value(QStringLiteral("manufacturerPart")).toString();
    item.name = object.value(QStringLiteral("name")).toString();
    item.value = object.value(QStringLiteral("value")).toString();
    item.footprint = object.value(QStringLiteral("footprint")).toString();
    item.voltage = object.value(QStringLiteral("voltage")).toString();
    item.manufacturer = object.value(QStringLiteral("manufacturer")).toString();
    item.supplier = object.value(QStringLiteral("supplier")).toString();
    item.device = object.value(QStringLiteral("device")).toString();
    item.category = object.value(QStringLiteral("category")).toString();
    item.designator = object.value(QStringLiteral("designator")).toString();
    item.comment = object.value(QStringLiteral("comment")).toString();
    item.quantity = object.value(QStringLiteral("quantity")).toInt();
    item.sourceRow = object.value(QStringLiteral("sourceRow")).toInt();
    return item;
}

QJsonObject matchCandidateToJson(const InventoryMatchCandidate &candidate)
{
    return {
        {QStringLiteral("itemId"), candidate.itemId},
        {QStringLiteral("manufacturerPart"), candidate.manufacturerPart},
        {QStringLiteral("manufacturer"), candidate.manufacturer},
        {QStringLiteral("supplier"), candidate.supplier},
        {QStringLiteral("name"), candidate.name},
        {QStringLiteral("value"), candidate.value},
        {QStringLiteral("footprint"), candidate.footprint},
        {QStringLiteral("voltage"), candidate.voltage},
        {QStringLiteral("uniqueId"), candidate.uniqueId},
        {QStringLiteral("unit"), candidate.unit},
        {QStringLiteral("location"), candidate.location},
        {QStringLiteral("availableQuantity"), candidate.availableQuantity},
        {QStringLiteral("score"), candidate.score},
        {QStringLiteral("matchedFields"), QJsonArray::fromStringList(candidate.matchedFields)}
    };
}

InventoryMatchCandidate matchCandidateFromJson(const QJsonObject &object)
{
    InventoryMatchCandidate candidate;
    candidate.itemId = object.value(QStringLiteral("itemId")).toString();
    candidate.manufacturerPart = object.value(QStringLiteral("manufacturerPart")).toString();
    candidate.manufacturer = object.value(QStringLiteral("manufacturer")).toString();
    candidate.supplier = object.value(QStringLiteral("supplier")).toString();
    candidate.name = object.value(QStringLiteral("name")).toString();
    candidate.value = object.value(QStringLiteral("value")).toString();
    candidate.footprint = object.value(QStringLiteral("footprint")).toString();
    candidate.voltage = object.value(QStringLiteral("voltage")).toString();
    candidate.uniqueId = object.value(QStringLiteral("uniqueId")).toString();
    candidate.unit = object.value(QStringLiteral("unit")).toString();
    candidate.location = object.value(QStringLiteral("location")).toString();
    candidate.availableQuantity = object.value(QStringLiteral("availableQuantity")).toInt();
    candidate.score = object.value(QStringLiteral("score")).toInt();
    const QJsonArray matchedFields = object.value(QStringLiteral("matchedFields")).toArray();
    for (const QJsonValue &value : matchedFields) {
        candidate.matchedFields.append(value.toString());
    }
    return candidate;
}

QJsonObject fulfillmentResultToJson(const InventoryFulfillmentResult &result)
{
    QJsonArray sourceRows;
    for (const int row : result.sourceRows) {
        sourceRows.append(row);
    }

    QJsonArray candidates;
    for (const InventoryMatchCandidate &candidate : result.candidates) {
        candidates.append(matchCandidateToJson(candidate));
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
        {QStringLiteral("requestManufacturerPart"), result.requestManufacturerPart},
        {QStringLiteral("requestName"), result.requestName},
        {QStringLiteral("requestValue"), result.requestValue},
        {QStringLiteral("requestFootprint"), result.requestFootprint},
        {QStringLiteral("requestVoltage"), result.requestVoltage},
        {QStringLiteral("requestManufacturer"), result.requestManufacturer},
        {QStringLiteral("requestSupplier"), result.requestSupplier},
        {QStringLiteral("requestDevice"), result.requestDevice},
        {QStringLiteral("requestCategory"), result.requestCategory},
        {QStringLiteral("requestDesignator"), result.requestDesignator},
        {QStringLiteral("requestComment"), result.requestComment},
        {QStringLiteral("sourceHeaders"), QJsonArray::fromStringList(result.sourceHeaders)},
        {QStringLiteral("sourceRowValues"), QJsonArray::fromStringList(result.sourceRowValues)},
        {QStringLiteral("requiredQuantity"), result.requiredQuantity},
        {QStringLiteral("availableQuantity"), result.availableQuantity},
        {QStringLiteral("matchScore"), result.matchScore},
        {QStringLiteral("confirmed"), result.confirmed},
        {QStringLiteral("matchedFields"), QJsonArray::fromStringList(result.matchedFields)},
        {QStringLiteral("candidates"), candidates},
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
    result.requestManufacturerPart = object.value(QStringLiteral("requestManufacturerPart")).toString();
    result.requestName = object.value(QStringLiteral("requestName")).toString();
    result.requestValue = object.value(QStringLiteral("requestValue")).toString();
    result.requestFootprint = object.value(QStringLiteral("requestFootprint")).toString();
    result.requestVoltage = object.value(QStringLiteral("requestVoltage")).toString();
    result.requestManufacturer = object.value(QStringLiteral("requestManufacturer")).toString();
    result.requestSupplier = object.value(QStringLiteral("requestSupplier")).toString();
    result.requestDevice = object.value(QStringLiteral("requestDevice")).toString();
    result.requestCategory = object.value(QStringLiteral("requestCategory")).toString();
    result.requestDesignator = object.value(QStringLiteral("requestDesignator")).toString();
    result.requestComment = object.value(QStringLiteral("requestComment")).toString();
    const QJsonArray sourceHeaders = object.value(QStringLiteral("sourceHeaders")).toArray();
    for (const QJsonValue &value : sourceHeaders) {
        result.sourceHeaders.append(value.toString());
    }
    const QJsonArray sourceRowValues = object.value(QStringLiteral("sourceRowValues")).toArray();
    for (const QJsonValue &value : sourceRowValues) {
        result.sourceRowValues.append(value.toString());
    }
    result.requiredQuantity = object.value(QStringLiteral("requiredQuantity")).toInt();
    result.availableQuantity = object.value(QStringLiteral("availableQuantity")).toInt();
    result.matchScore = object.value(QStringLiteral("matchScore")).toInt();
    result.confirmed = object.value(QStringLiteral("confirmed")).toBool();
    const QJsonArray matchedFields = object.value(QStringLiteral("matchedFields")).toArray();
    for (const QJsonValue &value : matchedFields) {
        result.matchedFields.append(value.toString());
    }
    const QJsonArray candidates = object.value(QStringLiteral("candidates")).toArray();
    for (const QJsonValue &value : candidates) {
        result.candidates.append(matchCandidateFromJson(value.toObject()));
    }
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

QJsonArray TcpMessageCodec::demandListItemsToJson(const QList<DemandListItem> &items)
{
    QJsonArray array;
    for (const DemandListItem &item : items) {
        array.append(demandListItemToJson(item));
    }
    return array;
}

QList<DemandListItem> TcpMessageCodec::demandListItemsFromJson(const QJsonArray &array)
{
    QList<DemandListItem> items;
    for (const QJsonValue &value : array) {
        items.append(demandListItemFromJson(value.toObject()));
    }
    return items;
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
