#pragma once

#include "appservice.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>

namespace TcpMessageCodec {

QByteArray encodeMessage(const QJsonObject &message);
bool tryTakeMessage(QByteArray *buffer, QJsonObject *message);

QJsonArray fieldDefinitionsToJson(const QList<FieldDefinition> &fields);
QList<FieldDefinition> fieldDefinitionsFromJson(const QJsonArray &array);

QJsonArray variantMapsToJson(const QList<QVariantMap> &records);
QList<QVariantMap> variantMapsFromJson(const QJsonArray &array);

QJsonArray fulfillmentResultsToJson(const QList<InventoryFulfillmentResult> &results);
QList<InventoryFulfillmentResult> fulfillmentResultsFromJson(const QJsonArray &array);

QJsonObject inventoryEnrichmentResultToJson(const InventoryEnrichmentResult &result);
InventoryEnrichmentResult inventoryEnrichmentResultFromJson(const QJsonObject &object);

}