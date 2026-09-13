#pragma once

#include "aiapisettings.h"
#include "appservice.h"

#include <QVariantMap>

class AiInventoryEnricher {
public:
    static bool isSafeSourceUrl(const QString &url);

    static bool testConnection(const AiApiSettings &settings,
                               QString *responsePreview = nullptr,
                               QString *errorMessage = nullptr);

    static bool enrich(const QString &manufacturerPart,
                       const QVariantMap &currentRecord,
                       const QList<QVariantMap> &inventoryRecords,
                       const QStringList &desiredFieldKeys,
                       InventoryEnrichmentResult *result,
                       QString *errorMessage = nullptr);
};
