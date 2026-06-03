#include "datainitializer.h"

#include "appservice.h"

void ensureInventorySampleData(AppService *service)
{
    if (service == nullptr) {
        return;
    }

    if (!service->loadPageRecords(QStringLiteral("inventory")).isEmpty()) {
        return;
    }

    const QList<QVariantMap> sampleRecords = {
        {
            {QStringLiteral("number"), QStringLiteral("1")},
            {QStringLiteral("quantity"), 120},
            {QStringLiteral("date"), QStringLiteral("2026-05-10")},
            {QStringLiteral("unit"), QStringLiteral("个")},
            {QStringLiteral("location"), QStringLiteral("A区-01架-02层")},
            {QStringLiteral("comment"), QStringLiteral("主控板电源滤波电容")},
            {QStringLiteral("designator"), QStringLiteral("C12,C13,C18")},
            {QStringLiteral("footprint"), QStringLiteral("0805")},
            {QStringLiteral("value"), QStringLiteral("10uF")},
            {QStringLiteral("manufacturerPart"), QStringLiteral("CL21A106KAYNNNE")},
            {QStringLiteral("manufacturer"), QStringLiteral("Samsung Electro-Mechanics")},
            {QStringLiteral("addIntoBom"), QStringLiteral("Yes")},
            {QStringLiteral("convertToPcb"), QStringLiteral("Yes")},
            {QStringLiteral("pinCount"), 2},
            {QStringLiteral("category"), QStringLiteral("Capacitor")},
            {QStringLiteral("device"), QStringLiteral("MLCC")},
            {QStringLiteral("name"), QStringLiteral("去耦电容")},
            {QStringLiteral("uniqueId"), QStringLiteral("CAP-0805-10UF-001")},
            {QStringLiteral("minStock"), 40},
            {QStringLiteral("batchNo"), QStringLiteral("202605-A")},
            {QStringLiteral("supplier"), QStringLiteral("立创商城")},
            {QStringLiteral("currentRating"), QStringLiteral("-")},
            {QStringLiteral("currentRatingMax"), QStringLiteral("-")},
            {QStringLiteral("dcResistanceDcr"), QStringLiteral("-")},
            {QStringLiteral("equivalentSeriesResistanceEsr"), QStringLiteral("35mOhm")},
            {QStringLiteral("gateChargeQg"), QStringLiteral("-")},
            {QStringLiteral("gateThresholdVoltageVgsTh"), QStringLiteral("-")},
            {QStringLiteral("overloadVoltageMax"), QStringLiteral("25V")}
        },
        {
            {QStringLiteral("number"), QStringLiteral("2")},
            {QStringLiteral("quantity"), 36},
            {QStringLiteral("date"), QStringLiteral("2026-05-08")},
            {QStringLiteral("unit"), QStringLiteral("个")},
            {QStringLiteral("location"), QStringLiteral("A区-02架-01层")},
            {QStringLiteral("comment"), QStringLiteral("接口保护 TVS 二极管")},
            {QStringLiteral("designator"), QStringLiteral("D5,D6")},
            {QStringLiteral("footprint"), QStringLiteral("SOD-323")},
            {QStringLiteral("value"), QStringLiteral("ESD5V")},
            {QStringLiteral("manufacturerPart"), QStringLiteral("PESD5V0S1BA,115")},
            {QStringLiteral("manufacturer"), QStringLiteral("Nexperia")},
            {QStringLiteral("addIntoBom"), QStringLiteral("Yes")},
            {QStringLiteral("convertToPcb"), QStringLiteral("Yes")},
            {QStringLiteral("pinCount"), 2},
            {QStringLiteral("category"), QStringLiteral("Diode")},
            {QStringLiteral("device"), QStringLiteral("TVS")},
            {QStringLiteral("name"), QStringLiteral("ESD 保护二极管")},
            {QStringLiteral("uniqueId"), QStringLiteral("DIO-ESD5V-002")},
            {QStringLiteral("minStock"), 20},
            {QStringLiteral("batchNo"), QStringLiteral("202604-C")},
            {QStringLiteral("supplier"), QStringLiteral("贸泽")},
            {QStringLiteral("currentRating"), QStringLiteral("5A")},
            {QStringLiteral("currentRatingMax"), QStringLiteral("8A")},
            {QStringLiteral("dcResistanceDcr"), QStringLiteral("-")},
            {QStringLiteral("equivalentSeriesResistanceEsr"), QStringLiteral("-")},
            {QStringLiteral("gateChargeQg"), QStringLiteral("-")},
            {QStringLiteral("gateThresholdVoltageVgsTh"), QStringLiteral("-")},
            {QStringLiteral("overloadVoltageMax"), QStringLiteral("5V")}
        },
        {
            {QStringLiteral("number"), QStringLiteral("3")},
            {QStringLiteral("quantity"), 18},
            {QStringLiteral("date"), QStringLiteral("2026-05-12")},
            {QStringLiteral("unit"), QStringLiteral("个")},
            {QStringLiteral("location"), QStringLiteral("B区-01架-03层")},
            {QStringLiteral("comment"), QStringLiteral("电源开关 MOSFET")},
            {QStringLiteral("designator"), QStringLiteral("Q1")},
            {QStringLiteral("footprint"), QStringLiteral("SOT-23")},
            {QStringLiteral("value"), QStringLiteral("N-MOS")},
            {QStringLiteral("manufacturerPart"), QStringLiteral("AO3400A")},
            {QStringLiteral("manufacturer"), QStringLiteral("Alpha & Omega")},
            {QStringLiteral("addIntoBom"), QStringLiteral("Yes")},
            {QStringLiteral("convertToPcb"), QStringLiteral("Yes")},
            {QStringLiteral("pinCount"), 3},
            {QStringLiteral("category"), QStringLiteral("Transistor")},
            {QStringLiteral("device"), QStringLiteral("MOSFET")},
            {QStringLiteral("name"), QStringLiteral("电源开关管")},
            {QStringLiteral("uniqueId"), QStringLiteral("MOS-AO3400A-003")},
            {QStringLiteral("minStock"), 10},
            {QStringLiteral("batchNo"), QStringLiteral("202605-B")},
            {QStringLiteral("supplier"), QStringLiteral("得捷")},
            {QStringLiteral("currentRating"), QStringLiteral("5.8A")},
            {QStringLiteral("currentRatingMax"), QStringLiteral("6.9A")},
            {QStringLiteral("dcResistanceDcr"), QStringLiteral("48mOhm")},
            {QStringLiteral("equivalentSeriesResistanceEsr"), QStringLiteral("-")},
            {QStringLiteral("gateChargeQg"), QStringLiteral("7nC")},
            {QStringLiteral("gateThresholdVoltageVgsTh"), QStringLiteral("1.4V")},
            {QStringLiteral("overloadVoltageMax"), QStringLiteral("30V")}
        }
    };

    for (const QVariantMap &record : sampleRecords) {
        service->upsertRecord(QStringLiteral("inventory"), record);
    }
}