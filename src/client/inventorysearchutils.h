#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>

int inventoryRecordSearchScore(const QVariantMap &record, const QString &keyword);
QList<QVariantMap> rankInventoryRecordsByKeyword(const QList<QVariantMap> &records, const QString &keyword);
